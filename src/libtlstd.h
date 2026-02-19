#ifndef LIBTLSTD_H_
#define LIBTLSTD_H_

#include "libtl.h"

// TL standard library
// prefix 'tlstd*f_' for TL user functions
// prefix 'tlstd*_' for C library functions

// TODO: move tlstdf_add to the separate module (std.math)
// (+ ...)
void tlstdf_add(struct tl_state *, tl_env *);

// Load all TL std library functions into the 'env' environment.
// Set 'env' to NULL to load it into the top environment.
// 'prefix' may also be NULL
int tlstd_load(struct tl_state *, struct tl_env *env, tl_symbol *prefix);

#endif
