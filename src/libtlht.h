#ifndef LIBTLHT_H_
#define LIBTLHT_H_

// TL Hash Table library
// General Hash Table structs and functions
// Mostly intended to be used with a wrapper (e.g. tl_env)

typedef struct tlht_bucket {
  unsigned long hash;
  struct tl_ht_bucket *prev, *next, *next_col;
} tlht_bucket;

// 0 if equal (usually keys), any other value if not equal
typedef int(tlht_cmp_func)(tlht_bucket *lhs, tlht_bucket *rhs);

typedef tlht_bucket **(tlht_alloc_func)(void *allocator, unsigned long new_cap);

typedef struct tl_ht {
  unsigned int len, cap;
  struct tlht_bucket **buckets, *first;
} tl_ht;

int tlht_insert(tl_ht *, tlht_bucket *bucket, tlht_cmp_func *cmp);
int tlht_remove(tl_ht *, tlht_bucket *bucket, tlht_cmp_func *cmp);
int tlht_get(tl_ht *, tlht_bucket *query, tlht_cmp_func *cmp,
             tlht_bucket **out);

// fit ht's cap so that ratio_lower <= len / cap <= ratio_upper. Then resize
// buckets** and rearrange the buckets. Won't do anything if ht fits the ratios.
// e.g. 0.5 and 0.75 are good ratios to check for after insertion; 0.25 0.5 -
// after removing
int tlht_fit(tl_ht *, double ratio_lower, double ratio_upper, void *allocator,
             tlht_alloc_func *alloc);

#endif
