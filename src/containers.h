/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Gabriel de Quadros Ligneul
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef typesafeccontainers_h
#define typesafeccontainers_h

#include <string.h>

/* Modifier for static functions. */
#ifndef TSCC_INLINE
#define TSCC_INLINE static __inline
#endif

/* Reallocation function. */
#ifndef tscc_realloc
#include <stdlib.h>
#define tscc_realloc(allocator, ptr, oldsize, newsize)                         \
  ((newsize != 0) ? (realloc(ptr, newsize)) : (free(ptr), NULL))
#endif

/* Allocator type. */
#ifndef TsccAllocator
#define TSCC_ALLOC_ENABLE 0
#else
#define TSCC_ALLOC_ENABLE 1
#endif

/* cpp magic. */
#define TSCC_CAT(x, y) x##y
#define TSCC_IF(c, x) TSCC_CAT(TSCC_IF_, c)(x)
#define TSCC_IF_0(x)
#define TSCC_IF_1(x) x
#define TSCC_IFEL(c, x, y) TSCC_CAT(TSCC_IFEL_, c)(x, y)
#define TSCC_IFEL_0(_, x) x
#define TSCC_IFEL_1(x, _) x
#define TSCC_IF_ALLOC(x) TSCC_IF(TSCC_ALLOC_ENABLE, x)
#define TSCC_IFEL_ALLOC(x, y) TSCC_IFEL(TSCC_ALLOC_ENABLE, x, y)

/* Default assert function. */
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

/* Vector declaration.
 * Dynamic array that grows automaticaly. Can be used as a stack. */
#define TSCC_DECL_VECTOR(Vector, pref, T)                                      \
                                                                               \
/* Vector type. */                                                             \
typedef struct Vector {                                                        \
  TSCC_IF_ALLOC(TsccAllocator allocator;)                                      \
  T *buffer;                                                                   \
  size_t size;                                                                 \
  size_t capacity;                                                             \
} Vector;                                                                      \
                                                                               \
/* Resize the vector capacity. */                                              \
TSCC_INLINE void pref##resizevector_(Vector *v, size_t newcapacity) {          \
  v->buffer = tscc_realloc(TSCC_IFEL_ALLOC(v->allocator, NULL), v->buffer,     \
    v->capacity * sizeof(T), newcapacity * sizeof(T));                         \
  v->capacity = newcapacity;                                                   \
}                                                                              \
                                                                               \
/* Grow the vector capacity if the necessary. */                               \
TSCC_INLINE void pref##growvector_(Vector *v) {                                \
  if (v->size + 1 > v->capacity)                                               \
    pref##resizevector_(v, v->capacity * 2);                                   \
}                                                                              \
                                                                               \
/* Initialize the vector's internal buffer. */                                 \
TSCC_INLINE void pref##create                                                  \
    TSCC_IFEL_ALLOC((Vector *v, TsccAllocator allocator), (Vector *v)) {       \
  TSCC_IF_ALLOC(v->allocator = allocator);                                     \
  v->buffer = NULL;                                                            \
  v->size = v->capacity = 0;                                                   \
  pref##resizevector_(v, 4);                                                   \
}                                                                              \
                                                                               \
/* Destroy the vector's internal buffer. Don't free it's elements. */          \
TSCC_INLINE void pref##destroy(Vector *v) {                                    \
  if (!v) return;                                                              \
  (void)tscc_realloc(TSCC_IFEL_ALLOC(v->allocator, NULL), v->buffer,           \
    v->capacity * sizeof(T), 0);                                               \
}                                                                              \
                                                                               \
/* Obtain the vector capacity. */                                              \
TSCC_INLINE size_t pref##capacity(Vector *v) {                                 \
  return v->capacity;                                                          \
}                                                                              \
                                                                               \
/* Update the vector capacity. If the new capacity is smaller than the current
 * size, the vector will be resized. */                                        \
TSCC_INLINE void pref##reserve(Vector *v, size_t newcapacity) {                \
  if (newcapacity == 0) return;                                                \
  pref##resizevector_(v, newcapacity);                                         \
  if (v->size > newcapacity) v->size = newcapacity;                            \
}                                                                              \
                                                                               \
/* Shrink the vector capacity to the current size. */                          \
TSCC_INLINE void pref##shrink(Vector *v) {                                     \
  pref##reserve(v, v->size);                                                   \
}                                                                              \
                                                                               \
/* Return 1 if size equals to 0. */                                            \
TSCC_INLINE int pref##empty(Vector *v) {                                       \
  return v->size == 0;                                                         \
}                                                                              \
                                                                               \
/* Return the number of elements. */                                           \
TSCC_INLINE size_t pref##size(Vector *v) {                                     \
  return v->size;                                                              \
}                                                                              \
                                                                               \
/* Change the vector size. */                                                  \
TSCC_INLINE void pref##resize(Vector *v, size_t newsize) {                     \
  if (newsize > v->capacity) pref##resizevector_(v, newsize);                  \
  v->size = newsize;                                                           \
}                                                                              \
                                                                               \
/* Remove all elements from the vector. */                                     \
TSCC_INLINE void pref##clear(Vector *v) {                                      \
  v->size = 0;                                                                 \
}                                                                              \
                                                                               \
/* Insert an element into the last position. */                                \
TSCC_INLINE void pref##push(Vector *v, T value) {                              \
  pref##growvector_(v);                                                        \
  v->buffer[v->size++] = value;                                                \
}                                                                              \
                                                                               \
/* Remove the last element and return it. */                                   \
TSCC_INLINE T pref##pop(Vector *v) {                                           \
  tscc_assert(v->size > 0, "empty vector");                                    \
  return v->buffer[--v->size];                                                 \
}                                                                              \
                                                                               \
/* Insert an element at the required position. */                              \
TSCC_INLINE void pref##insert(Vector *v, size_t pos, T value) {                \
  size_t i;                                                                    \
  tscc_assert(pos <= v->size, "out of bounds");                                \
  pref##growvector_(v);                                                        \
  for (i = v->size; i > pos; --i)                                              \
      v->buffer[i] = v->buffer[i - 1];                                         \
  v->buffer[pos] = value;                                                      \
  v->size++;                                                                   \
}                                                                              \
                                                                               \
/* Erase the element at the required position. */                              \
TSCC_INLINE void pref##erase(Vector *v, size_t pos) {                          \
  size_t i;                                                                    \
  tscc_assert(pos <= v->size, "out of bounds");                                \
  for (i = pos; i < v->size; ++i)                                              \
      v->buffer[i] = v->buffer[i + 1];                                         \
  v->size--;                                                                   \
}                                                                              \
                                                                               \
/* Obtain the element. */                                                      \
TSCC_INLINE T pref##get(Vector *v, size_t pos) {                               \
  tscc_assert(pos < v->size, "out of bounds");                                 \
  return v->buffer[pos];                                                       \
}                                                                              \
                                                                               \
/* Obtain a reference of the element. */                                       \
TSCC_INLINE T *pref##getref(Vector *v, size_t pos) {                           \
  tscc_assert(pos < v->size, "out of bounds");                                 \
  return v->buffer + pos;                                                      \
}                                                                              \
                                                                               \
/* Set the value of an element. */                                             \
TSCC_INLINE void pref##set(Vector *v, size_t pos, T value) {                   \
  tscc_assert(pos < v->size, "out of bounds");                                 \
  v->buffer[pos] = value;                                                      \
}                                                                              \
                                                                               \
/* Obtain the element at the first position. */                                \
TSCC_INLINE T pref##front(Vector *v) {                                         \
  tscc_assert(v->size > 0, "empty vector");                                    \
  return v->buffer[0];                                                         \
}                                                                              \
                                                                               \
/* Obtain the element at the last position. */                                 \
TSCC_INLINE T pref##back(Vector *v) {                                          \
  tscc_assert(v->size > 0, "empty vector");                                    \
  return v->buffer[v->size - 1];                                               \
}                                                                              \
                                                                               \
/* Obtain the raw pointer to the internal buffer. */                           \
TSCC_INLINE T *pref##data(Vector *v) {                                         \
  return v->buffer;                                                            \
}

/* Iterates throght the vector and executes the command */
#define TSCC_VECTOR_FOREACH(prefix, v, T, t, cmd)                              \
  do {                                                                         \
    size_t _i_##t;                                                             \
    size_t _n_##t = prefix##size(v);                                           \
    for (_i_##t = 0; _i_##t < _n_##t; ++_i_##t) {                              \
      T *t = prefix##getref(v, _i_##t);                                        \
      (void)t;                                                                 \
      cmd;                                                                     \
    }                                                                          \
  } while(0)

/* HashTable max load factor. */
#ifndef TSCC_HASH_LOAD
#define TSCC_HASH_LOAD 0.6
#endif

/* GNU C extension used to detect the pointer size. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
#define TSCC_ENABLE_64BITS
#endif

/* FNV constants. */
#ifdef TSCC_ENABLE_64BITS
#define TSCC_FNV_OFFSET 0xcbf29ce484222325
#define TSCC_FNV_PRIME 0x00000100000001b3
#else
#define TSCC_FNV_OFFSET 0x811c9dc5
#define TSCC_FNV_PRIME 0x01000193
#endif

/* FNV-1a hash function for strings. */
TSCC_INLINE size_t tscc_str_hashfunc(const char *str) {
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
TSCC_INLINE size_t prefix##hashfunc(T value) {                                 \
  char mem[sizeof(value)];                                                     \
  size_t i, hash = TSCC_FNV_OFFSET;                                            \
  memcpy(mem, &value, sizeof(value));                                          \
  for (i = 0; i < sizeof(T); ++i) {                                            \
    hash ^= mem[i];                                                            \
    hash *= TSCC_FNV_PRIME;                                                    \
  }                                                                            \
  return hash;                                                                 \
}

/* Specific hash functions declarations. */
TSCC_DECL_GENERAL_HASHFUNC(long int, tscc_int_)
TSCC_DECL_GENERAL_HASHFUNC(double, tscc_float_)
TSCC_DECL_GENERAL_HASHFUNC(void *, tscc_ptr_)

/* Hash compare function for strings. */
TSCC_INLINE int tscc_str_compare(const char *a, const char *b) {
  if (!a || !b)
    return a == b;
  else
    return !strcmp(a, b);
}

/* Hash compare function for primitive types. */
#define tscc_general_compare(a, b) ((a) == (b))

/* Hash table that maps a key to a value. */
#define TSCC_DECL_HASHTABLE(HashTable, pref, Key, Value, hashfunc,             \
                               keyequal)                                       \
                                                                               \
/* HashTable declartion. */                                                    \
typedef struct HashTable {                                                     \
  TSCC_IF_ALLOC(TsccAllocator allocator;)                                      \
  Key *keys;                                                                   \
  Value *values;                                                               \
  char *used;                                                                  \
  size_t size;                                                                 \
  size_t capacity;                                                             \
} HashTable;                                                                   \
                                                                               \
/* Compute the required capacity given the number of elements. */              \
TSCC_INLINE size_t pref##requiredcapacity_(size_t size) {                      \
  return size / TSCC_HASH_LOAD;                                                \
}                                                                              \
                                                                               \
/* Obtain the position of the entry. Return by reference if it was found. */   \
TSCC_INLINE size_t pref##getposition_(HashTable *h, Key key, int *found) {     \
  size_t i;                                                                    \
  size_t pos = hashfunc(key) % h->capacity;                                    \
  for (i = 0; i < h->size + 1; ++i) {                                          \
    int used = h->used[pos];                                                   \
    if (!used) {                                                               \
      *found = 0;                                                              \
      break;                                                                   \
    }                                                                          \
    else if (used && keyequal(key, h->keys[pos])) {                            \
      *found = 1;                                                              \
      break;                                                                   \
    }                                                                          \
    else {                                                                     \
      pos = (pos + 1) % h->capacity;                                           \
    }                                                                          \
  }                                                                            \
  return pos;                                                                  \
}                                                                              \
                                                                               \
TSCC_INLINE void pref##insert(HashTable *h, Key key, Value value);             \
TSCC_INLINE void pref##destroy(HashTable *h);                                  \
                                                                               \
/* Reallocate the buffer and rehash all the entries. */                        \
TSCC_INLINE void pref##resizehash_(HashTable *h, size_t newcapacity) {         \
  size_t i;                                                                    \
  HashTable oldhash = *h; /* shallow copy */                                   \
  h->keys = tscc_realloc(TSCC_IFEL_ALLOC(h->allocator, NULL), NULL, 0,         \
    newcapacity * sizeof(Key));                                                \
  h->values = tscc_realloc(TSCC_IFEL_ALLOC(h->allocator, NULL), NULL, 0,       \
    newcapacity * sizeof(Value));                                              \
  h->used = tscc_realloc(TSCC_IFEL_ALLOC(h->allocator, NULL), NULL, 0,         \
    newcapacity);                                                              \
  memset(h->used, 0, newcapacity);                                             \
  h->capacity = newcapacity;                                                   \
  h->size = 0;                                                                 \
  for (i = 0; i < oldhash.capacity; ++i)                                       \
    if (oldhash.used[i])                                                       \
      pref##insert(h, oldhash.keys[i], oldhash.values[i]);                     \
  pref##destroy(&oldhash);                                                     \
}                                                                              \
                                                                               \
/* Increase the capacity of the hash table by 1 if necessary. */               \
TSCC_INLINE int pref##growhashtable_(HashTable *h) {                           \
  if (pref##requiredcapacity_(h->size + 1) > h->capacity) {                    \
    pref##resizehash_(h, pref##requiredcapacity_(h->size * 2));                \
    return 1;                                                                  \
  } else {                                                                     \
    return 0;                                                                  \
  }                                                                            \
}                                                                              \
                                                                               \
/* Create a hash table that can hold nelements without rehashing. */           \
TSCC_INLINE void pref##create                                                  \
    TSCC_IFEL_ALLOC((HashTable *h, size_t nelements, TsccAllocator allocator), \
                    (HashTable *h, size_t nelements)) {                        \
  if (nelements < 8) nelements = 8;                                            \
  TSCC_IF_ALLOC(h->allocator = allocator);                                     \
  h->keys = NULL;                                                              \
  h->values = NULL;                                                            \
  h->used = NULL;                                                              \
  h->size = h->capacity = 0;                                                   \
  pref##resizehash_(h, pref##requiredcapacity_(nelements));                    \
}                                                                              \
                                                                               \
/* Destroy the hash table. ATTENTION: don't free it's elements. */             \
TSCC_INLINE void pref##destroy(HashTable *h) {                                 \
  if (!h || !h->keys) return;                                                  \
  (void)tscc_realloc(TSCC_IFEL_ALLOC(h->allocator, NULL), h->keys,             \
    h->capacity * sizeof(Key), 0);                                             \
  (void)tscc_realloc(TSCC_IFEL_ALLOC(h->allocator, NULL), h->values,           \
    h->capacity * sizeof(Value), 0);                                           \
  (void)tscc_realloc(TSCC_IFEL_ALLOC(h->allocator, NULL), h->used,             \
    h->capacity, 0);                                                           \
}                                                                              \
                                                                               \
/* Remove all entries from the hash table. */                                  \
TSCC_INLINE void pref##clear(HashTable *h) {                                   \
  memset(h->used, 0, h->capacity);                                             \
  h->size = 0;                                                                 \
}                                                                              \
                                                                               \
/* Return 1 if size equals to 0. */                                            \
TSCC_INLINE int pref##empty(HashTable *h) {                                    \
  return h->size == 0;                                                         \
}                                                                              \
                                                                               \
/* Return the number of elements. */                                           \
TSCC_INLINE size_t pref##size(HashTable *h) {                                  \
  return h->size;                                                              \
}                                                                              \
                                                                               \
/* Return the maximum number of elements that the hash table can hold without
 * having to rehash. */                                                        \
TSCC_INLINE size_t pref##maxsize(HashTable *h) {                               \
  size_t maxsize;                                                              \
  maxsize = 1 + h->capacity * TSCC_HASH_LOAD;                                  \
  if (pref##requiredcapacity_(maxsize) > h->capacity)                          \
    return maxsize - 1;                                                        \
  else                                                                         \
    return maxsize;                                                            \
}                                                                              \
                                                                               \
/* Insert the entry (key, value) into the hash. Replace the old one if it
 * already exists. */                                                          \
TSCC_INLINE void pref##insert(HashTable *h, Key key, Value value) {            \
  int found;                                                                   \
  size_t pos;                                                                  \
  pos = pref##getposition_(h, key, &found);                                    \
  if (!found) {                                                                \
    h->size++;                                                                 \
    if (pref##growhashtable_(h))                                               \
      pos = pref##getposition_(h, key, &found);                                \
  }                                                                            \
  h->used[pos] = 1;                                                            \
  h->keys[pos] = key;                                                          \
  h->values[pos] = value;                                                      \
}                                                                              \
                                                                               \
/* Try to insert the entry (key, value) into the hash. Return 1 if the entry
 * was inserted and 0 there was already an entry with this key. */             \
TSCC_INLINE int pref##tryinsert(HashTable *h, Key key, Value value) {          \
  int found;                                                                   \
  size_t pos;                                                                  \
  pos = pref##getposition_(h, key, &found);                                    \
  if (!found) {                                                                \
    if (pref##growhashtable_(h))                                               \
      pos = pref##getposition_(h, key, &found);                                \
    h->used[pos] = 1;                                                          \
    h->keys[pos] = key;                                                        \
    h->values[pos] = value;                                                    \
    h->size++;                                                                 \
    return 1;                                                                  \
  } else {                                                                     \
    return 0;                                                                  \
  }                                                                            \
}                                                                              \
                                                                               \
/* Find an entry in the hash table and return the value by reference. Return 1
 * if an entry was found, return 0 otherwise. */                               \
TSCC_INLINE int pref##find(HashTable *h, Key key, Value *value) {              \
  int found;                                                                   \
  size_t pos;                                                                  \
  pos = pref##getposition_(h, key, &found);                                    \
  if (found)                                                                   \
    *value = h->values[pos];                                                   \
  return found;                                                                \
}                                                                              \
                                                                               \
/* Search for an entry with the key in the hash table. Return 1 if an entry
 * was found, return 0 otherwise. */                                           \
TSCC_INLINE int pref##contains(HashTable *h, Key key) {                        \
  Value v;                                                                     \
  return pref##find(h, key, &v);                                               \
}                                                                              \
                                                                               \
/* Find an entry in the hash table and return the value. If no entry was found
 * with the key, return the default one. */                                    \
TSCC_INLINE Value pref##get(HashTable *h, Key key, Value def) {                \
  Value value;                                                                 \
  int found = pref##find(h, key, &value);                                      \
  if (found)                                                                   \
    return value;                                                              \
  else                                                                         \
    return def;                                                                \
}                                                                              \
                                                                               \
/* Erase an entry from the hash table and return the value by reference.
 * Return 1 if an entry was found and removed, return 0 if no entry was found.
 * The value parameter can be null. */                                         \
TSCC_INLINE int pref##erase(HashTable *h, Key key, Value *value) {             \
  int found;                                                                   \
  size_t pos;                                                                  \
  pos = pref##getposition_(h, key, &found);                                    \
  if (found) {                                                                 \
    if (value)                                                                 \
      *value = h->values[pos];                                                 \
    h->used[pos] = 0;                                                          \
    h->size--;                                                                 \
  }                                                                            \
  return found;                                                                \
}

/* Iterates throught the hash table and execute the command. */
#define TSCC_HASH_FOREACH(pref, h, Key, k, Value, v, cmd)                      \
  do {                                                                         \
    size_t i##k;                                                               \
    for (i##k = 0; i##k < (h)->capacity; i##k++) {                             \
      if ((h)->used[i]) {                                                      \
        Key k = (h)->keys[i];                                                  \
        Value v = (h)->values[i];                                              \
        (void)k;                                                               \
        (void)v;                                                               \
        cmd;                                                                   \
      }                                                                        \
    }                                                                          \
  } while (0)

#endif

