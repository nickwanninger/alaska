#include "sim/trace.h"
#include <stdio.h>
#include <zlib.h>

static gzFile gzfile;

int init_tracer(char *file_name) {
  gzfile = gzopen(file_name, "wb");

  if (gzfile == NULL) {
    perror("gzopen");
    return 1;
  }

  return 0;
}

void deinit_tracer() { gzclose(gzfile); }

void alaska_trace_halloc(uint32_t hid, size_t requested_size) {
  struct halloc_trace_ty halloc_op = {.requested_size = requested_size,
                                      .hid = hid};

  gzwrite(gzfile, &halloc_op, sizeof(halloc_op));
}

void alaska_trace_hfree(uint32_t hid) {
  struct hfree_trace_ty hfree_op = {.hid = hid};

  gzwrite(gzfile, &hfree_op, sizeof(hfree_op));
}

void alaska_trace_translation(enum mem_op_ty m_op, uint32_t t_id, void *haddr,
                              size_t access_width) {
  struct mem_op_trace_ty mem_op = {
      .m_op = m_op, .t_id = t_id, .haddr = haddr, .access_width = access_width};

  gzwrite(gzfile, &mem_op, sizeof(mem_op));
}
