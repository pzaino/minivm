#include "../jump.h"
#include "../opcode.h"
#include "../vm.h"
#include "int.h"

#define vm_int_read_at(type_, index_) (*(type_ *)&ops[index_])
#define vm_int_read_type(type_) ({type_ ret=*(type_*)&ops[index]; index += sizeof(type_); ret; })
#define vm_int_read_reg() ({ vm_int_read_type(vm_reg_t); })
#define vm_int_read_int() ({ vm_int_read_type(vm_int_t); })
#define vm_int_read_load() ({ regs[vm_int_read_reg()]; })
#define vm_int_read_store() ({ &regs[vm_int_read_reg()]; })
#define vm_int_read_value() ({ vm_int_read_type(vm_value_t); })
#define vm_int_read_loc() ({ vm_int_read_type(vm_loc_t); })
#define vm_int_jump_next() ({ goto *vm_int_read_type(void *); })

#if VM_CONFIG_GROW_STACK
#define vm_int_check_stack() ({                                            \
  size_t nstack = stack - stack_base;                                      \
  if (nstack + 4 >= nframes)                                               \
  {                                                                        \
    stack_base = vm_realloc(stack_base, sizeof(size_t) * (nstack + 4));    \
    stack = stack_base + nstack;                                           \
    nframes = nstack;                                                      \
  }                                                                        \
  size_t nregs = regs - regs_base;                                         \
  if (nregs + 256 > nregs0)                                                \
  {                                                                        \
    regs_base = vm_realloc(regs_base, sizeof(vm_value_t) * (nregs * 4 + 256)); \
    regs = regs_base + nregs;                                              \
    nregs0 = nregs * 4 + 256;                                                        \
    gc->regs = regs_base;                                                  \
    gc->nregs = nregs0;                                                    \
  }                                                                        \
})
#else
#define vm_int_check_stack() ({})
#endif
#define vm_int_jump_next_check() ({vm_int_check_stack(); vm_int_jump_next(); })

#pragma GCC optimize "no-ssa-phiopt"
int vm_int_run(size_t nops, const vm_opcode_t *iops, vm_gc_t *restrict gc)
{
  void *ptrs[] = {
      [VM_INT_OP_EXIT] = &&exec_exit,
      [VM_INT_OP_PUTC] = &&exec_putc,
      [VM_INT_OP_MOV] = &&exec_mov,
      [VM_INT_OP_MOVC] = &&exec_movc,
      [VM_INT_OP_MOVF] = &&exec_movf,
      [VM_INT_OP_ADD] = &&exec_add,
      [VM_INT_OP_SUB] = &&exec_sub,
      [VM_INT_OP_RET] = &&exec_ret,
      [VM_INT_OP_BEQ] = &&exec_beq,
      [VM_INT_OP_BEQC] = &&exec_beqc,
      [VM_INT_OP_CALL0] = &&exec_call0,
      [VM_INT_OP_CALL1] = &&exec_call1,
      [VM_INT_OP_CALL2] = &&exec_call2,
      [VM_INT_OP_CALL3] = &&exec_call3,
      [VM_INT_OP_CALL4] = &&exec_call4,
      [VM_INT_OP_CALL5] = &&exec_call5,
      [VM_INT_OP_CALL6] = &&exec_call6,
      [VM_INT_OP_CALL7] = &&exec_call7,
      [VM_INT_OP_CALL8] = &&exec_call8,
      [VM_INT_OP_DCALL0] = &&exec_dcall0,
      [VM_INT_OP_DCALL1] = &&exec_dcall1,
      [VM_INT_OP_DCALL2] = &&exec_dcall2,
      [VM_INT_OP_DCALL3] = &&exec_dcall3,
      [VM_INT_OP_DCALL4] = &&exec_dcall4,
      [VM_INT_OP_DCALL5] = &&exec_dcall5,
      [VM_INT_OP_DCALL6] = &&exec_dcall6,
      [VM_INT_OP_DCALL7] = &&exec_dcall7,
      [VM_INT_OP_DCALL8] = &&exec_dcall8,
      [VM_INT_OP_JUMP] = &&exec_jump,
      [VM_INT_OP_DJUMP] = &&exec_djump,
      [VM_INT_OP_ADDC] = &&exec_addc,
      [VM_INT_OP_SUBC] = &&exec_subc,
      [VM_INT_OP_CSUB] = &&exec_csub,
      [VM_INT_OP_RETC] = &&exec_retc,
      [VM_INT_OP_MUL] = &&exec_mul,
      [VM_INT_OP_MULC] = &&exec_mulc,
      [VM_INT_OP_DIV] = &&exec_div,
      [VM_INT_OP_DIVC] = &&exec_divc,
      [VM_INT_OP_CDIV] = &&exec_cdiv,
      [VM_INT_OP_MOD] = &&exec_mod,
      [VM_INT_OP_MODC] = &&exec_modc,
      [VM_INT_OP_CMOD] = &&exec_cmod,
      [VM_INT_OP_BB] = &&exec_bb,
      [VM_INT_OP_BLT] = &&exec_blt,
      [VM_INT_OP_BLTC] = &&exec_bltc,
      [VM_INT_OP_CBLT] = &&exec_cblt,
      [VM_INT_OP_ARR] = &&exec_arr,
      [VM_INT_OP_ARRC] = &&exec_arrc,
      [VM_INT_OP_MAP] = &&exec_map,
      [VM_INT_OP_GET] = &&exec_get,
      [VM_INT_OP_GETC] = &&exec_getc,
      [VM_INT_OP_SET] = &&exec_set,
      [VM_INT_OP_SETC] = &&exec_setc,
      [VM_INT_OP_CSET] = &&exec_cset,
      [VM_INT_OP_CSETC] = &&exec_csetc,
      [VM_INT_OP_LEN] = &&exec_len,
  };
  uint8_t *jumps = vm_jump_all(nops, iops);
  if (jumps == NULL)
  {
    return 1;
  }
  uint8_t *ops = vm_int_comp(nops, iops, jumps, &ptrs[0], gc);
  vm_free(jumps);
  if (ops == NULL)
  {
    return 1;
  }
  size_t nframes = VM_CONFIG_NUM_FRAMES;
  size_t nregs0 = VM_CONFIG_NUM_REGS;
  vm_value_t *regs = vm_alloc0(sizeof(vm_value_t) * nregs0);
  vm_value_t *regs_base = regs;
  vm_reg_t *stack = vm_malloc(sizeof(vm_value_t) * nframes);
  vm_reg_t *stack_base = stack;
  vm_loc_t index = 0;
  gc->running = true;
  gc->regs = regs_base;
  gc->nregs = nregs0;
  vm_int_jump_next();
exec_exit:
{
  vm_free(stack_base);
  vm_free(regs_base);
  vm_free(ops);
  return 0;
}
exec_putc:
{
  vm_value_t reg = vm_int_read_load();
  putchar(vm_value_to_int(gc, reg));
  vm_int_jump_next();
}
exec_mov:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t in = vm_int_read_load();
  *out = in;
  vm_int_jump_next();
}
exec_movc:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t in = vm_int_read_value();
  *out = in;
  vm_int_jump_next();
}
exec_movf:
{
  vm_value_t *out = vm_int_read_store();
  vm_loc_t in = vm_int_read_loc();
  *out = vm_value_from_func(in);
  vm_int_jump_next();
}
exec_add:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_add(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_sub:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_sub(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_ret:
{
  vm_value_t inval = vm_int_read_load();
  index = *--stack;
  size_t nregs = regs - regs_base;
  regs -= vm_int_read_at(vm_reg_t, index - sizeof(vm_reg_t));
  *vm_int_read_store() = inval;
  vm_int_jump_next();
}
exec_beq:
{
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_load();
  index = vm_int_read_at(vm_loc_t, index + vm_value_is_equal(gc, lhs, rhs) * sizeof(vm_loc_t));
  vm_int_jump_next();
}
exec_beqc:
{
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_value();
  index = vm_int_read_at(vm_loc_t, index + vm_value_is_equal(gc, lhs, rhs) * sizeof(vm_loc_t));
  vm_int_jump_next();
}
exec_call0:
{
  vm_loc_t func = vm_int_read_loc();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  index = func;
  vm_int_jump_next_check();
}
exec_call1:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  index = func;
  vm_int_jump_next_check();
}
exec_call2:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  index = func;
  vm_int_jump_next_check();
}
exec_call3:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  index = func;
  vm_int_jump_next_check();
}
exec_call4:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  index = func;
  vm_int_jump_next_check();
}
exec_call5:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  index = func;
  vm_int_jump_next_check();
}
exec_call6:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_value_t r6 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  regs[6] = r6;
  index = func;
  vm_int_jump_next_check();
}
exec_call7:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_value_t r6 = vm_int_read_load();
  vm_value_t r7 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  regs[6] = r6;
  regs[7] = r7;
  index = func;
  vm_int_jump_next_check();
}
exec_call8:
{
  vm_loc_t func = vm_int_read_loc();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_value_t r6 = vm_int_read_load();
  vm_value_t r7 = vm_int_read_load();
  vm_value_t r8 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  regs[6] = r6;
  regs[7] = r7;
  regs[8] = r8;
  index = func;
  vm_int_jump_next_check();
}
exec_dcall0:
{
  vm_value_t func = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall1:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall2:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall3:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall4:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall5:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall6:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_value_t r6 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  regs[6] = r6;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall7:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_value_t r6 = vm_int_read_load();
  vm_value_t r7 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  regs[6] = r6;
  regs[7] = r7;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_dcall8:
{
  vm_value_t func = vm_int_read_load();
  vm_value_t r1 = vm_int_read_load();
  vm_value_t r2 = vm_int_read_load();
  vm_value_t r3 = vm_int_read_load();
  vm_value_t r4 = vm_int_read_load();
  vm_value_t r5 = vm_int_read_load();
  vm_value_t r6 = vm_int_read_load();
  vm_value_t r7 = vm_int_read_load();
  vm_value_t r8 = vm_int_read_load();
  vm_reg_t nregs = vm_int_read_reg();
  *stack++ = index;
  regs += nregs;
  regs[1] = r1;
  regs[2] = r2;
  regs[3] = r3;
  regs[4] = r4;
  regs[5] = r5;
  regs[6] = r6;
  regs[7] = r7;
  regs[8] = r8;
  index = vm_value_to_func(func);
  vm_int_jump_next_check();
}
exec_jump:
{
  vm_loc_t loc = vm_int_read_loc();
  index = loc;
  vm_int_jump_next();
}
exec_djump:
{
  vm_value_t reg = vm_int_read_load();
  index = vm_value_to_func(reg);
  vm_int_jump_next();
}
exec_addc:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_value();
  *out = vm_value_add(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_subc:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_value();
  *out = vm_value_sub(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_csub:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_value();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_sub(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_retc:
{
  vm_value_t inval = vm_int_read_value();
  index = *--stack;
  size_t nregs = regs - regs_base;
  regs -= vm_int_read_at(vm_reg_t, index - sizeof(vm_reg_t));
  *vm_int_read_store() = inval;
  vm_int_jump_next();
}
exec_mul:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_mul(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_mulc:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_value();
  *out = vm_value_mul(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_div:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_div(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_divc:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_value();
  *out = vm_value_div(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_cdiv:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_value();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_div(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_mod:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_mod(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_modc:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_value();
  *out = vm_value_mod(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_cmod:
{
  vm_value_t *out = vm_int_read_store();
  vm_value_t lhs = vm_int_read_value();
  vm_value_t rhs = vm_int_read_load();
  *out = vm_value_mod(gc, lhs, rhs);
  vm_int_jump_next();
}
exec_bb:
{
  vm_value_t in = vm_int_read_load();
  index = vm_int_read_at(vm_loc_t, index + vm_value_to_bool(gc, in) * sizeof(vm_loc_t));
  vm_int_jump_next();
}
exec_blt:
{
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_load();
  index = vm_int_read_at(vm_loc_t, index + vm_value_is_less(gc, lhs, rhs) * sizeof(vm_loc_t));
  vm_int_jump_next();
}
exec_bltc:
{
  vm_value_t lhs = vm_int_read_load();
  vm_value_t rhs = vm_int_read_value();
  index = vm_int_read_at(vm_loc_t, index + vm_value_is_less(gc, lhs, rhs) * sizeof(vm_loc_t));
  vm_int_jump_next();
}
exec_cblt:
{
  vm_value_t lhs = vm_int_read_value();
  vm_value_t rhs = vm_int_read_load();
  index = vm_int_read_at(vm_loc_t, index + vm_value_is_less(gc, lhs, rhs) * sizeof(vm_loc_t));
  vm_int_jump_next();
}
exec_arr:
{
  vm_value_t *outreg = vm_int_read_store();
  vm_value_t num = vm_int_read_load();
  *outreg = VM_VALUE_SET_ARR(vm_gc_arr(gc, vm_value_to_int(gc, num)));
  vm_gc_run(gc);
  vm_int_jump_next();
}
exec_arrc:
{
  vm_value_t *outreg = vm_int_read_store();
  vm_value_t num = vm_int_read_value();
  *outreg = VM_VALUE_SET_ARR(vm_gc_arr(gc, vm_value_to_int(gc, num)));
  vm_gc_run(gc);
  vm_int_jump_next();
}
exec_map:
{
  __builtin_trap();
  vm_int_jump_next();
}
exec_get:
{
  vm_value_t *outreg = vm_int_read_store();
  vm_value_t obj = vm_int_read_load();
  vm_value_t key = vm_int_read_load();
  *outreg = vm_gc_get_v(gc, obj, key);
  vm_int_jump_next();
}
exec_getc:
{
  vm_value_t *outreg = vm_int_read_store();
  vm_value_t obj = vm_int_read_load();
  vm_value_t key = vm_int_read_value();
  *outreg = vm_gc_get_v(gc, obj, key);
  vm_int_jump_next();
}
exec_set:
{
  vm_value_t obj = vm_int_read_load();
  vm_value_t key = vm_int_read_load();
  vm_value_t value = vm_int_read_load();
  vm_gc_set_vv(gc, obj, key, value);
  vm_int_jump_next();
}
exec_setc:
{
  vm_value_t obj = vm_int_read_load();
  vm_value_t key = vm_int_read_load();
  vm_value_t value = vm_int_read_value();
  vm_gc_set_vv(gc, obj, key, value);
  vm_int_jump_next();
}
exec_cset:
{
  vm_value_t obj = vm_int_read_load();
  vm_value_t key = vm_int_read_value();
  vm_value_t value = vm_int_read_load();
  vm_gc_set_vv(gc, obj, key, value);
  vm_int_jump_next();
}
exec_csetc:
{
  vm_value_t obj = vm_int_read_load();
  vm_value_t key = vm_int_read_value();
  vm_value_t value = vm_int_read_value();
  vm_gc_set_vv(gc, obj, key, value);
  vm_int_jump_next();
}
exec_len:
{
  vm_value_t *output = vm_int_read_store();
  vm_value_t obj = vm_int_read_load();
  *output = vm_value_from_int(gc, vm_gc_len(gc, obj));
  vm_int_jump_next();
}
}

int vm_run_arch_int(size_t nops, const vm_opcode_t *ops)
{
  vm_gc_t gc;
  vm_gc_init(&gc);
  int res = vm_int_run(nops, ops, &gc);
  vm_gc_stop(gc);
  return res;
}
