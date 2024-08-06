#ifndef AMALGAM_ASM_UTIL
#define AMALGAM_ASM_UTIL

#include <stdlib.h>
#include <stdint.h>

typedef struct _ll llist_t;
typedef struct _hm hashmap_t;
typedef struct _hs hashset_t;
typedef struct _fa array_t;

#define NEW(type) (type*) emalloc(sizeof(type))
#define NEW_A(type, size) (type*) emalloc(sizeof(type) * size)
#define E_FOR(entry, from) for (struct _le *entry = from; entry; entry = entry->next)

struct _le {
  struct _le *next;
  void *value;
};

struct _lke {
  struct _lke *next;
  uint64_t key;
  const char *str;
};

struct _lkee {
  struct _lkee *next;
  uint64_t key;
  const char *str;
  void *value;
};

struct _ll {
  size_t size;
  struct _le **last;
  struct _le *data;
};

struct _hs {
  size_t capacity;
  size_t size;
  struct _lke **buckets;
};

struct _hm {
  size_t capacity;
  size_t size;
  struct _lkee **buckets;
};

struct _fa {
  size_t size;
  void **data;
};

void *emalloc(size_t size);
uint64_t hash(const char *str);

hashset_t *hs_make(void);
uint8_t hs_contains(const hashset_t *set, const char *str);
//WARNING: calls free() on the string during hs_free(), but not during hs_clear()
void hs_put(hashset_t *set, const char *str);
void hs_erase(hashset_t *set, const char *str);
void hs_rehash(hashset_t *set);
void hs_free(hashset_t *set);
void hs_free_val(hashset_t *set);

hashmap_t *hm_make(void);
//WARNING: calls free() on the key and value during hm_free(), but not during hm_clear()
void *hm_put(hashmap_t *map, const char *str, void *val);
struct _lkee *hm_get(hashmap_t *map, const char *str);
struct _lkee *hm_erase(hashmap_t *map, const char *str);
void hm_rehash(hashmap_t *map);
void hm_free(hashmap_t *map);
void hm_free_val(hashmap_t *map);
llist_t *hm_free_to(hashmap_t *map);

llist_t *ll_make(void);
//WARNING: calls free() on the value during ll_free(), but not during ll_clear()
void ll_append(llist_t *list, void *value);
//WARNING: calls free() on the value during ll_free(), but not during ll_clear()
void ll_prepend(llist_t *list, void *value);
void *ll_pop(llist_t *list);
void *ll_shift(llist_t *list);
void ll_free(llist_t *list);
void ll_free_val(llist_t *list);
array_t *ll_free_to(llist_t *list);

array_t *ar_make(size_t size);
void *ar_set(array_t *array, size_t index, void *value);
void *ar_get(array_t *array, size_t index);
const void *ar_cget(const array_t *array, size_t index);
void ar_cutout(array_t *array, size_t start, size_t end);
void ar_free(array_t *array);
void ar_free_val(array_t *array);

#endif
