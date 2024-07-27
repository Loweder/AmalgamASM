#include <stdlib.h>
#include <stdint.h>

typedef struct _ll l_list;
typedef struct _hm hashmap;
typedef struct _hs hashset;

#define NEW(type) (type*) emalloc(sizeof(type))
#define NEW_A(type, size) (type*) emalloc(sizeof(type) * size)

struct _le {
  struct _le *next;
  const char *str;
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
  const char *value;
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

void *emalloc(size_t size);
uint64_t hash(const char *str);

hashset *hs_make(void);
uint8_t hs_contains(const hashset *set, const char *str);
//WARNING: calls free() on the string during hs_free()
void hs_put(hashset *set, const char *str);
void hs_erase(hashset *set, const char *str);
void hs_rehash(hashset *set);
void hs_free(hashset *set);

hashmap *hm_make(void);
uint8_t hm_contains(const hashmap *map, const char *str);
//WARNING: calls free() on the string during hs_free()
void hm_put(hashmap *map, const char *str, const char *val);
const char *hm_get(hashmap *map, const char *str);
void hm_erase(hashmap *map, const char *str);
void hm_rehash(hashmap *map);
void hm_free(hashmap *map);

l_list *ll_make(void);
//WARNING: calls free() on the string during ll_free(), but not during ll_clear()
void ll_append(l_list *list, const char *str);
//WARNING: calls free() on the string during ll_free(), but not during ll_clear()
void ll_prepend(l_list *list, const char *str);
const char *ll_pop(l_list *list);
const char *ll_shift(l_list *list);
void ll_clear(l_list *list);
void ll_free(l_list *list);

