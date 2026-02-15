#include "libtlstd.h"

// (+ 1 2 3.0 4.5 -1337 +42.111)
// TODO: unsigned integer handle?
void tlstdf_add(struct tl_state *s, tl_env *_) {
  tl_obj_ptr result = (tl_obj_ptr){.t = tltInteger, .intg = 0};

  tl_obj_ptr pop;
  while (s->stack_cur) {
    if (tl_stack_pop(s, &pop))
      return;
    if (pop.t == tltDouble) {
      if (result.t != tltDouble) {
        result = (tl_obj_ptr){.t = tltDouble, .dbl = ((double)result.intg)};
      }
      result.dbl += pop.dbl;
    } else if (pop.t == tltInteger) {
      if (result.t == tltDouble) {
        result.dbl += ((double)pop.intg);
      }
    } else {
      // TODO: error handling
    }
  }

  tl_stack_push(s, result);
}
