/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Gabriel de Quadros Ligneul
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef typesafeccontainers_h
#define typesafeccontainers_h

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Iterators naming convention. */
#ifndef TSCC_ITERATOR
#define TSCC_ITERATOR(T) T##Iterator
#endif

/* Modifier for static functions */
#define TSCC_INLINE static __inline

/* Default reallocation function. */
#define tscc_realloc_default(allocator, ptr, oldsize, newsize)                 \
  ((newsize != 0) ? (realloc(ptr, newsize)) : (free(ptr), NULL))

/* Default assert function
 * Redefine it before the include to change the assert behavior. */
#ifndef tscc_assert
#ifdef NDEBUG
#include <assert.h>
#include <stdio.h>
#define tscc_assert(expression, message)                                       \
  do {                                                                         \
    if (!(expression))                                                         \
      fprintf(stderr, "Error: %s\n", message);                                 \
    assert(expression);                                                        \
  } while (0)
#else
#define tscc_assert(expression, message) (void)0
#endif
#endif

/* Vector declaration start */
#define TSCC_DECL_VECTOR(typename, pref, T)                                    \
  TSCC_DECL_VECTOR_WA(typename, pref, T, void *)

#define TSCC_DECL_VECTOR_WA(typename, pref, T, AllocatorType)                  \
  struct typename {                                                            \
    AllocatorType allocator;                                                   \
    T *buffer;                                                                 \
    size_t size;                                                               \
    size_t capacity;                                                           \
  };                                                                           \
  typedef struct typename typename;                                            \
  typename *pref##create(void);                                                \
  typename *pref##createwa(AllocatorType allocator);                           \
  void pref##destroy(typename *v);                                             \
  int pref##empty(typename *v);                                                \
  size_t pref##size(typename *v);                                              \
  void pref##resize(typename *v, size_t newsize);                              \
  void pref##push(typename *v, T value);                                       \
  T pref##pop(typename *v);                                                    \
  void pref##insert(typename *v, size_t pos, T value);                         \
  void pref##erase(typename *v, size_t pos);                                   \
  T pref##get(typename *v, size_t pos);                                        \
                                                                               \
  /* Get a reference to the value. */                                          \
  TSCC_INLINE T *pref##getref(typename *v, size_t pos) {                       \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(pos < v->size, "out of bounds");                               \
    return v->buffer + pos;                                                    \
  }                                                                            \
                                                                               \
  void pref##set(typename *v, size_t pos, T value);                            \
  T pref##front(typename *v);                                                  \
  T pref##back(typename *v);                                                   \
  T *pref##data(typename *v);

#define TSCC_IMPL_VECTOR(typename, pref, T)                                    \
  TSCC_IMPL_VECTOR_WA(typename, pref, T, void *, tscc_realloc_default)

#define TSCC_IMPL_VECTOR_WA(typename, pref, T, AllocatorType,                  \
                            realloc_function)                                  \
                                                                               \
  static void pref##resizevector(typename *v, size_t newcapacity) {            \
    v->buffer =                                                                \
        realloc_function(v->allocator, v->buffer, v->capacity * sizeof(T),     \
                         newcapacity * sizeof(T));                             \
    v->capacity = newcapacity;                                                 \
  }                                                                            \
  static void pref##growvector(typename *v) {                                  \
    if (v->capacity == 0)                                                      \
      pref##resizevector(v, 1);                                                \
    else if (v->size + 1 > v->capacity)                                        \
      pref##resizevector(v, v->capacity * 2);                                  \
  }                                                                            \
  typename *pref##create(void) { return pref##createwa(NULL); }                \
  typename *pref##createwa(AllocatorType allocator) {                          \
    typename *v = realloc_function(allocator, NULL, 0, sizeof(typename));      \
    v->allocator = allocator;                                                  \
    v->buffer = NULL;                                                          \
    v->size = v->capacity = 0;                                                 \
    pref##resizevector(v, 8);                                                  \
    return v;                                                                  \
  }                                                                            \
  void pref##destroy(typename *v) {                                            \
    if (!v)                                                                    \
      return;                                                                  \
    (void)realloc_function(v->allocator, v->buffer, v->capacity * sizeof(T),   \
                           0);                                                 \
    (void)realloc_function(v->allocator, v, sizeof(typename), 0);              \
  }                                                                            \
  int pref##empty(typename *v) {                                               \
    tscc_assert(v, "null vector");                                             \
    return v->size == 0;                                                       \
  }                                                                            \
  size_t pref##size(typename *v) {                                             \
    tscc_assert(v, "null vector");                                             \
    return v->size;                                                            \
  }                                                                            \
  void pref##resize(typename *v, size_t newsize) {                             \
    tscc_assert(v, "null vector");                                             \
    if (newsize > v->capacity)                                                 \
      pref##resizevector(v, newsize);                                          \
    v->size = newsize;                                                         \
  }                                                                            \
  void pref##push(typename *v, T value) {                                      \
    tscc_assert(v, "null vector");                                             \
    pref##growvector(v);                                                       \
    v->buffer[v->size++] = value;                                              \
  }                                                                            \
  T pref##pop(typename *v) {                                                   \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(v->size > 0, "empty vector");                                  \
    return v->buffer[--v->size];                                               \
  }                                                                            \
  void pref##insert(typename *v, size_t pos, T value) {                        \
    size_t i;                                                                  \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(pos <= v->size, "out of bounds");                              \
    pref##growvector(v);                                                       \
    for (i = v->size; i > pos; --i)                                            \
      v->buffer[i] = v->buffer[i - 1];                                         \
    v->buffer[pos] = value;                                                    \
    v->size++;                                                                 \
  }                                                                            \
  void pref##erase(typename *v, size_t pos) {                                  \
    size_t i;                                                                  \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(pos <= v->size, "out of bounds");                              \
    for (i = pos; i < v->size; ++i)                                            \
      v->buffer[i] = v->buffer[i + 1];                                         \
    v->size--;                                                                 \
  }                                                                            \
  T pref##get(typename *v, size_t pos) {                                       \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(pos < v->size, "out of bounds");                               \
    return v->buffer[pos];                                                     \
  }                                                                            \
  void pref##set(typename *v, size_t pos, T value) {                           \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(pos < v->size, "out of bounds");                               \
    v->buffer[pos] = value;                                                    \
  }                                                                            \
  T pref##front(typename *v) {                                                 \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(v->size > 0, "empty vector");                                  \
    return v->buffer[0];                                                       \
  }                                                                            \
  T pref##back(typename *v) {                                                  \
    tscc_assert(v, "null vector");                                             \
    tscc_assert(v->size > 0, "empty vector");                                  \
    return v->buffer[v->size - 1];                                             \
  }                                                                            \
  T *pref##data(typename *v) { return v->buffer; }

/* Iterates throght the vector and executes the command */
#define TSCC_VECTOR_FOREACH(prefix, v, T, t, cmd)                              \
  {                                                                            \
    size_t _i_##t;                                                             \
    size_t _n_##t = prefix##size(v);                                           \
    for (_i_##t = 0; _i_##t < _n_##t; ++_i_##t) {                              \
      T t = prefix##get(v, _i_##t);                                            \
      (void)t;                                                                 \
      cmd;                                                                     \
    }                                                                          \
  }


/* HashTable max load factor. */
#ifndef TSCC_HASH_LOAD
#define TSCC_HASH_LOAD 0.6
#endif

/* GNU C extension used to detect the pointer size. */
#if __SIZEOF_POINTER__ && __SIZEOF_POINTER__ == 4
#define TSCC_ENABLE_32BITS
#endif

/* FNV constants. */
#ifdef TSCC_ENABLE_32BITS
#define TSCC_FNV_OFFSET 0x811c9dc5
#define TSCC_FNV_PRIME 0x01000193
#else
#define TSCC_FNV_OFFSET 0xcbf29ce484222325
#define TSCC_FNV_PRIME 0x00000100000001b3
#endif

/* FNV-1a hash function for strings. */
static __inline size_t tscc_str_hashfunc(const char *str) {
  if (str != NULL) {
    size_t i, hash = TSCC_FNV_OFFSET;
    for (i = 0; str[i] != '\0'; ++i) {
      hash ^= str[i];
      hash *= TSCC_FNV_PRIME;
    }
    return hash;
  } else {
    return 0;
  }
}

/* Declare a general hash function and use the FNV-1a hash function. */
#define TSCC_DECL_GENERAL_HASHFUNC(T, prefix)                                  \
  static __inline size_t prefix##hashfunc(T value) {                           \
    char mem[sizeof(value)];                                                   \
    size_t i, hash = TSCC_FNV_OFFSET;                                          \
    memcpy(mem, &value, sizeof(value));                                        \
    for (i = 0; i < sizeof(T); ++i) {                                          \
      hash ^= mem[i];                                                          \
      hash *= TSCC_FNV_PRIME;                                                  \
    }                                                                          \
    return hash;                                                               \
  }

/* Specific hash functions declarations. */
TSCC_DECL_GENERAL_HASHFUNC(long int, tscc_int_)
TSCC_DECL_GENERAL_HASHFUNC(double, tscc_float_)
TSCC_DECL_GENERAL_HASHFUNC(void *, tscc_ptr_)

/* Hash compare function for strings. */
static __inline int tscc_str_compare(const char *a, const char *b) {
  if (!a || !b) {
    if (a || b)
      return 0;
    else
      return 1;
  } else {
    return !strcmp(a, b);
  }
}

/* Hash compare function for primitive types */
#define tscc_general_compare(a, b) ((a) == (b))


#define TSCC_DECL_HASHTABLE(HashTable, pref, Key, Value)                       \
  TSCC_DECL_HASHTABLE_WA(HashTable, pref, Key, Value, void *)

#define TSCC_DECL_HASHTABLE_WA(HashTable, pref, Key, Value, AllocatorType)     \
                                                                               \
  typedef struct HashTable HashTable;                                          \
                                                                               \
  typedef struct TSCC_ITERATOR(HashTable) {                                    \
    size_t pos;                                                                \
  } TSCC_ITERATOR(HashTable);                                                  \
                                                                               \
  HashTable *pref##create(size_t nelements);                                   \
  HashTable *pref##createwa(size_t nelements, AllocatorType allocator);        \
  void pref##destroy(HashTable *h);                                            \
  void pref##clear(HashTable *h);                                              \
  HashTable *pref##clone(HashTable* h);                                        \
  int pref##empty(HashTable *h);                                               \
  size_t pref##size(HashTable *h);                                             \
  size_t pref##maxsize(HashTable *h);                                          \
  void pref##insert(HashTable *h, Key key, Value value);                       \
  int pref##tryinsert(HashTable *h, Key key, Value value);                     \
  int pref##find(HashTable *h, Key key, Value *value);                         \
  int pref##contains(HashTable *h, Key key);                                   \
  Value pref##get(HashTable *h, Key key, Value def);                           \
  int pref##erase(HashTable *h, Key key, Value *value);                        \
                                                                               \
  /* Create an iterator that points to the first entry. */                     \
  TSCC_ITERATOR(HashTable) pref##begin(HashTable *h);                          \
                                                                               \
  /* Move the iterator to the next entry. */                                   \
  TSCC_ITERATOR(HashTable)                                                     \
  pref##next(HashTable *h, TSCC_ITERATOR(HashTable) it);                       \
                                                                               \
  /* Verify if the iterator didn't reach the end. */                           \
  int pref##valid(HashTable *h, TSCC_ITERATOR(HashTable) it);                  \
                                                                               \
  /* Obtain the key given the iterator. */                                     \
  Key pref##key(HashTable *h, TSCC_ITERATOR(HashTable) it);                    \
                                                                               \
  /* Obtain the value given the iterator. */                                   \
  Value pref##value(HashTable *h, TSCC_ITERATOR(HashTable) it);

#define TSCC_IMPL_HASHTABLE(HashTable, pref, Key, Value, hashfunc, keyequal)   \
  TSCC_IMPL_HASHTABLE_WA(HashTable, pref, Key, Value, hashfunc, keyequal,      \
                         void *, tscc_realloc_default)

#define TSCC_IMPL_HASHTABLE_WA(HashTable, pref, Key, Value, hashfunc,          \
                               keyequal, AllocatorType, realloc_function)      \
                                                                               \
  struct HashTable {                                                           \
    AllocatorType allocator;                                                   \
    Key *keys;                                                                 \
    Value *values;                                                             \
    unsigned char *usedpos;                                                    \
    size_t size;                                                               \
    size_t capacity;                                                           \
  };                                                                           \
                                                                               \
  static size_t pref##getusedpossize(size_t capacity) {                        \
    return sizeof(unsigned char) * (capacity / 8 + ((capacity % 8) != 0));     \
  }                                                                            \
                                                                               \
  static int pref##getusedpos(HashTable *h, size_t pos) {                      \
    unsigned char mask = 1 << (pos % 8);                                       \
    return !!(h->usedpos[pos / 8] & mask);                                     \
  }                                                                            \
                                                                               \
  static void pref##setusedpos(HashTable *h, size_t pos, int used) {           \
    unsigned char mask = 1 << (pos % 8);                                       \
    size_t cellid = pos / 8;                                                   \
    if (used)                                                                  \
      h->usedpos[cellid] |= mask;                                              \
    else                                                                       \
      h->usedpos[cellid] &= ~mask;                                             \
  }                                                                            \
                                                                               \
  static size_t pref##computerequiredcapacity(size_t size) {                   \
    return size / TSCC_HASH_LOAD;                                              \
  }                                                                            \
                                                                               \
  static size_t pref##hash(HashTable *h, Key key) {                            \
    return hashfunc(key) % h->capacity;                                        \
  }                                                                            \
                                                                               \
  static size_t pref##getposition(HashTable *h, Key key, int *found) {         \
    size_t i;                                                                  \
    size_t pos = pref##hash(h, key);                                           \
    for (i = 0; i < h->size + 1; ++i) {                                        \
      int usedpos = pref##getusedpos(h, pos);                                  \
      if (!usedpos) {                                                          \
        *found = 0;                                                            \
        break;                                                                 \
      } else if (usedpos && keyequal(key, h->keys[pos])) {                     \
        *found = 1;                                                            \
        break;                                                                 \
      } else {                                                                 \
        pos = (pos + 1) % h->capacity;                                         \
      }                                                                        \
    }                                                                          \
    return pos;                                                                \
  }                                                                            \
                                                                               \
  static void pref##destroybuffers(HashTable *h) {                             \
    if (h->keys)                                                               \
      (void)realloc_function(h->allocator, h->keys, h->capacity * sizeof(Key), \
                             0);                                               \
    if (h->values)                                                             \
      (void)realloc_function(h->allocator, h->values,                          \
                             h->capacity * sizeof(Value), 0);                  \
    if (h->usedpos)                                                            \
      (void)realloc_function(h->allocator, h->usedpos,                         \
                             pref##getusedpossize(h->capacity), 0);            \
  }                                                                            \
                                                                               \
  static void pref##resizehash(HashTable *h, size_t newcapacity) {             \
    size_t i;                                                                  \
    HashTable oldhash = *h; /* shallow copy */                                 \
    h->keys =                                                                  \
        realloc_function(h->allocator, NULL, 0, newcapacity * sizeof(Key));    \
    h->values =                                                                \
        realloc_function(h->allocator, NULL, 0, newcapacity * sizeof(Value));  \
    h->usedpos = realloc_function(h->allocator, NULL, 0,                       \
                                  pref##getusedpossize(newcapacity));          \
    memset(h->usedpos, 0, pref##getusedpossize(newcapacity));                  \
    h->capacity = newcapacity;                                                 \
    h->size = 0;                                                               \
    for (i = 0; i < oldhash.capacity; ++i)                                     \
      if (pref##getusedpos(&oldhash, i))                                       \
        pref##insert(h, oldhash.keys[i], oldhash.values[i]);                   \
    pref##destroybuffers(&oldhash);                                            \
  }                                                                            \
                                                                               \
  static int pref##growhashtable(HashTable *h) {                               \
    if (pref##computerequiredcapacity(h->size + 1) > h->capacity) {            \
      pref##resizehash(h, pref##computerequiredcapacity(h->size * 2));         \
      return 1;                                                                \
    } else {                                                                   \
      return 0;                                                                \
    }                                                                          \
  }                                                                            \
                                                                               \
  HashTable *pref##create(size_t nelements) {                                  \
    return pref##createwa(nelements, NULL);                                    \
  }                                                                            \
                                                                               \
  HashTable *pref##createwa(size_t nelements, AllocatorType allocator) {       \
    HashTable *h;                                                              \
    if (nelements < 8)                                                         \
      nelements = 8;                                                           \
    h = realloc_function(allocator, NULL, 0, sizeof(HashTable));               \
    h->allocator = allocator;                                                  \
    h->keys = NULL;                                                            \
    h->values = NULL;                                                          \
    h->usedpos = NULL;                                                         \
    h->size = h->capacity = 0;                                                 \
    pref##resizehash(h, pref##computerequiredcapacity(nelements));             \
    return h;                                                                  \
  }                                                                            \
                                                                               \
  void pref##destroy(HashTable *h) {                                           \
    if (!h)                                                                    \
      return;                                                                  \
    pref##destroybuffers(h);                                                   \
    (void)realloc_function(h->allocator, h, sizeof(HashTable), 0);             \
  }                                                                            \
                                                                               \
  HashTable *pref##clone(HashTable *h) {                                       \
    size_t i;                                                                  \
    HashTable *newtable;                                                       \
    tscc_assert(h, "null hash table");                                         \
    newtable = pref##createwa(pref##maxsize(h), h->allocator);                 \
    for (i = 0; i < h->capacity; ++i)                                          \
      if (pref##getusedpos(h, i))                                              \
        pref##insert(newtable, h->keys[i], h->values[i]);                      \
    return newtable;                                                           \
  }                                                                            \
                                                                               \
  void pref##clear(HashTable *h) {                                             \
    tscc_assert(h, "null hash table");                                         \
    memset(h->usedpos, 0, pref##getusedpossize(h->capacity));                  \
    h->size = 0;                                                               \
  }                                                                            \
                                                                               \
  int pref##empty(HashTable *h) {                                              \
    tscc_assert(h, "null hash table");                                         \
    return h->size == 0;                                                       \
  }                                                                            \
                                                                               \
  size_t pref##size(HashTable *h) {                                            \
    tscc_assert(h, "null hash table");                                         \
    return h->size;                                                            \
  }                                                                            \
                                                                               \
  size_t pref##maxsize(HashTable *h) {                                         \
    size_t maxsize;                                                            \
    tscc_assert(h, "null hash table");                                         \
    maxsize = 1 + h->capacity * TSCC_HASH_LOAD;                                \
    if (pref##computerequiredcapacity(maxsize) > h->capacity)                  \
      return maxsize - 1;                                                      \
    else                                                                       \
      return maxsize;                                                          \
  }                                                                            \
                                                                               \
  void pref##insert(HashTable *h, Key key, Value value) {                      \
    int found;                                                                 \
    size_t pos;                                                                \
    tscc_assert(h, "null hash table");                                         \
    pos = pref##getposition(h, key, &found);                                   \
    if (!found && pref##growhashtable(h))                                      \
      pos = pref##getposition(h, key, &found);                                 \
    pref##setusedpos(h, pos, 1);                                               \
    h->keys[pos] = key;                                                        \
    h->values[pos] = value;                                                    \
    h->size++;                                                                 \
  }                                                                            \
                                                                               \
  int pref##tryinsert(HashTable *h, Key key, Value value) {                    \
    int found;                                                                 \
    size_t pos;                                                                \
    tscc_assert(h, "null hash table");                                         \
    pos = pref##getposition(h, key, &found);                                   \
    if (!found) {                                                              \
      if (pref##growhashtable(h))                                              \
        pos = pref##getposition(h, key, &found);                               \
      pref##setusedpos(h, pos, 1);                                             \
      h->keys[pos] = key;                                                      \
      h->values[pos] = value;                                                  \
      h->size++;                                                               \
      return 1;                                                                \
    } else {                                                                   \
      return 0;                                                                \
    }                                                                          \
  }                                                                            \
                                                                               \
  int pref##find(HashTable *h, Key key, Value *value) {                        \
    int found;                                                                 \
    size_t pos;                                                                \
    tscc_assert(h, "null hash table");                                         \
    pos = pref##getposition(h, key, &found);                                   \
    if (found)                                                                 \
      *value = h->values[pos];                                                 \
    return found;                                                              \
  }                                                                            \
                                                                               \
  int pref##contains(HashTable *h, Key key) {                                  \
    Value v;                                                                   \
    return pref##find(h, key, &v);                                             \
  }                                                                            \
                                                                               \
  Value pref##get(HashTable *h, Key key, Value def) {                          \
    Value value;                                                               \
    int found = pref##find(h, key, &value);                                    \
    if (found)                                                                 \
      return value;                                                            \
    else                                                                       \
      return def;                                                              \
  }                                                                            \
                                                                               \
  int pref##erase(HashTable *h, Key key, Value *value) {                       \
    int found;                                                                 \
    size_t pos;                                                                \
    tscc_assert(h, "null hash table");                                         \
    pos = pref##getposition(h, key, &found);                                   \
    if (found) {                                                               \
      if (value)                                                               \
        *value = h->values[pos];                                               \
      pref##setusedpos(h, pos, 0);                                             \
      h->size--;                                                               \
    }                                                                          \
    return found;                                                              \
  }                                                                            \
                                                                               \
  TSCC_ITERATOR(HashTable) pref##begin(HashTable *h) {                         \
    TSCC_ITERATOR(HashTable) it = {0};                                         \
    while (it.pos < h->capacity && !pref##getusedpos(h, it.pos))               \
      it.pos++;                                                                \
    return it;                                                                 \
  }                                                                            \
                                                                               \
  int pref##valid(HashTable *h, TSCC_ITERATOR(HashTable) it) {                 \
    return it.pos < h->capacity;                                               \
  }                                                                            \
                                                                               \
  TSCC_ITERATOR(HashTable)                                                     \
  pref##next(HashTable *h, TSCC_ITERATOR(HashTable) it) {                      \
    do {                                                                       \
      it.pos++;                                                                \
    } while (it.pos < h->capacity && !pref##getusedpos(h, it.pos));            \
    return it;                                                                 \
  }                                                                            \
                                                                               \
  Key pref##key(HashTable *h, TSCC_ITERATOR(HashTable) it) {                   \
    return h->keys[it.pos];                                                    \
  }                                                                            \
                                                                               \
  Value pref##value(HashTable *h, TSCC_ITERATOR(HashTable) it) {               \
    return h->values[it.pos];                                                  \
  }

/* Iterates throught the hash table and execute the command. */
#define TSCC_HASH_FOREACH(HashTable, pref, h, Key, k, Value, v, cmd)           \
  {                                                                            \
    TSCC_ITERATOR(HashTable) _it_##k;                                          \
    for (_it_##k = pref##begin(h); pref##valid(h, _it_##k);                     \
         _it_##k = pref##next(h, _it_##k)) {                                   \
      Key k = pref##key(h, _it_##k);                                           \
      Value v = pref##value(h, _it_##k);                                       \
      (void)k;                                                                 \
      (void)v;                                                                 \
      cmd;                                                                     \
    }                                                                          \
  }

/* HashTable declaration end */

#endif

