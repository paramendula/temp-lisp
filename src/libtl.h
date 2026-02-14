#ifndef LIBTL_H_
#define LIBTL_H_

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

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
  tltUserFunction,
  tltUserPointer,
} tl_obj_type;

// type of allocation
typedef enum tl_alloc_type {
  tlatStack,
  tlatNode,
  tlatStrRaw,
  tlatStrStruct,
  tlatSymStruct,
} tl_alloc_type;

typedef struct tl_func {
  struct tl_env *env;
  // TODO: func code
} tl_func;

typedef void(tl_user_func)(struct tl_state *);

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
    tl_func *func;
    tl_user_func *user_func;
    void *user_ptr;
  };
} tl_obj_ptr;

typedef struct tl_node {
  tl_obj_ptr head, tail;
} tl_node;

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

typedef struct tl_env {
  struct tl_env *prev; // parent environment
} tl_env;

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

// Read(parse) the first object of the UTF-8 string 'str' of length 'len'
// and push it to the top of the stack. If readen_out != NULL, put the
// amount of readen characters into it.
int tl_read_raw(struct tl_state *, const char *str, size_t len,
                size_t *readen_out);
// Pop the top stack string and read(parse) it using tl_read_raw
int tl_read(struct tl_state *);
// Pop the top stack object and evaluate it
int tl_eval(struct tl_state *);

int tl_run_func(struct tl_state *, tl_func *func);
int tl_run_ufunc(struct tl_state *, tl_user_func *ufunc);

int tl_stack_pop(struct tl_state *, tl_obj_ptr *ret);
// Just like tl_stack_pop but without actually deleting the value from the stack
int tl_stack_peek(struct tl_state *, tl_obj_ptr *ret);
int tl_stack_push(struct tl_state *, tl_obj_ptr obj);

// Insert an object pointer into GC, making it managed memory
int tl_gc_register(struct tl_state *, tl_obj_ptr obj);
// Remove an object from GC, stopping it from being managed memory
int tl_gc_unregister(struct tl_state *, tl_obj_ptr obj);
// Mark all inaccessible objects (from the top env and stack)
int tl_gc_mark(struct tl_state *);
// Free all inaccessible objects and remove them from GC
int tl_gc_sweep(struct tl_state *);

extern tl_obj_ptr tlNil;
extern tl_obj_ptr tlTrue;
extern tl_obj_ptr tlFalse;

#endif
