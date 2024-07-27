#include "aasm/util.h"
#include <stdio.h>
#include <string.h>

void *emalloc(size_t size) {
  void *p = malloc(size);
  if (!p) { perror("Out of memory!"); exit(-1); }
  return p;
}

uint64_t hash(const char *str) {
    uint64_t hash = 5381;

    do {
      hash = ((hash << 5) + hash) + *str;
    } while (*str++);

    return hash;
}


hashset *hs_make(void) {
  hashset *res = NEW(hashset);
  res->size = 0;
  res->capacity = 8;
  res->buckets = NEW_A(struct _lke*, 8);
  memset(res->buckets, 0, sizeof(struct _lke*) * 8);
  return res;
}

uint8_t hs_contains(const hashset *set, const char *str) {
  uint64_t s_hash = hash(str);
  for (const struct _lke *bucket = set->buckets[s_hash % set->capacity]; bucket; bucket = bucket->next) {
    if (bucket->key == s_hash && !strcmp(str, bucket->str)) return 1;
  }
  return 0;
}

void hs_put(hashset *set, const char *str) {
  uint64_t s_hash = hash(str);
  struct _lke *bucket = NEW(struct _lke);
  bucket->next = set->buckets[s_hash % set->capacity];
  set->buckets[s_hash % set->capacity] = bucket;
  bucket->key = s_hash;
  bucket->str = str;
  set->size++;
  if (((double)set->size / (double)set->capacity) > 2) hs_rehash(set);
}

void hs_erase(hashset *set, const char *str) {
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

void hs_rehash(hashset *set) {
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

void hs_free(hashset *set) {
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


hashmap *hm_make(void) {
  hashmap *res = NEW(hashmap);
  res->size = 0;
  res->capacity = 8;
  res->buckets = NEW_A(struct _lkee*, 8);
  memset(res->buckets, 0, sizeof(struct _lkee*) * 8);
  return res;
}

uint8_t hm_contains(const hashmap *map, const char *str) {
  uint64_t s_hash = hash(str);
  for (const struct _lkee *bucket = map->buckets[s_hash % map->capacity]; bucket; bucket = bucket->next) {
    if (bucket->key == s_hash && !strcmp(str, bucket->str)) return 1;
  }
  return 0;
}

void hm_put(hashmap *map, const char *str, const char *value) {
  uint64_t s_hash = hash(str);
  struct _lkee *bucket = NEW(struct _lkee);
  bucket->next = map->buckets[s_hash % map->capacity];
  map->buckets[s_hash % map->capacity] = bucket;
  bucket->key = s_hash;
  bucket->str = str;
  bucket->value = value;
  map->size++;
  if (((double)map->size / (double)map->capacity) > 2) hm_rehash(map);
}

const char *hm_get(hashmap *map, const char *str) {
  uint64_t s_hash = hash(str);
  for (const struct _lkee *bucket = map->buckets[s_hash % map->capacity]; bucket; bucket = bucket->next) {
    if (bucket->key == s_hash && !strcmp(str, bucket->str)) return bucket->value;
  }
  return 0;
}

void hm_erase(hashmap *map, const char *str) {
  uint64_t s_hash = hash(str);
  for (struct _lkee **bucket = &(map->buckets[s_hash % map->capacity]); *bucket; bucket = &((*bucket)->next)) {
    if ((*bucket)->key == s_hash && (*bucket)->str == str) {
      struct _lkee *s_bucket = *bucket;
      *bucket = (*bucket)->next;
      free(s_bucket);
      break;
    }
  }
}

void hm_rehash(hashmap *map) {
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

void hm_free(hashmap *map) {
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


l_list *ll_make(void) {
  l_list *res = NEW(l_list);
  res->size = 0;
  res->data = 0;
  res->last = &res->data;
  return res;
}

void ll_append(l_list *list, const char *str) {
  *(list->last) = NEW(struct _le);
  (*(list->last))->str = str;
  (*(list->last))->next = 0;
  list->last = &((*(list->last))->next);
  list->size++;
}

void ll_prepend(l_list *list, const char *str) {
  struct _le *data = NEW(struct _le);
  data->str = str;
  data->next = list->data;
  list->data = data;
  list->size++;
  if (*list->last == list->data) list->last = &(data->next);
}

const char *ll_pop(l_list *list) {
  if (!list->size) return 0;
  list->size--;
  //WARN: Pointer "struct _le**" to "struct _le*" manipulation
  struct _le *data = (struct _le*) list->last;
  const char *str = data->str;
  *(list->last) = data->next;
  free(data);
  return str;
}

const char *ll_shift(l_list *list) {
  if (!list->size) return 0;
  list->size--;
  struct _le *data = list->data;
  const char *str = data->str;
  list->data = data->next;
  free(data);
  return str;
}

void ll_clear(l_list *list) {
  struct _le *entry = list->data, *s_entry; 
  while(entry) {
    s_entry = entry;
    entry = entry->next;
    free(s_entry);
  }
  list->data = 0;
  list->last = &list->data;
  list->size = 0;
}

void ll_free(l_list *list) {
  struct _le *entry = list->data, *s_entry; 
  while(entry) {
    s_entry = entry;
    entry = entry->next;
    free((void*)s_entry->str);
    free(s_entry);
  }
  free(list);
}
