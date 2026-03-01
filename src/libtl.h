#ifndef LIBTL_H_
#define LIBTL_H_

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

// Project-wide TODO
// TODO: use uniform 'cell' instead of unsigned ints or unsigned longs
// ^ typedef uintmax_t cell;
// TODO: refactor all struct names and functions
// TODO: error handling (raise?)
// TODO: complex refactoring of parsing and evaluation
// TODO: make TL_DEBUG_* definitions dependent on each other
// TODO: move project-wide TODO to another file...

// TODO: divide libht into separate files (modularity):
// * libtlread
// * libtleval
// * libtlenv
// * libtltable
// * libtlexec ?? vm? for bytecode funcs
// ...

// Config     ---
#define TL_DEBUG 1
#define TL_DEBUG_LOG 1
#define TL_DEBUG_STACK 1
#define TL_DEBUG_RSTACK 1
// Config End ---

// allocator's destroy() frees all its memory (used in tl_destroy)
// e.g. useful for arena allocators (no need to free() everything manually)
#define TL_FLAG_ALLOC_DIF ((int)1)
// allocator's free() isn't needed, so it won't be called in tl_destroy
// allocators that don't have neither destroy() nor free() must have this flag
// set
#define TL_FLAG_ALLOC_FNN ((int)2)

typedef int tl_bool;

// constants for tl_bool
#define TL_TRUE ((tl_bool)INT_MAX) // just like in Forth
#define TL_FALSE ((tl_bool)0)

typedef uint32_t tl_uchar;

typedef struct tl_state tl_state;

// 'raw' isn't necessarily zero-terminated
typedef struct tl_str {
  unsigned int len;
  char *raw;
} tl_str;

// Multipart Symbol
// e.g. math.sin or game.players.ban
typedef struct tl_symbol {
  struct tl_symbol *next;
  tl_str *part;
} tl_symbol;

typedef enum tl_obj_type {
  tltNil,
  tltNode,
  tltString,
  tltChar,
  tltSymbol,
  tltBool,
  tltInteger,
  tltUInteger,
  tltDouble, // ^ these three are all a Number
  tltFunction,
  tltMacro,
  tltUserFunction,
  tltUserMacro,
  tltUserPointer,
  tltTable,
} tl_obj_type;

// type of allocation
typedef enum tl_alloc_type {
  tlatStack,
  tlatRStack,
  tlatNode,
  tlatStrStruct,
  tlatStrRaw,
  tlatSymStruct,
  tlatEnvStruct,
  tlatEnvBuckArr,
  tlatEnvBucket,
  tlatHtStruct,
  tlatHtBucket,
  tlatHtBuckArr,
} tl_alloc_type;

typedef enum tl_bytecode {
  tlbNop = 0,
  tlbRet,
  // TODO: bytecode
} tl_bytecode;

typedef struct tl_func_param {
  struct tl_func_param *next;
  tl_str *name;
} tl_func_param;

typedef struct tl_func {
  struct tl_env *env;
  tl_func_param *first_param;
  tl_str *rest_param;
  char is_bytecode;
  unsigned long bc_len;
  union {
    char *bytecode;
    struct tl_node *items;
  };
} tl_func;

// always used as *tl_user_func
typedef void(tl_user_func)(struct tl_state *, struct tl_env *);

typedef struct tl_ufunc_wrap {
  struct tl_env *env;
  tl_user_func *ufunc;
} tl_ufunc_wrap;

// 8 or 16 bytes
typedef struct tl_obj_ptr {
  tl_obj_type t;
  union {
    struct tl_node *node;
    struct tl_str *str;
    tl_uchar ch;
    struct tl_symbol *sym;
    tl_bool booln;
    intmax_t intg;
    uintmax_t uintg;
    double dbl;
    struct tl_func *func, *macro;
    struct tl_ufunc_wrap *user_func, *user_macro;
    void *user_ptr;
    struct tl_table *table;
  };
} tl_obj_ptr;

typedef struct tl_node {
  tl_obj_ptr head, tail;
} tl_node;

typedef struct tl_env_bucket {
  unsigned long hash;
  struct tl_env_bucket *prev, *next, *next_col;
  tl_symbol *key;
  tl_obj_ptr val;
} tl_env_bucket;

typedef struct tl_env {
  unsigned long len, cap;
  struct tl_env_bucket **buckets, *last;
  struct tl_env *prev; // parent environment
} tl_env;

typedef struct tl_table_bucket {
  unsigned long hash;
  struct tl_table_bucket *prev, *next, *next_col;
  tl_obj_ptr key, val;
} tl_table_bucket;

typedef struct tl_table {
  unsigned long len, cap;
  struct tl_table_bucket **buckets, *last;
} tl_table;

// This is an allocator VT with metadata (allocation types)
// free() and destroy() may be NULL
// Rules:
// if both are NULL, then set the TL_FLAG_ALLOC_FNN flag (freeing memory isn't
// needed)
// if only free is NULL, then set the TL_FLAG_ALLOC_DIF flag (destroy()
// frees the memory)
typedef struct tl_alloc_vt {
  void *(*alloc)(void *, tl_alloc_type type, size_t size_bytes);
  void (*free)(void *, tl_alloc_type type, void *ptr);
  int (*destroy)(void *);
} tl_alloc_vt;

typedef struct tl_init_opts {
  int flags;
  unsigned int stack_size, stack_cur;
  tl_obj_ptr *stack_preinit;
  unsigned int rstack_size, rstack_cur;
  struct tl_ret *rstack_preinit;
  void *alloc; // allocator ptr
  const tl_alloc_vt *alloc_vt;
} tl_init_opts;

typedef struct tl_gc {
  int pass;
} tl_gc;

typedef enum tl_ret_type {
  tlrInterpret,  // object eval, usually from tl_eval
  tlrBytecode,   // bytecode run
  tlrFunc,       // function call
  tlrUser,       // user (C) function call
  tlrRet,        // stop TL (return from tl_run)
  tlrInterCheck, // check node's head evaluation result (run or error)
} tl_ret_type;

// TODO: more metadata
typedef struct tl_ret {
  tl_ret_type t;
  union {
    struct {
      tl_obj_ptr *obj;
      tl_node *parent;
    } inter;
    struct {
      tl_node *rest;
    } inter_check;
    struct {
      tl_func *f;
      unsigned long stack_offset; // for calculating args count
    } func;
    struct {
      tl_func *f;
      unsigned long offset;
    } bc;
    struct {
      tl_ufunc_wrap *u;
      unsigned long stack_offset;
    } user;
    struct {
      tl_obj_ptr *out;
      unsigned long stack_offset;
    } ret;
  };
} tl_ret;

typedef struct tl_state {
  int flags;

  tl_gc gc;
  void *alloc; // allocator ptr
  const tl_alloc_vt *alloc_vt;

  unsigned int stack_size, stack_cur;
  tl_obj_ptr *stack;

  unsigned int rstack_size, rstack_cur;
  tl_ret *rstack;

  int args_count; // for function calls

  struct tl_env *top_env;
} tl_state;

// Initialize TL, possibly allocating the stack
int tl_init(struct tl_state *, tl_init_opts *opts);
// Deinitialize TL, freeing all memory (if possible)
int tl_destroy(struct tl_state *);

// Read(parse) the first object of the UTF-8 string 'str' of length 'len'
// into '*ret'. If readen_out != NULL, put the
// amount of readen characters into it.
// 'ret' may be null.
// If no object was parsed (whitespace met only), then '*readen_out' = 0 and
// '*ret' = NULL.
// If an object was parsed, it's NOT automatically registered in
// the GC.
int tl_read_raw(struct tl_state *, const char *str, size_t len, tl_obj_ptr *ret,
                size_t *readen_out);
// Evaluate 'obj' into 'ret'.
// The resulting object IS registered in the GC.
// Calls tl_run.
// TODO: current limitation: only one return value possible
int tl_eval_raw(struct tl_state *, tl_obj_ptr obj, tl_obj_ptr *ret);
// Pop the top stack string and read(parse) it using tl_read_raw.
// Then push the output object onto the stack. The object is registered in the
// GC.
int tl_read(struct tl_state *);
// Pop the top stack object and evaluate it using tl_eval_raw.
// Then push the output object (eval result) onto the stack.
// The resulting object is registered in the GC.
// Calls tl_run.
int tl_eval(struct tl_state *);

// Pop the last return object (tl_ret) from the return stack and run it.
int tl_run(struct tl_state *);

// Push arguments onto the stack to pass them to the func.
// Calls tl_run.
int tl_run_func(struct tl_state *, tl_func *func);
// Push arguments onto the stack to pass them to the user func.
// Calls tl_run.
int tl_run_ufunc(struct tl_state *, tl_ufunc_wrap *ufunc);

int tl_stack_pop(struct tl_state *, tl_obj_ptr *ret);
// Just like tl_stack_pop but without actually deleting the value from the stack
int tl_stack_peek(struct tl_state *, tl_obj_ptr *ret);
int tl_stack_push(struct tl_state *, tl_obj_ptr obj);

int tl_rstack_pop(struct tl_state *, tl_ret *ret);
// Just like tl_rstack_pop but without actually deleting the value from the
// return stack
int tl_rstack_peek(struct tl_state *, tl_ret *ret);
int tl_rstack_push(struct tl_state *, tl_ret obj);

// Insert an object pointer into GC, making it managed memory
int tl_gc_register(struct tl_state *, tl_obj_ptr obj);
// Remove an object from GC, stopping it from being managed memory.
// If obj isn't gc registered, the function does nothing.
int tl_gc_unregister(struct tl_state *, tl_obj_ptr obj);
// Mark all accessible objects to escape deletion.
// Object is accessible if you can access it while traversing:
// Stack and Return Stack, Top Env.
// Traversing the return stack also means traversing related local environments.
int tl_gc_mark(struct tl_state *, struct tl_env *env);
// Free all inaccessible (unmarked) objects and remove them from GC
int tl_gc_sweep(struct tl_state *);

// returns 0 if equal, both may be NULL
int tl_str_cmp(tl_str *lhs, tl_str *rhs);

// TODO: tl_env_* description

// Beware: neither key nor val are automatically GC registered.
int tl_env_insert(struct tl_state *, struct tl_env *, tl_symbol *key,
                  tl_obj_ptr val, tl_env_bucket **out);
int tl_env_remove(struct tl_state *, struct tl_env *, tl_symbol *key,
                  tl_env_bucket **out);
int tl_env_get(struct tl_state *, struct tl_env *, tl_symbol *key,
               tl_env_bucket **out);
// Try getting env[key] but only in 'env', without searching in parents
int tl_env_get_here(struct tl_state *, struct tl_env *, tl_symbol *key,
                    tl_env_bucket **out);
int tl_env_set(struct tl_state *, struct tl_env *, tl_symbol *key,
               tl_obj_ptr val, tl_obj_ptr *out);

// TODO: tl_table_* description

// Objects that can server as table keys
#define TL_TABLE_CAN_KEY(t)                                                    \
  ((t) == tltString || (t) == tltSymbol || (t) == tltChar ||                   \
   (t) == tltInteger || (t) == tltUInteger || (t) == tltBool)

int tl_table_insert(struct tl_state *, struct tl_table *, tl_obj_ptr key,
                    tl_obj_ptr val, tl_table_bucket **out);
int tl_table_remove(struct tl_state *, struct tl_table *, tl_obj_ptr key,
                    tl_table_bucket **out);
int tl_table_get(struct tl_state *, struct tl_table *, tl_obj_ptr key,
                 tl_table_bucket **out);

// Constants
extern tl_obj_ptr tlNil;
extern tl_obj_ptr tlTrue;
extern tl_obj_ptr tlFalse;

#endif
