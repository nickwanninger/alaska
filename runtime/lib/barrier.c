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

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service.h>

extern uint64_t alaska_next_usage_timestamp;


extern void trace_defrag_hook(void* oldptr, void* newptr, size_t size);


void (*alaska_barrier_hook)(void*, void*, size_t) = NULL;


// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
__declspec(noinline) void alaska_barrier(void) {
	alaska_service_barrier();
}
