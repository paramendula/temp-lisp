#include "libtl.h"
#include "libtlht.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if TL_DEBUG != 0 && TL_DEBUG_LOG != 0
#include "libtlaux.h"
#include <stdarg.h>
#include <stdio.h>

void tl_dlog(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fputs("[TLD]: ", stderr);
  vfprintf(stderr, fmt, args);
  fputc('\n', stderr);
  va_end(args);
}

#else

#define tl_dlog(fmt, ...) ((void)0)

#endif

#if TL_DEBUG_STACK != 0
#include "libtlaux.h"
#endif

tl_obj_ptr tlNil = (tl_obj_ptr){.t = tltNil, .user_ptr = NULL};
tl_obj_ptr tlTrue = (tl_obj_ptr){.t = tltBool, .booln = TL_TRUE};
tl_obj_ptr tlFalse = (tl_obj_ptr){.t = tltBool, .booln = TL_FALSE};

int tl_init(struct tl_state *s, tl_init_opts *opts) {
  s->flags = opts->flags;
  s->alloc = opts->alloc;
  s->alloc_vt = opts->alloc_vt;

  if (opts->stack_size == 0) {
    tl_dlog("Stack size can't be 0.");
    return -1;
  }

  s->stack_size = opts->stack_size;

  if (opts->stack_preinit) {
    s->stack = opts->stack_preinit;
    s->stack_cur = opts->stack_cur;
  } else {
    if (!s->alloc_vt->alloc) {
      tl_dlog("Can't allocate the stack: allocator's alloc() and "
              "opts->stack_preinit are both NULL.");
      return -1;
    }

    if (opts->stack_cur) {
      tl_dlog("Options' stack_cur != 0 while stack_preinit == NULL");
      return -1;
    }

    tl_obj_ptr *stack = s->alloc_vt->alloc(s->alloc, tlatStack,
                                           opts->stack_size * sizeof(*stack));

    if (!stack) {
      tl_dlog("Couldn't allocate the stack (NULL returned).");
      return -1;
    }

    s->stack_cur = 0;
    s->stack = stack;
  }

  return 0;
}

int tl_destroy(struct tl_state *s) {
  char manual_free = !(s->flags & TL_FLAG_ALLOC_FNN);

  if (s->flags & TL_FLAG_ALLOC_DIF) {
    if (!s->alloc_vt->destroy) {
      tl_dlog("TL_FLAG_ALLOC_DIF is set, but allocator's destroy() is NULL.");
      if (manual_free) {
        tl_dlog("^ manual_free=1, still freeing the memory. Check the "
                "TL_FLAG_ALLOC_FNN flag.");
      }
    } else {
      manual_free = 0;
    }
  }

  if (manual_free) {
    if (!s->alloc_vt->free) {
      tl_dlog("Can't free the memory: allocator's free() is NULL.");
    } else {
      // free the stack
      s->alloc_vt->free(s->alloc, tlatStack, s->stack);

      tl_dlog("Manual free not implemented yet.");
    }
  }

  if (s->alloc_vt->destroy) {
    s->alloc_vt->destroy(s->alloc);
  }

  return 0;
}

int tl_stack_pop(struct tl_state *s, tl_obj_ptr *ret) {
  if (s->stack_cur == 0) {
    tl_dlog("Tried to tl_stack_pop an empty stack.");
    return -1;
  }

  s->stack_cur -= 1;

#if TL_DEBUG != 0 && TL_DEBUG_STACK != 0
  tl_obj_ptr obj = s->stack[s->stack_cur];
  tl_dlog("Popped %s [%u/%u]:", tlaux_type_to_str(obj.t), s->stack_cur,
          s->stack_size);
  tlaux_print_obj(obj, 2, stderr);
  fputc('\n', stderr);
  tl_dlog("--");
#endif

  *ret = s->stack[s->stack_cur];

  return 0;
}

int tl_stack_peek(struct tl_state *s, tl_obj_ptr *ret) {
  if (s->stack_cur == 0) {
    tl_dlog("Tried to tl_stack_peek into an empty stack.");
    return -1;
  }

  *ret = s->stack[s->stack_cur - 1];

  return 0;
}

int tl_stack_push(struct tl_state *s, tl_obj_ptr obj) {
  if (s->stack_cur == s->stack_size) {
    tl_dlog("Tried to tl_stack_push into a full stack: [%u/%u].", s->stack_cur,
            s->stack_size);
    return -1;
  }

  s->stack[s->stack_cur] = obj;
  s->stack_cur++;

#if TL_DEBUG != 0 && TL_DEBUG_STACK != 0
  tl_dlog("Appended %s [%u/%u]:", tlaux_type_to_str(obj.t), s->stack_cur,
          s->stack_size);
  tlaux_print_obj(obj, 2, stderr);
  fputc('\n', stderr);
  tl_dlog("--");
#endif

  return 0;
}

#define _tl_cis_ident_start(ch)                                                \
  (isalpha(ch) || ((ch) == '_') || ((ch) == '!') || ((ch) == '~') ||           \
   ((ch) == '$') || ((ch) == '%') || ((ch) == '^') || ((ch) == ':') ||         \
   ((ch) == '/') || ((ch) == '?') || ((ch) == '&') || ((ch) == '*') ||         \
   ((ch) == '=') || ((ch) == '<') || ((ch) == '>') || ((ch) == '+') ||         \
   ((ch) == '-'))

#define _tl_cis_ident(ch) (_tl_cis_ident_start(ch) || isdigit(ch))

#define _tlr_lit 1
#define _tlr_nos 2
#define _tlr_str 3
#define _tlr_nnt 4
#define _tlr_sym 5
#define _tlr_num 6

int _tl_obj_free(struct tl_state *s, tl_obj_ptr obj, char free_node_insides) {
  // TODO:
  switch (obj.t) {
  case tltNode: {
    if (free_node_insides) {
      // TODO: make node free not stack-dependent
      tl_node *cur = obj.node;
      while (cur != NULL) {
        _tl_obj_free(s, cur->head, free_node_insides);
        if (cur->tail.t != tltNil) {
          if (cur->tail.t == tltNode) {
            cur = cur->tail.node;
          } else {
            _tl_obj_free(s, cur->tail, free_node_insides);
            break;
          }
        } else
          break;
      }
    }
    s->alloc_vt->free(s->alloc, tlatNode, obj.node);
    break;
  }
  case tltString:
    if (!obj.str)
      break;
    s->alloc_vt->free(s->alloc, tlatStrRaw, obj.str->raw);
    s->alloc_vt->free(s->alloc, tlatStrStruct, obj.str);
    break;
  case tltSymbol:
    break;
  default:
    break;
  }
  return 0;
}

int tl_str_cmp(tl_str *lhs, tl_str *rhs) {
  return (lhs != rhs) && (lhs->len != rhs->len) &&
         (memcmp(lhs->raw, rhs->raw, lhs->len));
}

tl_obj_ptr _tl_str_from_c(struct tl_state *s, const char *str, size_t len) {

  tl_str *tstr = s->alloc_vt->alloc(s->alloc, tlatStrStruct, sizeof(tl_str));
  if (!tstr) {
    return tlNil;
  }
  char *raw = s->alloc_vt->alloc(s->alloc, tlatStrRaw, (unsigned long)len);
  if (!raw) {
    s->alloc_vt->free(s->alloc, tlatStrStruct, tstr);
    return tlNil;
  }

  memcpy(raw, str, len);
  tstr->len = len;
  tstr->raw = raw;

  return (tl_obj_ptr){.t = tltString, .str = tstr};
}

int tl_gc_register(struct tl_state *s, tl_obj_ptr obj) { return 0; }

int tl_gc_unregister(struct tl_state *s, tl_obj_ptr obj) { return 0; }

// TODO: 1) divide into separate functions
// TODO: 2) refactor into recursive descent
// TODO: 2.5) maybe token parsing first?
// TODO: 3) syntax extensibility from outside (C) and inside (TL)
int tl_read_raw(struct tl_state *s, const char *str, size_t len,
                size_t *readen_out) {
  // TODO: line, number indicator in errors
  // TODO: 'quote, `semiquote, ,unquote; ,@splice-unquote
  tl_node *nodes[64]; // TODO: fix nodes parse depth, temp solution
  char buf[128];      // TODO: fix numbers parsing, temp solution

  tl_node *node_top = NULL, *node_cur = NULL;
  // How deep we are in the list tree? 0 = not in a list
  int depth = 0;
  // Current flag (symbol parsing, string parsing, etc.)
  int flag = 0;
  // For saving beginnings(indexes) of strings, symbols, etc.
  int temp = 0;

  // Current character index
  size_t i = 0;
  // Current character
  char ch;

  // Is node_cur->head NOT filled with a ready object?
  char head_empty = 1;
  // Did we go through the additional space char loop?
  char unfinished = 0;
  // If 1, then we've met tail specifier dot, if 2, then we've me the tail value
  char tailed = 0;
  // Is current number being parsed real (after dot was met)
  char real = 0;
  // Do we have an object ready in to_append?
  char append = 0;
  // Is the object in to_append allocated (not constant)?
  char allocated = 0;

  tl_obj_ptr to_append;

  for (; i < len; i++) { // parse loop (state machine)
    if (append) {
    label_append: // for finishing

      if (depth == 0) {
        if (allocated && tl_gc_register(s, to_append)) {
          goto on_fatal;
        }
        if (tl_stack_push(s, to_append)) {
          if (allocated)
            tl_gc_unregister(s, to_append); // shouldn't fail
          goto on_fatal;
        }
        allocated = 0;
        append = 0;
        break;
      }

      if (head_empty) {
        if (tailed) {
          tl_dlog("tl_read_raw met an attempt to set tail in a node without "
                  "head set");
          goto on_fatal;
        }
        head_empty = 0;
        node_cur->head = to_append;
      } else {
        if (!tailed) {
          tl_node *n = s->alloc_vt->alloc(s->alloc, tlatNode, sizeof(*n));
          if (!n) {
            goto on_nem;
          }
          node_cur->tail = (tl_obj_ptr){.t = tltNode, .node = n};
          node_cur = n;
          node_cur->head = to_append;
          n->tail = tlNil;
          nodes[depth - 1] = n;
        } else {
          if (tailed == 2) {
            tl_dlog("tl_read_raw found token after node's tail was set "
                    "(something after the tail value)");
            goto on_fatal;
          }
          tailed = 2;
          node_cur->tail = to_append;
        }
      }

      append = 0;

      i--;
      continue;
    }
    ch = str[i];
    /* printf("[%c] E%d T%d\n", ch, head_empty, tailed); */

  label_iteration: // for finishing
    switch (flag) {
    case _tlr_sym: { // Symbol
      if (_tl_cis_ident(ch)) {
        continue;
      } else if (ch == '.') {
        if (str[i - 1] == '.') {
          tl_dlog("tl_read_raw met two dots in a row while parsing a "
                  "symbol.");
          goto on_fatal;
        }
        continue;
      }
      if (str[i - 1] == '.') { // if symbol ends with '.' (wrong)
        tl_dlog("tl_read_raw met a symbol ending with a '.'");
        goto on_fatal;
      }
      // TODO: invalid symbol
      // TODO: can symbol non-first parts start with a digit?
      // str[temp:i] is a Symbol

      // allocate symbol struct, then allocate tl_str for each part
      tl_symbol *sym =
          s->alloc_vt->alloc(s->alloc, tlatSymStruct, sizeof(*sym));
      if (!sym) {
        goto on_nem;
      }

      tl_symbol *sym_last = sym;

      int start = temp, cur = temp + 1;

      char sym_empty = 1;

      // TODO: check gc for strings (symbol)
      while (start != i) {
        ch = str[cur];
        if ((ch == '.') || (!_tl_cis_ident(ch))) {
          tl_obj_ptr part = _tl_str_from_c(s, str + start, cur - start);
          if (part.t == tltNil) {
            _tl_obj_free(s, (tl_obj_ptr){.t = tltSymbol, .sym = sym}, 1);
            goto on_nem;
          }
          if (sym_empty) {
            sym_empty = 0;
          } else {
            tl_symbol *new_sym =
                s->alloc_vt->alloc(s->alloc, tlatSymStruct, sizeof(*sym));
            if (!new_sym) {
              _tl_obj_free(s, (tl_obj_ptr){.t = tltSymbol, .sym = sym}, 1);
              _tl_obj_free(s, part, 1);
              goto on_nem;
            }
            sym_last->next = new_sym;
            sym_last = new_sym;
          }
          sym_last->part = part.str;

          if (ch != '.') {
            sym_last->next = NULL;
            break;
          }

          start = cur + 1;
          cur = start + 1;
          continue;
        }
        cur += 1;
      }

      flag = 0;
      allocated = 1;
      append = 1;
      i--;

      to_append = (tl_obj_ptr){.t = tltSymbol, .sym = sym};

      continue;
    }
    case _tlr_str: { // String
      // TODO: escape sequences
      if (ch != '"')
        continue;
      // str[temp:i] is a String contents
      flag = 0;
      append = 1;
      allocated = 1;

      size_t len = i - temp;

      if (len == 0) {
        to_append = (tl_obj_ptr){.t = tltString, .str = NULL};
        continue;
      }

      // TODO: gc check for existing strings

      to_append = _tl_str_from_c(s, str + temp, len);

      if (to_append.t == tltNil) {
        goto on_nem;
      }

      continue;
    }
    case _tlr_lit: {
      // TODO: ascii char parse
      // TODO: all(unicode) char parse
      if (isalpha(ch))
        continue;
      flag = 0;
      int lit_len = i - temp;
      // TODO: invalid lit
      if ((lit_len == 4) && !(strncmp(str + temp, "true", 4))) {
        to_append = tlTrue;
      } else if ((lit_len == 5) && !(strncmp(str + temp, "false", 5))) {
        to_append = tlFalse;
      } else if ((lit_len == 3) && !(strncmp(str + temp, "nil", 3))) {
        to_append = tlNil;
      }
      append = 1;

      i--;
      continue;
    }
    case _tlr_nnt:
      if (ch != ' ') {
        tl_dlog("tl_read_raw met an unknown token starting with '.'");
        goto on_fatal;
      }
      if (depth == 0) {
        tl_dlog("tl_read_raw met tail specifier while not in list.");
        goto on_fatal;
      }
      if (tailed) {
        tl_dlog("tl_read_raw met tail specifier twice");
        goto on_fatal;
      }
      tailed = 1;
      flag = 0;
      continue;
    case _tlr_nos:
      // TODO: can symbol start with +<digit> or -<digit> ?
      if (isdigit(ch)) {
        flag = _tlr_num;
        i--;
        continue;
      } else if (isspace(ch) || _tl_cis_ident(ch)) {
        flag = _tlr_sym;
        i--;
        continue;
      }
      flag = 0;
      // TODO: invalid NOS
      continue;
    case _tlr_num: {
      // TODO: unsinged integer read
      // TODO: other forms
      if (isdigit(ch))
        continue;
      if (ch == '.') {
        if (real) {
          tl_dlog("tl_read_raw tried parsing a real number with multiple '.' "
                  "characters.");
          goto on_fatal;
        }
        real = 1;
        continue;
      } else if (_tl_cis_ident(ch)) {
        tl_dlog("tl_read_raw met an attempt to make symbol starting with a "
                "+<digit> or -<digit>");
        goto on_fatal;
      }
      int len = i - temp;
      // TODO: temp fix numbers
      if (len >= (sizeof(buf))) {
        tl_dlog("!!tl_read_raw number parse buffer reached limit, this is "
                "because of the "
                "temp fix!!");
        goto on_fatal;
      }
      memcpy(buf, str + temp, len);
      buf[len] = 0;
      // TODO: invalid Number
      flag = 0;
      if (real) {
        real = 0;
        to_append = (tl_obj_ptr){.t = tltDouble, .dbl = atof(buf)};
      } else {
        to_append = (tl_obj_ptr){.t = tltInteger, .intg = atoll(buf)};
      }
      append = 1;

      i--;
      continue;
    }
    }

    // flag is 0 here

    if (isspace(ch)) // skip all whitespace
      continue;

    switch (ch) {
    case '(': { // +Depth
      // TODO: temp fix
      if (depth == (sizeof(nodes) / sizeof(*nodes))) {
        tl_dlog("!!tl_read_raw depth reached limit, this is because of the "
                "temp fix!!");
        goto on_fatal;
      }

      tl_node *n = s->alloc_vt->alloc(s->alloc, tlatNode, sizeof(*n));
      n->head = tlNil;
      n->tail = tlNil;
      if (!n) {
        goto on_nem;
      }

      if (depth == 0) {
        node_top = n;
      } else {
        if (tailed == 2) {
          s->alloc_vt->free(s->alloc, tlatNode, n);
          tl_dlog("tl_read_raw found a list after node's tail was set "
                  "(something after the tail value)");
          goto on_fatal;
        }
        if (head_empty) {
          if (tailed) {
            tl_dlog("tl_read_raw met an attempt to set tail in a node without "
                    "head set");
            s->alloc_vt->free(s->alloc, tlatNode, n);
            goto on_fatal;
          }
          node_cur->head = (tl_obj_ptr){.t = tltNode, .node = n};
        } else {
          if (tailed) {
            node_cur->tail = (tl_obj_ptr){.t = tltNode, .node = n};
            tailed = 0;
          } else {
            tl_node *parent =
                s->alloc_vt->alloc(s->alloc, tlatNode, sizeof(*n));
            if (!parent) {
              s->alloc_vt->free(s->alloc, tlatNode, n);
              goto on_nem;
            }
            parent->head = (tl_obj_ptr){.t = tltNode, .node = n};
            parent->tail = tlNil;
            node_cur->tail = (tl_obj_ptr){.t = tltNode, .node = parent};
            nodes[depth - 1] = parent;
          }
        }
      }

      node_cur = n;
      head_empty = 1;

      nodes[depth] = n;

      depth++;
      continue;
    }
    case '#': // Nil, Boolean, Char, etc.
      flag = _tlr_lit;
      temp = i + 1;
      continue;
    case ')': // -Depth
      if (depth == 0) {
        tl_dlog("tl_read_raw met a stray ')', maybe a mistake?");
        goto on_fatal;
      }
      depth--;

      if (!tailed) {
        node_cur->tail = tlNil;
      } else {
        if (tailed == 1) {
          tl_dlog("tl_read_raw met a tail specifier, but not the tail value");
          goto on_fatal;
        }
        tailed = 0;
      }
      head_empty = 0;

      if (depth == 0) {
        to_append = (tl_obj_ptr){.t = tltNode, .node = node_top};
        allocated = 1;
        append = 1;
        /* if (node_top->head.t == tltNil) { */
        /*   tl_dlog("tl_read_raw met a '()' (empty list), which is
         * prohibited"); */
        /*   goto on_fatal; */
        /* } */
      } else {
        node_cur = nodes[depth - 1];
        tailed = ((node_cur->tail.t != tltNil) ? 2 : 0);
      }

      continue;
    case '+': // Number or Symbol
    case '-': // ^
      flag = _tlr_nos;
      temp = i;
      continue;
    case '"': // String
      flag = _tlr_str;
      temp = i + 1;
      continue;
    case '.': // Non-nil tail
      flag = _tlr_nnt;
      continue;
    }

    if (_tl_cis_ident_start(ch)) { // Symbol
      flag = _tlr_sym;
      temp = i;
      continue;
    } else if (isdigit(ch)) { // Number
      flag = _tlr_num;
      temp = i;
      continue;
    }

    tl_dlog("tl_read_raw met an unexpected character with value %d", ch);
    // Unexpected
    goto on_fatal;
  }

  if (depth) {
    tl_dlog("tl_read_raw met an unfinished list");
    goto on_fatal;
  }

  if (flag) {
    if (unfinished) { // if still can't finish
      tl_dlog("tl_read_raw couldn't finish parsing a token.");
      goto on_fatal;
    }
    // act as if str ends with an additional space char
    ch = ' ';
    unfinished = 1;
    i++;
    goto label_iteration;
  } else if (append) {
    goto label_append;
  }

  if (readen_out) {
    *readen_out = i;
  }

  return 0;
on_nem:
  tl_dlog("tl_read_raw didn't have enough memory.");
on_fatal:
  if (append) {
    _tl_obj_free(s, to_append, 1);
  }
  if (depth) {
    _tl_obj_free(s, (tl_obj_ptr){.t = tltNode, .node = node_top}, 1);
  }
  return -1;
}

int tl_read(struct tl_state *s) { return 0; }

// Two actual forms: function run, macro run
// All 'special forms' are macro runs (usually user functions)
inline static int _tl_eval_node(struct tl_state *s, tl_node *node,
                                tl_obj_ptr *ret) {
  return 0;
}

inline static int _tl_eval_sym(struct tl_state *s, tl_symbol *sym,
                               tl_obj_ptr *ret) {
  return 0;
}

// TODO: make not C-stack dependent
int tl_eval_raw(struct tl_state *s, tl_obj_ptr obj, tl_obj_ptr *ret) {
  switch (obj.t) {
    // Constants(literals) evaluate to themselves
  case tltNil:
  case tltBool:
  case tltChar:
  case tltInteger:
  case tltUInteger:
  case tltDouble:
  case tltString:
    if (ret)
      *ret = obj;
    break;
  case tltNode:
    return _tl_eval_node(s, obj.node, ret);
  case tltSymbol:
    return _tl_eval_sym(s, obj.sym, ret);
  default:
    tl_dlog("tl_eval raw met an attempt to evaluate a %s",
            tlaux_type_to_str(obj.t));
    return -1;
  }
  return 0;
}

int tl_eval(struct tl_state *s) { return 0; }

// Jenkin's one_at_a_time (Bob Jenkins)
unsigned long _tl_hash_func(const char *cstr, unsigned long len) {
  unsigned long hash = 0;
  for (unsigned long i = 0; i < len; i++) {
    hash += cstr[i];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

int _tl_env_cmp(tl_env_bucket *b1, tl_env_bucket *b2) {
  tl_str *s1 = b1->key->part;
  tl_str *s2 = b2->key->part;
  return (s1->len != s2->len) || (memcmp(s1->raw, s2->raw, s1->len));
}

int tl_env_insert(struct tl_state *s, struct tl_env *e, tl_symbol *key,
                  tl_obj_ptr val, tl_env_bucket **out) {
  if (key->next) {
    tl_dlog("tl_env_insert: tl_env can't accept multipart symbols (key->next "
            "!= NULL)");
    return -1;
  }
  unsigned long hash = _tl_hash_func(key->part->raw, key->part->len);
  tl_env_bucket *to_out = NULL;

  // Optimization: no need to allocate a new bucket
  if (!out) {
    tl_env_bucket search_bucket = (tl_env_bucket){.hash = hash, .key = key};

    tlht_get((tl_ht *)e, (tlht_bucket *)&search_bucket,
             (tlht_cmp_func *)_tl_env_cmp, (tlht_bucket **)&to_out);

    if (to_out) { // if an equivalent bucket is found, just replace the value
      if (to_out->val.t == val.t && to_out->val.user_ptr == val.user_ptr)
        return 0;
      return 0;
      if (tl_gc_unregister(
              s, to_out->val)) { // does nothing if 'val' isn't registered
        // ^ we have to call unreg because 'out' is NULL (cant return to the
        // caller)
        tl_dlog("tl_env_insert: tl_gc_unregister error");
        return -3;
      }
      to_out->val = val;
      return 0;
    }
  }

  tl_env_bucket *b = s->alloc_vt->alloc(s->alloc, tlatEnvBucket, sizeof(*b));

  if (!b) {
    tl_dlog("tl_env_insert: NEM");
    return -2;
  }

  b->hash = hash;
  b->key = key;
  b->val = val;

  if (tlht_insert((tl_ht *)e, (tlht_bucket *)b, (tlht_cmp_func *)_tl_env_cmp,
                  (tlht_bucket **)out)) {
    return -1;
  }

  // TODO: call tlht_fit

  return 0;
}

int tl_env_remove(struct tl_state *s, struct tl_env *e, tl_symbol *key,
                  tl_env_bucket **out) {
  unsigned long hash = _tl_hash_func(key->part->raw, key->part->len);
  tl_env_bucket search_bucket = (tl_env_bucket){.hash = hash, .key = key};

  // TODO: call tlht_fit

  return tlht_remove((tl_ht *)e, (tlht_bucket *)&search_bucket,
                     (tlht_cmp_func *)_tl_env_cmp, (tlht_bucket **)out);
}

int tl_env_get_here(struct tl_state *s, struct tl_env *e, tl_symbol *key,
                    tl_env_bucket **out) {
  unsigned long hash = _tl_hash_func(key->part->raw, key->part->len);
  tl_env_bucket search_bucket = (tl_env_bucket){.hash = hash, .key = key};

  return tlht_get((tl_ht *)e, (tlht_bucket *)&search_bucket,
                  (tlht_cmp_func *)_tl_env_cmp, (tlht_bucket **)out);
}

int tl_env_get(struct tl_state *s, struct tl_env *e, tl_symbol *key,
               tl_env_bucket **out) {
  unsigned long hash = _tl_hash_func(key->part->raw, key->part->len);
  tl_env_bucket search_bucket = (tl_env_bucket){.hash = hash, .key = key};

  tl_env_bucket *local_out = NULL;

  while (e != NULL) {
    if (tlht_get((tl_ht *)e, (tlht_bucket *)&search_bucket,
                 (tlht_cmp_func *)_tl_env_cmp, (tlht_bucket **)&local_out)) {
      tl_dlog("tl_env_get: tlht_get returned non-zero");
      return -1;
    }

    if (local_out) { // found
      if (out) {
        *out = local_out;
        return 0;
      }
    }

    // go to the parent env
    e = e->prev;
  }

  if (out) { // not found
    *out = NULL;
  }
  return 0;
}

int tl_env_set(struct tl_state *s, struct tl_env *e, tl_symbol *key,
               tl_obj_ptr obj, tl_obj_ptr *out) {
  tl_env_bucket *get_try = NULL;

  if (tl_env_get(s, e, key, &get_try)) {
    tl_dlog("tl_env_set: tl_env_get returned non-zero");
    return -1;
  }

  if (!get_try) { // not found, error
    return -1;
  }

  if (out) {
    *out = get_try->val;
  } else {
    if (get_try->val.t == obj.t && get_try->val.user_ptr == obj.user_ptr)
      return 0;
    // we have to call it, as 'out' is NULL
    if (tl_gc_unregister(s, get_try->val)) {
      tl_dlog("tl_env_set: tl_gc_unregister returned non-zero");
      return -1;
    }
  }

  get_try->val = obj;

  return 0;
}

unsigned long _tl_table_hash(tl_obj_ptr obj) {
  switch (obj.t) {
  case tltString:
    if (!obj.str)
      return 0;
    return _tl_hash_func(obj.str->raw, obj.str->len);
  case tltSymbol:
    if (obj.sym->next) {
      // error
      // TODO: raise
      tl_dlog("_tl_table_hash can't hash multipart symbols");
      return 666;
    }
    return _tl_hash_func(obj.sym->part->raw, obj.sym->part->len);
  case tltChar:
    return obj.ch;
  case tltBool:
    return obj.booln ? 1 : 0;
  case tltInteger:
  case tltUInteger: {
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
    // TODO: differentiate between 32bit and 64 bit (different hashing)
    unsigned long hash = ((obj.uintg >> 16) ^ obj.uintg) * 0x45d9f3bu;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3bu;
    hash = (hash >> 16) ^ hash;
    return obj.uintg;
  }
  default:
    // error
    // TODO: raise
    tl_dlog("_tl_table_hash can't hash %s", tlaux_type_to_str(obj.t));
    return 666;
  }

  return 0;
}

// TODO: move equality check to separate function
int _tl_table_cmp(tl_obj_ptr lhs, tl_obj_ptr rhs) {
  if (lhs.t != rhs.t)
    return 1;

  switch (lhs.t) {
  case tltString:
    return tl_str_cmp(lhs.str, rhs.str);
  case tltSymbol:
    if (lhs.sym->next || rhs.sym->next) {
      // error
      // TODO: raise
      tl_dlog("_tl_table_cmp can't compare multipart symbols");
      return 1;
    }
    return tl_str_cmp(lhs.sym->part, rhs.sym->part);
  case tltChar:
    return lhs.ch != rhs.ch;
  case tltBool:
    return lhs.booln != rhs.booln;
  case tltInteger:
    return lhs.intg != rhs.intg;
  case tltUInteger:
    return lhs.uintg != rhs.uintg;
  default:
    // error
    // TODO: raise
    tl_dlog("_tl_table_cmp can't compare %s and %s", tlaux_type_to_str(lhs.t),
            tlaux_type_to_str(rhs.t));
    return 666;
  }

  return 0;
}

int tl_table_insert(struct tl_state *s, struct tl_table *t, tl_obj_ptr key,
                    tl_obj_ptr val, tl_table_bucket **out) {
  if (!TL_TABLE_CAN_KEY(key.t)) {
    tl_dlog("tl_table_insert: %s can't be a table key",
            tlaux_type_to_str(key.t));
    return -1;
  }

  unsigned long hash = _tl_table_hash(key);

  if (!out) {
    tl_table_bucket *try = NULL;
    tl_table_bucket search_bucket = (tl_table_bucket){.hash = hash, .key = key};

    if (tlht_get((tl_ht *)t, (tlht_bucket *)&search_bucket,
                 (tlht_cmp_func *)_tl_table_cmp, (tlht_bucket **)&try)) {
      tl_dlog("tl_table_insert: tlht_get returned non-zero");
      return -1;
    }

    if (try) { // just replace vals
      if (try->val.t == val.t && try->val.user_ptr == val.user_ptr)
        return 0;
      if (tl_gc_unregister(s, try->val)) {
        tl_dlog("tl_table_insert: tl_gc_unregister returned non-zero");
        return -1;
      }
      try->val = val;
      return 0;
    }
  }

  tl_table_bucket *bucket =
      s->alloc_vt->alloc(s->alloc, tlatHtBucket, sizeof(*bucket));

  if (!bucket) {
    tl_dlog("tl_table_insert: NEM");
    return -2;
  }

  bucket->hash = hash;
  bucket->key = key;
  bucket->val = val;

  // TODO: call tlht_fit

  return tlht_insert((tl_ht *)t, (tlht_bucket *)bucket,
                     (tlht_cmp_func *)_tl_table_cmp, (tlht_bucket **)out);
}

int tl_table_remove(struct tl_state *s, struct tl_table *t, tl_obj_ptr key,
                    tl_table_bucket **out) {
  unsigned long hash = _tl_table_hash(key);
  tl_table_bucket search_bucket = (tl_table_bucket){.hash = hash, .key = key};

  // TODO: call tlht_fit

  return tlht_remove((tl_ht *)t, (tlht_bucket *)&search_bucket,
                     (tlht_cmp_func *)_tl_table_cmp, (tlht_bucket **)out);
}

int tl_table_get(struct tl_state *s, struct tl_table *t, tl_obj_ptr key,
                 tl_table_bucket **out) {
  unsigned long hash = _tl_table_hash(key);
  tl_table_bucket search_bucket = (tl_table_bucket){.hash = hash, .key = key};

  return tlht_get((tl_ht *)t, (tlht_bucket *)&search_bucket,
                  (tlht_cmp_func *)_tl_table_cmp, (tlht_bucket **)out);
}
