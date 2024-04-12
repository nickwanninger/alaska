#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef ALASKA_TRANSLATION_TRACING
enum mem_op_ty { ld, st };

struct mem_op_trace_ty {
  void *haddr;
  size_t access_width;
  uint32_t t_id;
  enum mem_op_ty m_op;
};

struct halloc_trace_ty {
  size_t requested_size;
  uint32_t hid;
};

struct hfree_trace_ty {
  uint32_t hid;
};

int init_tracer(char *file_name);
void deinit_tracer();

void alaska_trace_halloc(uint32_t hid, size_t requested_size);
void alaska_trace_hfree(uint32_t hid);
void alaska_trace_translation(enum mem_op_ty m_op, uint32_t t_id, void *haddr, size_t access_width);
#endif
