#include "libtl.h"
#include "libtlaux.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  char buffer[1024] = {0};
  size_t len = 0;

  tl_state tls = {0};
  tl_init_opts opts = {
      .alloc_vt = &TLAUX_C_ALLOCATOR_VT,
      .stack_size = 256,
      .rstack_size = 128,
  };

  if (tl_init(&tls, &opts)) {
    return -1;
  }

  size_t readen = 0;
  size_t cur;

  tl_obj_ptr obj_read = {0}, ret = {0};
  while (1) {
    fputs("/>", stdout);
    fgets(buffer, sizeof(buffer) - 1, stdin);
    len = strlen(buffer);
    if (!strncmp("Q!", buffer, 2))
      break;
    cur = 0;
    while (cur != len) {
      if (tl_read_raw(&tls, buffer + cur, len - cur, &obj_read, &readen)) {
        printf("Error.\n");
        break;
      }

      if (readen == 0)
        break;

      cur += readen;

      if (tl_eval_raw(&tls, obj_read, &ret)) {
        printf("Eval error.\n");
        break;
      }

      if (ret.t != tltNil) {
        tlaux_print_obj(ret, 2, stdout);
        putc('\n', stdout);
        ret.t = tltNil;
      }
    }
    // printf("Stack: %u/%u\n", tls.stack_cur, tls.stack_size);
  }

  if (tl_destroy(&tls)) {
    return -1;
  }

  return 0;
}
