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
#pragma once
#include <stdlib.h>


typedef struct {
	size_t length;
	size_t cap;
	void **data;
} alaska_vec_t;



extern void alaska_vec_init(alaska_vec_t *vec);
extern void alaska_vec_free(alaska_vec_t *vec);
extern void alaska_vec_push(alaska_vec_t *vec, void *val);

