#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cstdio>

#ifdef ALASKA_TRANSLATION_TRACING
enum mem_op_ty { ld, st, alloc, dealloc };

struct mem_op_trace_ty {
  uintptr_t haddr;      // hid if m_op is alloc or free
  uintptr_t vaddr;      // hid if m_op is alloc or free
  size_t access_width;  // requested_size if m_op is alloc
  uint32_t t_id;
  enum mem_op_ty m_op;
};

void dump_mem_op(const mem_op_trace_ty *m_op);

int init_tracer(const char *file_name);
void deinit_tracer();

void alaska_trace_halloc(uint64_t haddr, uint64_t vaddr, size_t requested_size);
void alaska_trace_hfree(uint64_t haddr);
void alaska_trace_translation(
    enum mem_op_ty m_op, uint32_t t_id, uint64_t haddr, size_t access_width);
#endif
