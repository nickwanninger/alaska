#include "sim/trace.h"
#include <pthread.h>
#include <stdio.h>
#include <zlib.h>

static gzFile gzfile;
static pthread_mutex_t gzfile_lock = PTHREAD_MUTEX_INITIALIZER;

__attribute__((noinline)) int init_tracer(const char *file_name) {
  pthread_mutex_init(&gzfile_lock, NULL);
  pthread_mutex_lock(&gzfile_lock);

  gzfile = gzopen(file_name, "wb");

  if (gzfile == NULL) {
    pthread_mutex_unlock(&gzfile_lock);
    perror("gzopen");
    return 1;
  }

  pthread_mutex_unlock(&gzfile_lock);
  return 0;
}

__attribute__((noinline)) void deinit_tracer() {
  pthread_mutex_lock(&gzfile_lock);
  gzclose(gzfile);
  pthread_mutex_unlock(&gzfile_lock);
}

__attribute__((noinline)) void
alaska_trace_halloc(uint64_t haddr, uint64_t vaddr, size_t requested_size) {
  uint32_t hid = ((uint64_t)haddr >> 32) & 0x7FFFFFFF;
  struct mem_op_trace_ty mem_op = {
      .haddr = (uintptr_t)hid,
      .vaddr = (uintptr_t)vaddr,
      .access_width = requested_size,
      .m_op = alloc,
  };

  // dump_mem_op(&mem_op);

  pthread_mutex_lock(&gzfile_lock);
  gzwrite(gzfile, &mem_op, sizeof(mem_op));
  pthread_mutex_unlock(&gzfile_lock);
}

__attribute__((noinline)) void alaska_trace_hfree(uint64_t haddr) {
  uint32_t hid = ((uint64_t)haddr >> 32) & 0x7FFFFFFF;
  struct mem_op_trace_ty mem_op = {.haddr = (uintptr_t)hid, .m_op = dealloc};

  // dump_mem_op(&mem_op);

  pthread_mutex_lock(&gzfile_lock);
  gzwrite(gzfile, &mem_op, sizeof(mem_op));
  pthread_mutex_unlock(&gzfile_lock);
}

__attribute__((noinline)) void alaska_trace_translation(enum mem_op_ty m_op,
                                                        uint32_t t_id,
                                                        uint64_t haddr,
                                                        size_t access_width) {
  struct mem_op_trace_ty mem_op = {.haddr = (uintptr_t)haddr,
                                   .access_width = access_width,
                                   .t_id = t_id,
                                   .m_op = m_op};

  // dump_mem_op(&mem_op);

  pthread_mutex_lock(&gzfile_lock);
  gzwrite(gzfile, &mem_op, sizeof(mem_op));
  pthread_mutex_unlock(&gzfile_lock);
}

void dump_mem_op(const mem_op_trace_ty *m_op) {
  printf("m_op: %d, h_addr: %p, access_width: %zu, t_id: %u\n", m_op->m_op,
         (void *)m_op->haddr, m_op->access_width, m_op->t_id);
}
