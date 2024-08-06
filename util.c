#include "aasm/util.h"
#include <stdio.h>
#include <string.h>

void *emalloc(size_t size) {
  void *p = malloc(size);
  if (!p) { perror("Out of memory"); exit(-1); }
  return p;
}

uint64_t hash(const char *str) {
    uint64_t hash = 5381;

    do {
      hash = ((hash << 5) + hash) + *str;
    } while (*str++);

    return hash;
}


hashset_t *hs_make(void) {
  hashset_t *result = NEW(hashset_t);
  result->size = 0;
  result->capacity = 8;
  result->buckets = NEW_A(struct _lke*, 8);
  memset(result->buckets, 0, sizeof(struct _lke*) * 8);
  return result;
}

uint8_t hs_contains(const hashset_t *set, const char *str) {
  uint64_t s_hash = hash(str);
  for (const struct _lke *bucket = set->buckets[s_hash % set->capacity]; bucket; bucket = bucket->next) {
    if (bucket->key == s_hash && !strcmp(str, bucket->str)) return 1;
  }
  return 0;
}

void hs_put(hashset_t *set, const char *str) {
  uint64_t s_hash = hash(str);
  struct _lke *bucket = NEW(struct _lke);
  bucket->next = set->buckets[s_hash % set->capacity];
  set->buckets[s_hash % set->capacity] = bucket;
  bucket->key = s_hash;
  bucket->str = str;
  set->size++;
  if (((double)set->size / (double)set->capacity) > 2) hs_rehash(set);
}

void hs_erase(hashset_t *set, const char *str) {
  uint64_t s_hash = hash(str);
  for (struct _lke **bucket = &(set->buckets[s_hash % set->capacity]); *bucket; bucket = &((*bucket)->next)) {
    if ((*bucket)->key == s_hash && (*bucket)->str == str) {
      struct _lke *s_bucket = *bucket;
      *bucket = (*bucket)->next;
      free(s_bucket);
      break;
    }
  }
}

void hs_rehash(hashset_t *set) {
  size_t new_capacity = (size_t)((double)set->size / 0.75L);
  struct _lke **new_buckets = NEW_A(struct _lke*, new_capacity);
  memset(new_buckets, 0, sizeof(struct _lke*) * new_capacity);
  for (int i = 0; i < set->capacity; i++) {
    struct _lke *bucket = set->buckets[i], *s_bucket;
    size_t index;
    while (bucket) {
      s_bucket = bucket->next;
      index = bucket->key % new_capacity;
      bucket->next = new_buckets[index];
      new_buckets[index] = bucket;
      bucket = s_bucket;
    }
  }
  free(set->buckets);
  set->buckets = new_buckets;
  set->capacity = new_capacity;
}

void hs_free(hashset_t *set) {
  if (!set) return;
  for (int i = 0; i < set->capacity; i++) {
    struct _lke *bucket = set->buckets[i], *s_bucket;
    while (bucket) {
      s_bucket = bucket;
      bucket = bucket->next;
      free(s_bucket);
    }
  }
  free(set->buckets);
  free(set);
}

void hs_free_val(hashset_t *set) {
  if (!set) return;
  for (int i = 0; i < set->capacity; i++) {
    struct _lke *bucket = set->buckets[i], *s_bucket;
    while (bucket) {
      s_bucket = bucket;
      bucket = bucket->next;
      free((void*)s_bucket->str);
      free(s_bucket);
    }
  }
  free(set->buckets);
  free(set);
}


hashmap_t *hm_make(void) {
  hashmap_t *result = NEW(hashmap_t);
  result->size = 0;
  result->capacity = 8;
  result->buckets = NEW_A(struct _lkee*, 8);
  memset(result->buckets, 0, sizeof(struct _lkee*) * 8);
  return result;
}

struct _lkee *hm_get(hashmap_t *map, const char *str) {
  uint64_t s_hash = hash(str);
  for (struct _lkee *bucket = map->buckets[s_hash % map->capacity]; bucket; bucket = bucket->next) {
    if (bucket->key == s_hash && !strcmp(str, bucket->str)) return bucket;
  }
  return 0;
}

void *hm_put(hashmap_t *map, const char *str, void *value) {
  struct _lkee *bucket = hm_get(map, str);
  if (bucket) {
    void *old_data = bucket->value;
    bucket->value = value;
    return old_data;
  }

  uint64_t s_hash = hash(str);
  bucket = NEW(struct _lkee);
  bucket->next = map->buckets[s_hash % map->capacity];
  map->buckets[s_hash % map->capacity] = bucket;
  bucket->key = s_hash;
  bucket->str = str;
  bucket->value = value;
  map->size++;
  if (((double)map->size / (double)map->capacity) > 2) hm_rehash(map);
  return 0;
}

struct _lkee *hm_erase(hashmap_t *map, const char *str) {
  uint64_t s_hash = hash(str);
  for (struct _lkee **bucket = &(map->buckets[s_hash % map->capacity]); *bucket; bucket = &((*bucket)->next)) {
    if ((*bucket)->key == s_hash && (*bucket)->str == str) {
      struct _lkee *s_bucket = *bucket;
      *bucket = s_bucket->next;
      map->size--;
      return s_bucket;
    }
  }
  return 0;
}

void hm_rehash(hashmap_t *map) {
  size_t new_capacity = (size_t)((double)map->size / 0.75L);
  struct _lkee **new_buckets = NEW_A(struct _lkee*, new_capacity);
  memset(new_buckets, 0, sizeof(struct _lkee*) * new_capacity);
  for (int i = 0; i < map->capacity; i++) {
    struct _lkee *bucket = map->buckets[i], *s_bucket;
    size_t index;
    while (bucket) {
      s_bucket = bucket->next;
      index = bucket->key % new_capacity;
      bucket->next = new_buckets[index];
      new_buckets[index] = bucket;
      bucket = s_bucket;
    }
  }
  free(map->buckets);
  map->buckets = new_buckets;
  map->capacity = new_capacity;
}

void hm_free(hashmap_t *map) {
  if (!map) return;
  for (int i = 0; i < map->capacity; i++) {
    struct _lkee *bucket = map->buckets[i], *s_bucket;
    while (bucket) {
      s_bucket = bucket;
      bucket = bucket->next;
      free(s_bucket);
    }
  }
  free(map->buckets);
  free(map);
}

void hm_free_val(hashmap_t *map) {
  if (!map) return;
  for (int i = 0; i < map->capacity; i++) {
    struct _lkee *bucket = map->buckets[i], *s_bucket;
    while (bucket) {
      s_bucket = bucket;
      bucket = bucket->next;
      free((void*)s_bucket->str);
      free((void*)s_bucket->value);
      free(s_bucket);
    }
  }
  free(map->buckets);
  free(map);
}

llist_t *hm_free_to(hashmap_t *map) {
  if (!map) return 0;
  llist_t *result = ll_make();
  for (int i = 0; i < map->capacity; i++) {
    struct _lkee *bucket = map->buckets[i], *s_bucket;
    while (bucket) {
      llist_t *entry = ll_make();
      ll_append(result, entry);
      s_bucket = bucket;
      bucket = bucket->next;
      ll_append(entry, (char*) s_bucket->str);
      ll_append(entry, s_bucket->value);
      free(s_bucket);
    }
  }
  free(map->buckets);
  free(map);
  return result;
}


llist_t *ll_make(void) {
  llist_t *result = NEW(llist_t);
  result->size = 0;
  result->data = 0;
  result->last = &result->data;
  return result;
}

void ll_append(llist_t *list, void *value) {
  *(list->last) = NEW(struct _le);
  (*(list->last))->value = value;
  (*(list->last))->next = 0;
  list->last = &((*(list->last))->next);
  list->size++;
}

void ll_prepend(llist_t *list, void *value) {
  struct _le *data = NEW(struct _le);
  data->value = value;
  data->next = list->data;
  list->data = data;
  list->size++;
  if (*list->last == list->data) list->last = &(data->next);
}

void *ll_pop(llist_t *list) {
  if (!list->size) return 0;
  list->size--;
  for (struct _le **data = &list->data; *data; data = &((*data)->next)) {
    if (!(*data)->next) {
      struct _le *r_data = *data;
      void *value = r_data->value;
      list->last = data;
      *data = 0;
      free(r_data);
      return value;
    }
  }
  return 0;
}

void *ll_shift(llist_t *list) {
  if (!list->size) return 0;
  list->size--;
  if (list->size == 0) list->last = &list->data;
  struct _le *data = list->data;
  void *value = data->value;
  list->data = data->next;
  free(data);
  return value;
}

void ll_free(llist_t *list) {
  if (!list) return;
  struct _le *entry = list->data, *s_entry; 
  while(entry) {
    s_entry = entry;
    entry = entry->next;
    free(s_entry);
  }
  free(list);
}

void ll_free_val(llist_t *list) {
  if (!list) return;
  struct _le *entry = list->data, *s_entry; 
  while(entry) {
    s_entry = entry;
    entry = entry->next;
    free((void*)s_entry->value);
    free(s_entry);
  }
  free(list);
}

array_t *ll_free_to(llist_t *list) {
  if (!list) return 0;
  array_t *result = ar_make(list->size);
  int i = 0;
  struct _le *entry = list->data, *s_entry; 
  while(entry) {
    ar_set(result, i, entry->value);
    s_entry = entry;
    entry = entry->next;
    free(s_entry);
    i++;
  }
  free(list);
  return result;
}


array_t *ar_make(size_t size) {
  if (size < 0) return 0;
  array_t *result = NEW(array_t);
  result->size = size;
  result->data = NEW_A(void*, size);
  return result;
}

void *ar_set(array_t *array, size_t index, void *value) {
  if (array->size <= index) return 0;
  void *old = array->data[index];
  array->data[index] = value;
  return old;
}

void *ar_get(array_t *array, size_t index) {
  if (array->size <= index) return 0;
  return array->data[index];
}

const void *ar_cget(const array_t *array, size_t index) {
  if (array->size <= index) return 0;
  return array->data[index];
}

void ar_cutout(array_t *array, size_t start, size_t end) {
  if (array->size < end) return;
  void **data = NEW_A(void*, (end-start));
  for (int i = 0; i < array->size; i++) {
    if (i < start || i >= end) {
      free(array->data[i]);
    } else {
      data[i-start] = array->data[i];
    }
  }
  free(array->data);
  array->size = end-start;
  array->data = data;
}

void ar_free(array_t *array) {
  if (!array) return;
  free(array->data);
  free(array);
}

void ar_free_val(array_t *array) {
  if (!array) return;
  for (size_t i = 0; i < array->size; i++) {
    free(array->data[i]);
  }
  free(array->data);
  free(array);
}
