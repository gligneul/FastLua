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

/*
 * Vector and hashmap containers.
 */

#ifndef fl_container
#define fl_container

/* Default reallocation function. */
#ifndef flct_realloc
#define flct_realloc(allocator, ptr, oldsize, newsize) \
    ((newsize != 0) ? (realloc(ptr, newsize)) : (free(ptr), 0))
#endif

/*
 * Vector: dynamic array that grows automatically
 */
typedef struct FLVector FLVector;

/* Create an empty vector. Receive the init buffer size (to minimize
 * reallocation).
 * You can pass NULL to allocator if you want to use the default one. */
FLVector *flvec_create(size_t buffsize, void *allocator);

/* Destroy the vector. ATTENTION: don't free it's elements. */
void flvec_destroy(FLVector *v);

/* Return true if size equals to 0. */
bool flvec_empty(FLVector *v);

/* Return the number of elements. */
size_t flvec_size(FLVector *v);

/* Insert an element in the last position. */
void flvec_push(FLVector *v, void *value);

/* Insert an element at the required position */
void flvec_insert(FLVector *v, size_t pos, void *value);

/* Remove the last element and return it. */
void *flvec_pop(FLVector *v);

/* Return the elements at the last position. */
void *flvec_peek(FLVector *v);

/* Obtain the element at the required position. */
void *flvec_get(FLVector *v, size_t pos);

/* Set the element at the required position. */
void flvec_set(FLVector *v, size_t pos, void *value);

/* Change the vector size. */
void flvec_resize(FLVector *v, size_t size);

#endif

