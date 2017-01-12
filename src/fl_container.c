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

#include <assert.h>
#include <stdlib.h>

#include "fl_container.h"

struct FLVector {
  void *allocator;
  void **buffer;
  size_t size;
  size_t capacity;
} FLVector;

/* Resize the vector capacity */
void resizevector(FLVector *v, size_t newcapacity) {
  v->buffer = flct_realloc(v->allocator, v->buffer,
      v->capacity * sizeof(void *), newcapacity * sizeof(void *));
  v->capacity = newcapacity;
}

/* Updates the vector capacity if necessary */
void growvector(FLVector *v) {
  if (v->size + 1 > v->capacity)
    resizevector(v, v->capacity * 2);
}

FLVector *flvec_create(size_t buffsize, void *allocator) {
  FLVector *v = flct_realloc(allocator, NULL, 0, sizeof(FLVector));
  v->allocator = allocator;
  v->buffer = NULL;
  v->size = v->capacity = 0;
  resizevector(v, buffsize);
}

void flvec_destroy(FLVector *v) {
  assert(v);
  flct_realloc(v->allocator, v->buffer, v->capacity * sizeof(void *), 0);
  flct_realloc(v->allocator, v, sizeof(FLVector), 0);
}

bool flvec_empty(FLVector *v) {
  assert(v);
  return v->size == 0;
}

size_t flvec_size(FLVector *v) {
  assert(v);
  return v->size;
}

void flvec_push(FLVector *v, void *value) {
  assert(v);
  growvector(v);
  v->buffer[v->size++] = value;
}

void flvec_insert(FLVector *v, size_t pos, void *value) {
  size_t i;
  assert(v);
  assert(pos < v->size);
  growvector(v);
  for (i = v->size; i > pos; ++i)
    v->buffer[i] = v->buffer[i - 1];
  v->buffer[pos] = value;
  v->size++;
}

void *flvec_pop(FLVector *v) {
  assert(v);
  assert(v->size > 0);
  return v->buffer[--v->size];
}

void *flvec_peek(FLVector *v) {
  assert(v);
  assert(v->size > 0);
  return v->buffer[v->size - 1];
}

void *flvec_get(FLVector *v, size_t pos) {
  assert(v);
  assert(pos < v->size);
  return v->buffer[pos];
}

void flvec_set(FLVector *v, size_t pos, void *value) {
  assert(v);
  assert(pos < v->size);
  v->buffer[pos] = value;
}

void flvec_resize(FLVector *v, size_t size) {
  assert(v);
  if (size > v->capacity)
    resizevector(v, size);
  v->size = size;
}

#endif

