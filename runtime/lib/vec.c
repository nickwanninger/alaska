/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <string.h>
#include <stdlib.h>

#include <alaska/vec.h>


static void ensure_cap(alaska_vec_t *vec, size_t cap) {
  if (vec->cap < cap) {
    if (vec->cap == 0) {
			vec->cap = 1;
    }

		while (vec->cap < cap) {
			vec->cap *= 2;
		}
    vec->data = realloc(vec->data, sizeof(void *) * vec->cap);
  }
}

void alaska_vec_init(alaska_vec_t *vec) {
  memset(vec, 0, sizeof(*vec));
  vec->cap = vec->length = 0;
  vec->data = NULL;
  ensure_cap(vec, 16);
}

void alaska_vec_free(alaska_vec_t *vec) { free(vec->data); }

void alaska_vec_push(alaska_vec_t *vec, void *val) {
  ensure_cap(vec, vec->length + 1);
  vec->data[vec->length] = val;
  vec->length++;
}
