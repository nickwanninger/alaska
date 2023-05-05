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

#include <anchorage/Block.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/Swapper.hpp>
#include <alaska/internal.h>
#include <ck/pair.h>
#include <ck/map.h>
#include <ck/lock.h>
#include <string.h>

extern "C" void alaska_service_swap_in(alaska_mapping_t *m) {
  anchorage::swap_in(*m);
}


void anchorage::swap_in(alaska::Mapping &m) {
	m.ptr = (void*)m.swap.id;
	auto block = anchorage::Block::get(m.ptr);
	m.size = block->size();
}


void anchorage::swap_out(alaska::Mapping &m) {
	void *ptr = m.ptr;
	// next translation will "fault" and figure out what to do.
	m.ptr = NULL;
	m.swap.id = (size_t)ptr;
}
