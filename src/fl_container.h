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

/*
 * Default reallocation function
 * Redefine it before the include to change the realloc behavior.
 */
#define tscc_realloc_default(allocator, ptr, oldsize, newsize) \
    ((newsize != 0) ? (realloc(ptr, newsize)) : (free(ptr), NULL))

/*
 * Default assert function
 */
#ifndef tscc_assert
#ifdef NDEBUG
#include <assert.h>
#include <stdio.h>
#define tscc_assert(expression, message) \
  do { \
    if (!(expression)) \
      fprintf(stderr, "Error: %s\n", message); \
    assert(expression); \
  } while (0)
#else
#define tscc_assert(expression, message) (void)0
#endif
#endif

/*
 * HashTable max load factor.
 */
#ifndef TSCC_HASH_LOAD
#define TSCC_HASH_LOAD 0.6
#endif

/*
 * FNV-1a hash function for strings.
 */
static __inline size_t tscc_str_hashfunc(const char *str) {
#ifdef TSCC_ENABLE_32BITS
    size_t fnv_offset_basis = 0x811c9dc5;
    size_t fnv_prime = 0x01000193;
#else
    size_t fnv_offset_basis = 0xcbf29ce484222325;
    size_t fnv_prime = 0x00000100000001b3;
#endif
    if (str != NULL) {
        size_t i, hash = fnv_offset_basis;
        for (i = 0; str[i] != '\0'; ++i) {
            hash ^= str[i];
            hash *= fnv_prime;
        }
        return hash;
    } else {
        return 0;
    }
}

/*
 * Declares a general hash function that converts the type to a string and use
 * the FNV-1a hash function.
 */
#define TSCC_DECL_GENERAL_HASHFUNC(T, prefix) \
static __inline size_t prefix##hashfunc(T value) { \
    char mem[sizeof(value) + 1] = {'\0'}; \
    memcpy(mem, &value, sizeof(value)); \
    return tscc_str_hashfunc(mem); \
}

/*
 * Specific hash functions declarations.
 */
TSCC_DECL_GENERAL_HASHFUNC(long int, tscc_int_)
TSCC_DECL_GENERAL_HASHFUNC(double, tscc_float_)
TSCC_DECL_GENERAL_HASHFUNC(void *, tscc_ptr_)

/*
 * Hash compare function for strings.
 */
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

/*
 * Hash compare function for primitive types
 */
#define tscc_general_compare(a, b) ((a) == (b))

/* Vector declaration start */
#define TSCC_DECL_VECTOR(typename, funcprefix, T) \
   TSCC_DECL_VECTOR_WA(typename, funcprefix, T, void *) \

#define TSCC_DECL_VECTOR_WA(typename, funcprefix, T, AllocatorType) \
typedef struct typename typename; \
typename *funcprefix##create(size_t buffsize); \
typename *funcprefix##createwa(size_t buffsize, AllocatorType allocator); \
void funcprefix##destroy(typename *v); \
int funcprefix##empty(typename *v); \
size_t funcprefix##size(typename *v); \
void funcprefix##resize(typename *v, size_t newsize); \
void funcprefix##push(typename *v, T value); \
T funcprefix##pop(typename *v); \
void funcprefix##insert(typename *v, size_t pos, T value); \
T funcprefix##get(typename *v, size_t pos); \
void funcprefix##set(typename *v, size_t pos, T value); \
T funcprefix##front(typename *v); \
T funcprefix##back(typename *v); \

#define TSCC_IMPL_VECTOR(typename, funcprefix, T) \
   TSCC_IMPL_VECTOR_WA(typename, funcprefix, T, void *, tscc_realloc_default) \

#define TSCC_IMPL_VECTOR_WA(typename, funcprefix, T, AllocatorType, realloc_function) \
        struct typename { \
            AllocatorType allocator; \
            T *buffer; \
            size_t size; \
            size_t capacity; \
        }; \
static void funcprefix##resizevector(typename *v, size_t newcapacity) \
{ \
        v->buffer = realloc_function(v->allocator, v->buffer, \
        v->capacity * sizeof(T), newcapacity * sizeof(T)); \
        v->capacity = newcapacity; \
    } \
static void funcprefix##growvector(typename *v) \
{ \
        if (v->capacity == 0) \
            funcprefix##resizevector(v, 1); \
        else if (v->size + 1 > v->capacity) \
            funcprefix##resizevector(v, v->capacity * 2); \
    } \
typename *funcprefix##create(size_t buffsize) \
{ \
        return funcprefix##createwa(buffsize, NULL); \
    } \
typename *funcprefix##createwa(size_t buffsize, AllocatorType allocator) \
{ \
        typename *v = realloc_function(allocator, NULL, 0, sizeof(typename)); \
        v->allocator = allocator; \
        v->buffer = NULL; \
        v->size = v->capacity = 0; \
        funcprefix##resizevector(v, buffsize); \
        return v; \
    } \
void funcprefix##destroy(typename *v) \
{ \
        if (!v) return; \
        (void)realloc_function(v->allocator, v->buffer, v->capacity * sizeof(T), 0); \
        (void)realloc_function(v->allocator, v, sizeof(typename), 0); \
    } \
int funcprefix##empty(typename *v) \
{ \
        tscc_assert(v, "null vector"); \
        return v->size == 0; \
    } \
size_t funcprefix##size(typename *v) \
{ \
        tscc_assert(v, "null vector"); \
        return v->size; \
    } \
void funcprefix##resize(typename *v, size_t newsize) \
{ \
        tscc_assert(v, "null vector"); \
        if (newsize > v->capacity) \
            funcprefix##resizevector(v, newsize); \
        v->size = newsize; \
    } \
void funcprefix##push(typename *v, T value) \
{ \
        tscc_assert(v, "null vector"); \
        funcprefix##growvector(v); \
        v->buffer[v->size++] = value; \
    } \
T funcprefix##pop(typename *v) \
{ \
        tscc_assert(v, "null vector"); \
        tscc_assert(v->size > 0, "empty vector"); \
        return v->buffer[--v->size]; \
    } \
void funcprefix##insert(typename *v, size_t pos, T value) \
{ \
        size_t i; \
        tscc_assert(v, "null vector"); \
        tscc_assert(pos <= v->size, "out of bounds"); \
        funcprefix##growvector(v); \
        for (i = v->size; i > pos; --i) \
            v->buffer[i] = v->buffer[i - 1]; \
        v->buffer[pos] = value; \
        v->size++; \
    } \
T funcprefix##get(typename *v, size_t pos) \
{ \
        tscc_assert(v, "null vector"); \
        tscc_assert(pos < v->size, "out of bounds"); \
        return v->buffer[pos]; \
    } \
void funcprefix##set(typename *v, size_t pos, T value) \
{ \
        tscc_assert(v, "null vector"); \
        tscc_assert(pos < v->size, "out of bounds"); \
        v->buffer[pos] = value; \
    } \
T funcprefix##front(typename *v) \
{ \
        tscc_assert(v, "null vector"); \
        tscc_assert(v->size > 0, "empty vector"); \
        return v->buffer[0]; \
    } \
T funcprefix##back(typename *v) \
{ \
        tscc_assert(v, "null vector"); \
        tscc_assert(v->size > 0, "empty vector"); \
        return v->buffer[v->size - 1]; \
    } \

/* Vector declaration end */
/* HashTable declaration start */
#define TSCC_DECL_HASHTABLE(typename, funcprefix, Key, Value) \
   TSCC_DECL_HASHTABLE_WA(typename, funcprefix, Key, Value, void *) \

#define TSCC_DECL_HASHTABLE_WA(typename, funcprefix, Key, Value, AllocatorType) \
typedef struct typename typename; \
typename *funcprefix##create(size_t nelements); \
typename *funcprefix##createwa(size_t nelements, AllocatorType allocator); \
void funcprefix##destroy(typename *h); \
void funcprefix##clear(typename *h); \
int funcprefix##empty(typename *h); \
size_t funcprefix##size(typename *h); \
size_t funcprefix##maxsize(typename *h); \
void funcprefix##insert(typename *h, Key key, Value value); \
int funcprefix##tryinsert(typename *h, Key key, Value value); \
int funcprefix##find(typename *h, Key key, Value *value); \
int funcprefix##contains(typename *h, Key key); \
Value funcprefix##get(typename *h, Key key, Value def); \
int funcprefix##erase(typename *h, Key key, Value *value); \

#define TSCC_IMPL_HASHTABLE(typename, funcprefix, Key, Value, hashfunc, keyequal) \
   TSCC_IMPL_HASHTABLE_WA(typename, funcprefix, Key, Value, hashfunc, keyequal, void *, tscc_realloc_default) \

#define TSCC_IMPL_HASHTABLE_WA(typename, funcprefix, Key, Value, hashfunc, keyequal, AllocatorType, realloc_function) \
        struct typename { \
            AllocatorType allocator; \
            Key *keys; \
            Value *values; \
            unsigned char* usedpos; \
            size_t size; \
            size_t capacity; \
        }; \
static size_t funcprefix##getusedpossize(size_t capacity) \
{ \
        return sizeof(unsigned char) * (capacity / 8 + ((capacity % 8) != 0)); \
    } \
static int funcprefix##getusedpos(typename *h, size_t pos) \
{ \
        unsigned char mask = 1 << (pos % 8); \
        return !!(h->usedpos[pos / 8] & mask); \
    } \
static void funcprefix##setusedpos(typename *h, size_t pos, int used) \
{ \
        unsigned char mask = 1 << (pos % 8); \
        size_t cellid = pos / 8; \
        if (used) \
            h->usedpos[cellid] |= mask; \
        else \
            h->usedpos[cellid] &= ~mask; \
    } \
static size_t funcprefix##computerequiredcapacity(size_t size) \
{ \
        return size / TSCC_HASH_LOAD; \
    } \
static size_t funcprefix##hash(typename *h, Key key) \
{ \
        return hashfunc(key) % h->capacity; \
    } \
static size_t funcprefix##getposition(typename *h, Key key, int *found) \
{ \
        size_t i; \
        size_t pos = funcprefix##hash(h, key); \
        for (i = 0; i < h->size + 1; ++i) { \
            int usedpos = funcprefix##getusedpos(h, pos); \
            if (!usedpos) { \
                *found = 0; \
                break; \
            } else if (usedpos && keyequal(key, h->keys[pos])) { \
                *found = 1; \
                break; \
            } else { \
                pos = (pos + 1) % h->capacity; \
            } \
        } \
        return pos; \
    } \
static void funcprefix##destroybuffers(typename *h) \
{ \
        if (h->keys) \
            (void)realloc_function(h->allocator, h->keys, \
                    h->capacity * sizeof(Key), 0); \
        if (h->values) \
            (void)realloc_function(h->allocator, h->values, \
                    h->capacity * sizeof(Value), 0); \
        if (h->usedpos) \
            (void)realloc_function(h->allocator, h->usedpos, \
                    funcprefix##getusedpossize(h->capacity), 0); \
    } \
static void funcprefix##resizehash(typename *h, size_t newcapacity) \
{ \
        size_t i; \
        typename oldhash = *h; /* shallow copy */ \
        h->keys = realloc_function(h->allocator, NULL, 0, \
                newcapacity * sizeof(Key)); \
        h->values = realloc_function(h->allocator, NULL, 0, \
                newcapacity * sizeof(Value)); \
        h->usedpos = realloc_function(h->allocator, NULL, 0, \
                funcprefix##getusedpossize(newcapacity)); \
        memset(h->usedpos, 0, funcprefix##getusedpossize(newcapacity)); \
        h->capacity = newcapacity; \
        h->size = 0; \
        for (i = 0; i < oldhash.capacity; ++i) \
            if (funcprefix##getusedpos(&oldhash, i)) \
                funcprefix##insert(h, oldhash.keys[i], oldhash.values[i]); \
        funcprefix##destroybuffers(&oldhash); \
    } \
static int funcprefix##growhashtable(typename *h) \
{ \
        if (funcprefix##computerequiredcapacity(h->size + 1) > h->capacity) { \
            funcprefix##resizehash(h, funcprefix##computerequiredcapacity(h->size * 2)); \
            return 1; \
        } else { \
            return 0; \
        } \
    } \
typename *funcprefix##create(size_t nelements) \
{ \
        return funcprefix##createwa(nelements, NULL); \
    } \
typename *funcprefix##createwa(size_t nelements, AllocatorType allocator) \
{ \
        typename *h; \
        if (nelements < 8) \
            nelements = 8; \
        h = realloc_function(allocator, NULL, 0, sizeof(typename)); \
        h->allocator = allocator; \
        h->keys = NULL; \
        h->values = NULL; \
        h->usedpos = NULL; \
        h->size = h->capacity = 0; \
        funcprefix##resizehash(h, funcprefix##computerequiredcapacity(nelements)); \
        return h; \
    } \
void funcprefix##destroy(typename *h) \
{ \
        if (!h) \
            return; \
        funcprefix##destroybuffers(h); \
        (void)realloc_function(h->allocator, h, sizeof(typename), 0); \
    } \
void funcprefix##clear(typename *h) \
{ \
        tscc_assert(h, "null hash table"); \
        memset(h->usedpos, 0, funcprefix##getusedpossize(h->capacity)); \
        h->size = 0; \
    } \
int funcprefix##empty(typename *h) \
{ \
        tscc_assert(h, "null hash table"); \
        return h->size == 0; \
    } \
size_t funcprefix##size(typename *h) \
{ \
        tscc_assert(h, "null hash table"); \
        return h->size; \
    } \
size_t funcprefix##maxsize(typename *h) \
{ \
        size_t maxsize; \
        tscc_assert(h, "null hash table"); \
        maxsize = 1 + h->capacity * TSCC_HASH_LOAD; \
        if (funcprefix##computerequiredcapacity(maxsize) > h->capacity) \
            return maxsize - 1; \
        else \
            return maxsize; \
    } \
void funcprefix##insert(typename *h, Key key, Value value) \
{ \
        int found; \
        size_t pos; \
        tscc_assert(h, "null hash table"); \
        pos = funcprefix##getposition(h, key, &found); \
        if (!found && funcprefix##growhashtable(h)) \
            pos = funcprefix##getposition(h, key, &found); \
        funcprefix##setusedpos(h, pos, 1); \
        h->keys[pos] = key; \
        h->values[pos] = value; \
        h->size++; \
    } \
int funcprefix##tryinsert(typename *h, Key key, Value value) \
{ \
        int found; \
        size_t pos; \
        tscc_assert(h, "null hash table"); \
        pos = funcprefix##getposition(h, key, &found); \
        if (!found) { \
            if (funcprefix##growhashtable(h)) \
                pos = funcprefix##getposition(h, key, &found); \
            funcprefix##setusedpos(h, pos, 1); \
            h->keys[pos] = key; \
            h->values[pos] = value; \
            h->size++; \
            return 1; \
        } else { \
            return 0; \
        } \
    } \
int funcprefix##find(typename *h, Key key, Value *value) \
{ \
        int found; \
        size_t pos; \
        tscc_assert(h, "null hash table"); \
        pos = funcprefix##getposition(h, key, &found); \
        if (found) \
            *value = h->values[pos]; \
        return found; \
    } \
int funcprefix##contains(typename *h, Key key) \
{ \
        Value v; \
        return funcprefix##find(h, key, &v); \
    } \
Value funcprefix##get(typename *h, Key key, Value def) \
{ \
        Value value; \
        int found = funcprefix##find(h, key, &value); \
        if (found) \
            return value; \
        else \
            return def; \
    } \
int funcprefix##erase(typename *h, Key key, Value *value) \
{ \
        int found; \
        size_t pos; \
        tscc_assert(h, "null hash table"); \
        pos = funcprefix##getposition(h, key, &found); \
        if (found) { \
            if (value) \
                *value = h->values[pos]; \
            funcprefix##setusedpos(h, pos, 0); \
            h->size--; \
        } \
        return found; \
    } \

/* HashTable declaration end */
#endif
