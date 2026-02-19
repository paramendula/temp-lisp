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
// TODO: divide into separate files (modularity)

#include "libtlht.h"

#define TL_DEBUG 1
#define TL_DEBUG_LOG 1
#define TL_DEBUG_STACK 1

// allocator's destroy() frees all its memory (used in tl_destroy)
// e.g. useful for arena allocators (no need to free() everything manually)
#define TL_FLAG_ALLOC_DIF ((int)1)
// allocator's free() isn't needed, so it won't be called in tl_destroy
// allocators that don't have neither destroy() nor free() must have this flag
// set
#define TL_FLAG_ALLOC_FNN ((int)2)

typedef int tl_bool;

// constants for tl_bool
#define TL_TRUE ((tl_bool)INT_MAX)
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

typedef struct tl_func {
  struct tl_env *env;
  // TODO: func code
} tl_func;

// always used as *tl_user_func
typedef void(tl_user_func)(struct tl_state *, struct tl_env *);

typedef struct tl_ufunc_wrap {
  struct tl_env *env;
  tl_user_func *ufunc;
} tl_ufunc_wrap;

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
    tl_func *func, *macro;
    tl_ufunc_wrap *user_func, *user_macro;
    void *user_ptr;
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
  void *alloc; // allocator ptr
  const tl_alloc_vt *alloc_vt;
} tl_init_opts;

typedef struct tl_gc {
  int pass;
} tl_gc;

typedef struct tl_state {
  int flags;

  tl_gc gc;
  void *alloc; // allocator ptr
  const tl_alloc_vt *alloc_vt;

  unsigned int stack_size, stack_cur;
  tl_obj_ptr *stack;

  struct tl_env *top_env;
} tl_state;

// Initialize TL, possibly allocating the stack
int tl_init(struct tl_state *, tl_init_opts *opts);
// Deinitialize TL, freeing all memory (if possible)
int tl_destroy(struct tl_state *);

// TODO: are non-raw function variants needed?
// ^ maybe raw variants DON'T interact with GC!
// if so, TODO: remove GC interacts from raw variants

// Read(parse) the first object of the UTF-8 string 'str' of length 'len'
// and push it to the top of the stack. If readen_out != NULL, put the
// amount of readen characters into it.
int tl_read_raw(struct tl_state *, const char *str, size_t len,
                size_t *readen_out);
// Evaluate 'obj' into 'ret'
int tl_eval_raw(struct tl_state *, tl_obj_ptr obj, tl_obj_ptr *ret);
// Pop the top stack string and read(parse) it using tl_read_raw
int tl_read(struct tl_state *);
// Pop the top stack object and evaluate it using tl_eval_raw
int tl_eval(struct tl_state *);

int tl_run_func(struct tl_state *, tl_func *func);
int tl_run_ufunc(struct tl_state *, tl_user_func *ufunc);

int tl_stack_pop(struct tl_state *, tl_obj_ptr *ret);
// Just like tl_stack_pop but without actually deleting the value from the stack
int tl_stack_peek(struct tl_state *, tl_obj_ptr *ret);
int tl_stack_push(struct tl_state *, tl_obj_ptr obj);

// Insert an object pointer into GC, making it managed memory
int tl_gc_register(struct tl_state *, tl_obj_ptr obj);
// Remove an object from GC, stopping it from being managed memory.
// If obj isn't gc registered, the function does nothing.
int tl_gc_unregister(struct tl_state *, tl_obj_ptr obj);
// Mark all inaccessible objects (starting from 'env' ending with top env)
int tl_gc_mark(struct tl_state *, struct tl_env *env);
// Free all inaccessible objects and remove them from GC
int tl_gc_sweep(struct tl_state *);

// TODO: tl_env_* description

// gc register obj before calling this func
int tl_env_insert(struct tl_state *, struct tl_env *, tl_symbol *key,
                  tl_obj_ptr val, tl_env_bucket **out);
int tl_env_remove(struct tl_state *, struct tl_env *, tl_symbol *key,
                  tl_env_bucket **out);
int tl_env_get(struct tl_state *, struct tl_env *, tl_symbol *key,
               tl_env_bucket **out);
int tl_env_set(struct tl_state *, struct tl_env *, tl_symbol *key,
               tl_obj_ptr val, tl_obj_ptr *out);

// TODO: tl_table_* description

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
