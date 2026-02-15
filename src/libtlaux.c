#include "libtlaux.h"
#include "libtl.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

// TL C Allocator ---

void *_tlca_alloc(void *_, tl_alloc_type __, size_t size_bytes) {
  return malloc(size_bytes);
}

void _tlca_free(void *_, tl_alloc_type __, void *ptr) { free(ptr); }

const tl_alloc_vt TLAUX_C_ALLOCATOR_VT = {
    .alloc = _tlca_alloc,
    .free = _tlca_free,
    .destroy = NULL,
};

// ---

const char *tlaux_type_to_str(tl_obj_type t) {
  switch (t) {
  case tltChar:
    return "Char";
  case tltInteger:
    return "Int";
  case tltUInteger:
    return "UInt";
  case tltBool:
    return "Bool";
  case tltUserPointer:
    return "UPtr";
  case tltUserFunction:
    return "UFunc";
  case tltUserMacro:
    return "UMacro";
  case tltFunction:
    return "Func";
  case tltMacro:
    return "Macro";
  case tltNil:
    return "Nil";
  case tltDouble:
    return "Double";
  case tltNode:
    return "Node";
  case tltString:
    return "String";
  case tltSymbol:
    return "Symbol";
  default:
    return "!!UNKNOWN!!";
  }
}

int _tlaux_print_obj(tl_obj_ptr obj, int ident, FILE *stream, char top) {
  /* if (!top) { */
  /*   for (int i = 0; i < ident; i++) */
  /*     fputc(' ', stream); */
  /* } */
  /**/
  switch (obj.t) {
  case tltChar:
    // TODO: proper char print
    if (isprint(obj.ch)) {
      fprintf(stream, "#\\%c", obj.ch);
    } else {
      fprintf(stream, "#\\%d", obj.ch);
    }
    break;
  case tltInteger:
    fprintf(stream, "%ld", obj.intg);
    break;
  case tltUInteger:
    fprintf(stream, "%lu", obj.uintg);
    break;
  case tltBool:
    fprintf(stream, "#%s", (obj.booln) ? "true" : "false");
    break;
  case tltUserPointer:
    fprintf(stream, "<UPtr %p>", obj.user_ptr);
    break;
  case tltUserFunction:
    fprintf(stream, "<UFunc %p>", obj.user_func);
    break;
  case tltUserMacro:
    fprintf(stream, "<UMacro %p>", obj.user_macro);
    break;
  case tltFunction:
    fprintf(stream, "<Func %p>", obj.func);
    break;
  case tltMacro:
    fprintf(stream, "<Macro %p>", obj.macro);
    break;
  case tltNil:
    fprintf(stream, "#nil");
    break;
  case tltDouble:
    fprintf(stream, "%lf", obj.dbl);
    break;
  case tltNode: {
    // TODO: make printing nodes not stack-dependent
    // TODO: pretty print lists (currently ignores ident)
    fputc('(', stream);
    for (tl_node *n = obj.node;;) {
      _tlaux_print_obj(n->head, ident + 2, stream, 0);
      if (n->tail.t != tltNil) {
        if (n->tail.t == tltNode) {
          fputc(' ', stream);
          n = n->tail.node;
        } else {
          fputs(" . ", stream);
          _tlaux_print_obj(n->tail, ident + 2, stream, 0);
          break;
        }
      } else {
        break;
      }
    }
    fputc(')', stream);
    break;
  }
  case tltString:
    // TODO: proper raw str print (with escape chars and possible newlines for
    // prettiness)
    fputc('"', stream);
    if (obj.str)
      fwrite(obj.str->raw, 1, obj.str->len, stream);
    fputc('"', stream);
    break;
  case tltSymbol:
    fputc('\'', stream);
    for (tl_symbol *cur = obj.sym;;) {
      fwrite(cur->part->raw, 1, cur->part->len, stream);
      if (cur->next) {
        fputc('.', stream);
        cur = cur->next;
      } else
        break;
    }
    break;
  default:
    fputs("<!!UNKNOWN!!>", stream);
  }
  return 0;
}

int tlaux_print_obj(tl_obj_ptr obj, int ident, FILE *stream) {
  return _tlaux_print_obj(obj, ident, stream, 1);
}
