#ifndef LIBTLHT_H_
#define LIBTLHT_H_

// TL Hash Table library
// General Hash Table structs and functions
// Mostly intended to be used with a wrapper (e.g. tl_env)
// Implement with linked buckets for collisions
// May also be used for Hash Sets

typedef struct tlht_bucket {
  unsigned long hash;
  struct tlht_bucket *prev, *next, *next_col;
} tlht_bucket;

// 0 if equivalent, any other value if not
typedef int(tlht_cmp_func)(tlht_bucket *lhs, tlht_bucket *rhs);

typedef tlht_bucket **(tlht_alloc_func)(void *allocator, unsigned long new_cap);

typedef struct tl_ht {
  unsigned int len, cap;
  struct tlht_bucket **buckets, *last;
} tl_ht;

// Insert bucket into ht. If an equivalent exists (their hash is equal and cmp
// == 0), the equivalent is replaced and sent to '*rep_out' (if it's not NULL)
int tlht_insert(tl_ht *, tlht_bucket *bucket, tlht_cmp_func *cmp,
                tlht_bucket **out);
// Remove bucket from ht, cmp(search_bucket, bucket) must be 0 and hashes equal,
// where bucket is the actual bucket. Returns -1 if no equivalent bucket was
// found. Also if 'out' isn't NULL, sets '*out' to the found bucket.
int tlht_remove(tl_ht *, tlht_bucket *search_bucket, tlht_cmp_func *cmp,
                tlht_bucket **out);
// Works just like tlht_remove, but without actually removing the bucket...
int tlht_get(tl_ht *, tlht_bucket *search_bucket, tlht_cmp_func *cmp,
             tlht_bucket **out);

// Important! Freeing previous buckets array is caller's duty.
//
//
// Fit ht's cap so that ratio_lower <= len / cap <= ratio_upper (if possible
// with provided factor). Then resize buckets** and rearrange the buckets. Won't
// do anything if ht fits the ratios. e.g. 0.5 and 0.75 are good ratios to check
// for after insertion; 0.25 0.5 - after removing. Ensure that ratio_lower <
// ratio_upper. Set ratio_lower to 0.0 to never shrink, set ratio_upper to 1.0
// to never expand.
//
//
// The fitting process uses the factor variable, multiplying or diving cap by
// it. It must always be > 1.0.
// 2.0 is a good factor for general usage. If
// factor < 2.0, then you must ensure ht cap is big enough that ((unsigned
// long)(((double)cap) * factor)) > cap, to prevent infinite loops
//
//
// Returns -1 if something's wrong with ratios or the factor (check libtlht.c)
// Returns -2 if there is an allocation error (alloc() NULL or alloc() returned
// NULL)
// TODO: factor constraints, etc.
int tlht_fit(tl_ht *, double ratio_lower, double ratio_upper, double factor,
             void *allocator, tlht_alloc_func *alloc);

#endif
