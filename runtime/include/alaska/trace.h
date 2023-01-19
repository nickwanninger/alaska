#pragma once


#include <alaska/internal.h>


struct alaska_trace_alloc {
	uint8_t type; // 'A'
	uint64_t ptr;
	uint64_t size;
};

struct alaska_trace_realloc {
	uint8_t type; // 'R'
	uint64_t old_ptr;
	uint64_t new_ptr;
	uint64_t new_size;
};

struct alaska_trace_free {
	uint8_t type; // 'F'
	uint64_t ptr;
};

struct alaska_trace_lock {
	uint8_t type; // 'L'
	uint64_t ptr;
};

struct alaska_trace_unlock {
	uint8_t type; // 'U'
	uint64_t ptr;
};


struct alaska_trace_classify {
	uint8_t type; // 'C'
	uint64_t ptr;
	uint8_t class_id;
};


struct alaska_trace_barrier {
	uint8_t type; // 'B'
};
