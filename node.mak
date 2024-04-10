
EXE ?= .js
CC = emcc
TCC_SRCS = 
CFLAGS := $(CFLAGS)
GCCJIT = NO 

# CFLAGS_TB = -DTB_HAS_WASM

include core.mak

CFLAGS := -fPIC $(CFLAGS)
LDFLAGS := -s SINGLE_FILE=1 -lnodefs.js -s MALLOC=mimalloc -s BINARYEN_ASYNC_COMPILATION=0 -s STACK_SIZE=64mb -s INITIAL_MEMORY=256mb -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s ENVIRONMENT=node -s MAIN_MODULE=2 -s EXPORTED_RUNTIME_METHODS="['FS','callMain','ENV']" $(LDFLAGS) 
# LDFLAGS := -lnodefs.js -s MALLOC=mimalloc -s ASYNCIFY=1 -s STACK_SIZE=64mb -s INITIAL_MEMORY=256mb -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s ENVIRONMENT=node -s MAIN_MODULE=2 -s EXPORTED_RUNTIME_METHODS="['FS','callMain','ENV','TTY']" $(LDFLAGS) 

POST_INSTALL = mv build/bin/minivm.js build/bin/minivm.mjs
