#ifndef LIBTLSTD_CORE_H_
#define LIBTLSTD_CORE_H_

#include "libtl.h"

// define, set!

void tlstd_coref_define(struct tl_state *, struct tl_env *);
void tlstd_coref_set(struct tl_state *, struct tl_env *);

// Load all TL std.core library functions into the 'env' environment.
// Set 'env' to NULL to load it into the top environment.
// 'prefix' may also be NULL
int tlstd_core_load(struct tl_state *, struct tl_env *env, tl_symbol *prefix);

#endif
