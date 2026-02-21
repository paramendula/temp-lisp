#include "libtlht.h"

// TODO: refactor lookup into an internal function (possibly inline)

int tlht_insert(tl_ht *ht, tlht_bucket *bucket, tlht_cmp_func *cmp,
                tlht_bucket **out) {
  unsigned long local_hash = bucket->hash % ht->cap;

  tlht_bucket *lookup = ht->buckets[local_hash];
  tlht_bucket *check = lookup;

  if (!lookup) {
    ht->buckets[local_hash] = bucket;

    bucket->next_col = 0;
  } else {
    // append as a collision
    while (check) {
      if ((bucket->hash == check->hash) &&
          !cmp(check,
               bucket)) { // if they are equivalent (key or fully) -
                          // replace
        break;
      }
      lookup = check;
      check = check->next_col;
    }

    if (check) { // if found equivalent
      if (out) {
        *out = check;
      } // else - memory leak
      if (lookup == check) { // if equivalent is first entry
        ht->buckets[local_hash] = bucket;
        bucket->next_col = lookup->next_col;
      } else {
        lookup->next_col = bucket;
        bucket->next_col = check->next_col;
      }

      bucket->prev = check->prev;
      bucket->next = check->next;

      if (bucket->prev) {
        bucket->prev->next = bucket;
      }

      if (bucket->next == 0) {
        ht->last = bucket;
      } else {
        bucket->next->prev = bucket;
      }

      return 0;
    } else { // if new entry
      if (out) {
        *out = 0;
      }
      lookup->next_col = bucket;
      bucket->next_col = 0;
    }
  }

  // Set 'bucket' as ht->last

  if (ht->len++ != 0) {
    ht->last->next = bucket;
    bucket->prev = ht->last;
  } else {
    bucket->prev = 0;
  }

  bucket->next = 0;
  ht->last = bucket;

  return 0;
}

int tlht_remove(tl_ht *ht, tlht_bucket *search_bucket, tlht_cmp_func *cmp,
                tlht_bucket **out) {
  unsigned long local_hash = search_bucket->hash % ht->cap;

  tlht_bucket *lookup = ht->buckets[local_hash];
  tlht_bucket *check = lookup;

  if (!lookup) {
    goto not_found;
  } else {
    while (check) {
      if ((search_bucket->hash == check->hash) &&
          !cmp(check, search_bucket)) { // if found
        break;
      }
      lookup = check;
      check = check->next_col;
    }

    if (check) { // if found equivalent
      if (out) {
        *out = check;
      }
      if (lookup == check) { // if equivalent is first entry
        ht->buckets[local_hash] = check->next_col;
      } else {
        lookup->next_col = check->next_col;
      }
    } else {
      goto not_found;
    }
  }

  // Remove bucket from the buckets list

  if (!check->next) {
    ht->last = check->prev;
  } else {
    check->next->prev = check->prev;
  }

  if (check->prev) {
    check->prev->next = check->next;
  }

  ht->len--;

  return 0;
not_found:
  return -1;
}

int tlht_get(tl_ht *ht, tlht_bucket *bucket, tlht_cmp_func *cmp,
             tlht_bucket **out) {
  unsigned long local_hash = bucket->hash % ht->cap;

  tlht_bucket *lookup = ht->buckets[local_hash];
  tlht_bucket *check = lookup;

  if (!lookup) {
    if (out)
      *out = 0;
    return 0;
  } else {
    while (check) {
      if ((bucket->hash == check->hash) && !cmp(check, bucket)) { // if found
        break;
      }
      lookup = check;
      check = check->next_col;
    }

    if (check) { // if found equivalent
      if (out) {
        *out = check;
      }
    } else {
      if (out)
        *out = 0;
      return 0;
    }
  }

  return 0;
}

// TODO: add more checks, rethink?
// TODO: extensive testing needed and simplification (possibly)
int tlht_fit(tl_ht *ht, double ratio_lower, double ratio_upper, double factor,
             void *allocator, tlht_alloc_func *alloc,
             tlht_bucket ***old_buckets) {
  double ratio = ((double)ht->len) / ht->cap;

  unsigned long temp_cap = ht->cap;
  unsigned long new_cap = temp_cap;
  double temp_ratio = ratio;

  if (factor <= 1.0)
    return -1;

  if (!old_buckets)
    return -1;

  if (ratio < ratio_lower) {
    if (ratio_lower < (double)0.0)
      return -1;
    else if (ratio_lower >= 1.0)
      return -1;

    while (temp_ratio < ratio_lower) {
      unsigned long old_cap = temp_cap;
      temp_cap = (unsigned long)(((double)temp_cap) / factor);
      if (temp_cap >= old_cap)
        temp_cap--;
      if (temp_cap == 0)
        return -1;
      temp_ratio = ((double)ht->len) / temp_cap;
    }

    if (temp_ratio <= ratio_upper) {
      new_cap = temp_cap;
    }
  } else if (ratio > ratio_upper) {
    if (ratio_upper > (double)1.0)
      return -1;
    else if (ratio_upper <= 0.0)
      return -1;

    while (temp_ratio > ratio_upper) {
      unsigned long old_cap = temp_cap;
      temp_cap = (unsigned long)(((double)temp_cap) * factor);
      if (temp_cap <= old_cap)
        temp_cap++;
      if (temp_cap == 0)
        return -1;
      temp_ratio = ((double)ht->len) / temp_cap;
    }

    if (temp_ratio >= ratio_lower) {
      new_cap = temp_cap;
    }
  }

  if (ht->cap == new_cap) {
    *old_buckets = 0;
    return 0;
  }

  if (!alloc)
    return -2;

  tlht_bucket **new_buckets = alloc(allocator, new_cap);

  if (!new_buckets)
    return -2;

  // Insert all buckets
  unsigned long local_hash;
  for (tlht_bucket *b = ht->last; b != 0; b = b->prev) {
    local_hash = b->hash % new_cap;
    b->next_col = new_buckets[local_hash];
    new_buckets[local_hash] = b;
  }

  *old_buckets = ht->buckets;

  ht->buckets = new_buckets;
  ht->cap = new_cap;

  return 0;
}
