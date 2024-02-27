
typedef struct SchedNode SchedNode;
struct SchedNode {
    SchedNode* parent;

    TB_Node* n;
    int index;

    int anti_i;
    int anti_count;
    User* antis[];
};

typedef struct {
    TB_Node* phi;
    TB_Node* n;
} SchedPhi;

static SchedNode* sched_make_node(TB_Arena* arena, SchedNode* parent, TB_Node* n) {
    int anti_count = 0;
    if (n->type == TB_MERGEMEM) {
        anti_count = n->input_count - 2;
    } else if (is_mem_out_op(n) && n->type != TB_PHI && n->type != TB_PROJ) {
        anti_count = 1;
    }

    SchedNode* s = tb_arena_alloc(arena, sizeof(SchedNode) + anti_count*sizeof(User*));
    *s = (SchedNode){ .parent = parent, .n = n, .index = 0, .anti_count = anti_count };

    if (n->type == TB_MERGEMEM) {
        FOREACH_N(i, 2, n->input_count) {
            s->antis[i - 2] = n->inputs[i]->users;
        }
    } else if (anti_count == 1) {
        s->antis[0] = n->inputs[1]->users;
    }

    return s;
}

static bool sched_in_bb(TB_Passes* passes, Worklist* ws, TB_BasicBlock* bb, TB_Node* n) {
    return passes->scheduled[n->gvn] == bb && !worklist_test_n_set(ws, n);
}

typedef struct {
    int cap, count;
    SchedPhi* arr;
} Phis;

static void fill_phis(TB_Arena* arena, Phis* phis, TB_Node* succ, int phi_i) {
    for (User* u = succ->users; u; u = u->next) {
        if (u->n->type != TB_PHI) continue;

        // ensure cap (not very effective since it moves each time, that's ok it's rare)
        if (phis->count == phis->cap) {
            phis->cap *= 2;

            SchedPhi* new_phis = tb_arena_alloc(arena, phis->cap * sizeof(SchedPhi));
            memcpy(new_phis, phis->arr, phis->count * sizeof(SchedPhi));
            phis->arr = new_phis;
        }

        phis->arr[phis->count++] = (SchedPhi){ .phi = u->n, .n = u->n->inputs[1 + phi_i] };
    }
}

// basically just topological sort, no fancy shit
void greedy_scheduler(TB_Passes* passes, TB_CFG* cfg, Worklist* ws, DynArray(PhiVal)* phi_vals, TB_BasicBlock* bb, TB_Node* end) {
    TB_Arena* arena = tmp_arena;
    TB_ArenaSavepoint sp = tb_arena_save(arena);

    // find phis
    int phi_curr = 0;
    Phis phis = { .cap = 256, .arr = tb_arena_alloc(arena, 256 * sizeof(SchedPhi)) };

    if (end->type == TB_BRANCH) {
        for (User* u = end->users; u; u = u->next) {
            if (u->n->type != TB_PROJ) continue;

            // we might have some memory phis over here if the projections aren't bbs
            ptrdiff_t search = nl_map_get(cfg->node_to_block, u->n);
            if (search >= 0) continue;

            User* succ = cfg_next_user(end);
            if (cfg_is_region(succ->n)) {
                fill_phis(arena, &phis, succ->n, succ->slot);
            }
        }
    } else if (!cfg_is_endpoint(end)) {
        User* succ = cfg_next_user(end);
        if (cfg_is_region(succ->n)) {
            fill_phis(arena, &phis, succ->n, succ->slot);
        }
    }

    SchedNode* top = sched_make_node(arena, NULL, end);
    worklist_test_n_set(ws, end);

    // reserve projections for the top
    TB_Node* start = bb->id == 0 ? passes->f->root_node : NULL;
    if (start) {
        FOR_USERS(use, start) {
            if (use->n->type == TB_PROJ && !worklist_test_n_set(ws, use->n)) {
                dyn_array_put(ws->items, use->n);
            }
        }
    }

    size_t leftovers = 0;
    size_t leftover_count = 1ull << bb->items.exp;

    while (top != NULL) retry: {
        TB_Node* n = top->n;

        // resolve inputs first
        if (n->type != TB_PHI && top->index < n->input_count) {
            TB_Node* in = n->inputs[top->index++];
            if (in != NULL && sched_in_bb(passes, ws, bb, in)) {
                top = sched_make_node(arena, top, in);
            }
            continue;
        }

        // resolve anti-deps
        if (top->anti_i < top->anti_count) {
            if (top->antis[top->anti_i] != NULL) {
                User* next = top->antis[top->anti_i]->next;
                TB_Node* anti = top->antis[top->anti_i]->n;
                int slot = top->antis[top->anti_i]->slot;

                if (anti != n && slot == 1 && sched_in_bb(passes, ws, bb, anti)) {
                    top = sched_make_node(arena, top, anti);
                }

                top->antis[top->anti_i] = next;
                if (next == NULL) { top->anti_i++; }
                continue;
            }
        }

        // resolve phi edges & leftovers when we're at the endpoint
        if (end == n) {
            // skip non-phis
            if (phi_curr < phis.count) {
                TB_Node* phi = phis.arr[phi_curr].phi;
                TB_Node* val = phis.arr[phi_curr].n;
                phi_curr += 1;

                // reserve PHI space
                if (phi_vals && val->dt.type != TB_MEMORY) {
                    PhiVal p;
                    p.phi = phi;
                    p.n   = val;
                    p.dst = -1;
                    p.src = -1;
                    dyn_array_put(*phi_vals, p);
                }

                if (sched_in_bb(passes, ws, bb, val)) {
                    top = sched_make_node(arena, top, val);
                }
                continue;
            }

            // resolve leftover nodes placed here by GCM
            while (leftovers < leftover_count && (bb->items.data[leftovers] == NULL || bb->items.data[leftovers] == NL_HASHSET_TOMB)) {
                leftovers++;
            }

            if (leftovers < leftover_count) {
                if (!worklist_test_n_set(ws, bb->items.data[leftovers])) {
                    top = sched_make_node(arena, top, bb->items.data[leftovers]);
                }
                leftovers += 1;
                continue;
            }
        }

        dyn_array_put(ws->items, n);
        top = top->parent;

        // push outputs (projections, if they apply)
        if (n->dt.type == TB_TUPLE && n->type != TB_BRANCH && n->type != TB_ROOT) {
            for (User* use = n->users; use; use = use->next) {
                if (use->n->type == TB_PROJ && !worklist_test_n_set(ws, use->n)) {
                    dyn_array_put(ws->items, use->n);
                }
            }
        }
    }

    tb_arena_restore(arena, sp);
}
