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
  };

  if (tl_init(&tls, &opts)) {
    return -1;
  }

  size_t readen = 0;
  size_t cur;

  while (1) {
    fputs("/>", stdout);
    fgets(buffer, sizeof(buffer) - 1, stdin);
    len = strlen(buffer);
    if (!strncmp("Q!", buffer, 2))
      break;
    cur = 0;
    while (cur != len) {
      if (tl_read_raw(&tls, buffer + cur, len - cur, &readen)) {
        printf("Error.\n");
        break;
      }
      cur += readen;
    }
    printf("Stack: %u/%u\n", tls.stack_cur, tls.stack_size);
  }

  if (tl_destroy(&tls)) {
    return -1;
  }

  return 0;
}
