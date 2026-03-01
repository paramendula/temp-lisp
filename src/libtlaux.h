#ifndef LIBTLAUX_H_
#define LIBTLAUX_H_

#include "libtl.h"
#include <stdio.h>

// This is a TL allocator which utilizes C's malloc, calloc and free functions.
// It doesn't require any TL flags to be set
extern const tl_alloc_vt TLAUX_C_ALLOCATOR_VT;

const char *tlaux_type_to_str(tl_obj_type t);

const char *tlaux_ret_type_to_str(tl_ret_type t);

int tlaux_print_obj(tl_obj_ptr obj, int ident, FILE *stream);

// Fully evaluate the contents of the zero-terminated 'cstr'
int tlaux_eval(struct tl_state *, const char *cstr);

// Fully evaluate the contents of the 'cstr' of the length 'len'
int tlaux_eval_s(struct tl_state *, const char *cstr, unsigned long len);

#endif
