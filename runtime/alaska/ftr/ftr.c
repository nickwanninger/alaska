#include "ftr.h"
#include <assert.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef ftr_logf
#ifdef __APPLE__
static const char *os_getprogname(void) { return getprogname(); }
#elif defined(__linux__)
#include <errno.h>
extern char *program_invocation_short_name;
static const char *os_getprogname(void) {
  return program_invocation_short_name;
}
#else
static const char *os_getprogname(void) { return "process"; }
#endif

#define FXT_MAGIC 0x0016547846040010ULL
// Initialization record  (type = 1, always 2 words)
//
//  Word 0 — header:
//   [3:0]   type        = 1
//   [15:4]  size_words  = 2
//   [63:16] _reserved
//  Word 1 — ticks_per_second (we use nanoseconds, so 1 000 000 000)
typedef union {
  struct {
    uint64_t type : 4;        // = 1
    uint64_t size_words : 12; // = 2
    uint64_t _reserved : 48;
  };
  uint64_t raw;
} fxt_init_hdr;

// String record  (type = 2)
typedef union {
  struct {
    uint64_t type : 4;
    uint64_t size_words : 12;
    uint64_t str_index : 15;
    uint64_t _r0 : 1;
    uint64_t str_len : 15;
    uint64_t _r1 : 17;
  };
  uint64_t raw;
} fxt_string_hdr;

// Event record  (type = 4)
typedef union {
  struct {
    uint64_t type : 4;
    uint64_t size_words : 12;
    uint64_t event_type : 4;
    uint64_t arg_count : 4;
    uint64_t thread_ref : 8;
    uint64_t category_ref : 16;
    uint64_t name_ref : 16;
  };
  uint64_t raw;
} fxt_event_hdr;

// header word
// [0 .. 3]: record type (9)
// [4 .. 15]: record size (inclusive of this word) as a multiple of 8 bytes
// [16 .. 30]: log message length in bytes (range 0x0000 to 0x7fff)
// [31]: always zero (0)
// [32 .. 39]: thread (thread ref)
// [40 .. 63]: reserved (must be zero)
typedef union {
  struct {
    uint64_t type : 4;
    uint64_t size_words : 12;
    uint64_t msg_len : 15;
    uint64_t _r0 : 1;
    uint64_t thread_ref : 8;
    uint64_t _r1 : 24;
  };
  uint64_t raw;
} fxt_log_hdr;

#define FXT_MAX_STRINGS 0x7FFF // max unique interned strings
#define FXT_STRING_MAXLEN 63   // max string length in bytes

typedef struct {
  const char *key;
} ftr_intern_entry_t;

static ftr_intern_entry_t intern_pool[FXT_MAX_STRINGS];
static uint16_t intern_count = 0;

#define FTR_SHARED_BUF_SIZE (256 * 1024 * 1024) // 256 KB

static int trace_enabled = 0;
static ftr_write_fn g_write_fn = NULL;
static void *g_write_userdata = NULL;
static FILE *g_file_handle = NULL;
static int g_file_is_pipe = 0;
static uint8_t shared_buf[FTR_SHARED_BUF_SIZE];
static size_t shared_buf_pos = 0;
static atomic_flag shared_buf_lock = ATOMIC_FLAG_INIT;

static inline void buf_lock(void) {
  while (atomic_flag_test_and_set_explicit(&shared_buf_lock,
                                           memory_order_acquire)) {
  }
}

static inline void buf_unlock(void) {
  atomic_flag_clear_explicit(&shared_buf_lock, memory_order_release);
}

// ---------------------------------------------------------------------------
// Record-local staging helpers — build into a small stack buffer, then commit
// ---------------------------------------------------------------------------

typedef struct {
  uint8_t data[512]; // max FXT record size we'll ever produce
  size_t pos;
} ftr_record_t;

static inline void rec_u64(ftr_record_t *r, uint64_t v) {
  uint8_t b[8] = {
      (uint8_t)(v >> 0),  (uint8_t)(v >> 8),  (uint8_t)(v >> 16),
      (uint8_t)(v >> 24), (uint8_t)(v >> 32), (uint8_t)(v >> 40),
      (uint8_t)(v >> 48), (uint8_t)(v >> 56),
  };
  memcpy(r->data + r->pos, b, 8);
  r->pos += 8;
}

static inline void rec_str_padded(ftr_record_t *r, const char *s, size_t len) {
  size_t pad = (8 - len % 8) % 8;
  memcpy(r->data + r->pos, s, len);
  r->pos += len;
  if (pad) {
    memset(r->data + r->pos, 0, pad);
    r->pos += pad;
  }
}

static uint64_t g_ftr_pid = 0;

static _Atomic uint64_t next_local_thread_id = 0;
static __thread uint64_t g_ftr_tid = (uint64_t)-1;

static uint64_t get_local_thread_id(void) {
  if (g_ftr_tid == (uint64_t)-1) {
    g_ftr_tid = atomic_fetch_add(&next_local_thread_id, 1);
  }
  return g_ftr_tid;
}

// Forward declaration — flush_locked and buf_append_locked are mutually
// referential (buf_append_locked flushes when the buffer is full, and
// flush_locked appends a duration record after writing).
static void flush_locked(void);

// Append `len` bytes from `data` into the shared buffer, flushing first if
// there isn't enough room.  The entire `len` bytes are guaranteed to land in a
// single flush – a record is never split across two g_write_fn calls.
// Must be called with the lock held.
static void buf_append_locked(const void *data, size_t len) {
  if (shared_buf_pos + len > FTR_SHARED_BUF_SIZE) {
    flush_locked();
  }
  // Record larger than the whole buffer — write it directly so it stays atomic.
  if (len > FTR_SHARED_BUF_SIZE) {
    if (g_write_fn)
      g_write_fn(data, len, g_write_userdata);
    return;
  }
  memcpy(shared_buf + shared_buf_pos, data, len);
  shared_buf_pos += len;
}

// Must be called with the lock held.
static void flush_locked(void) {
  if (shared_buf_pos == 0)
    return;

  ftr_timestamp_t start_ns = ftr_now_ns();
  if (g_write_fn)
    g_write_fn(shared_buf, shared_buf_pos, g_write_userdata);
  shared_buf_pos = 0;
  ftr_timestamp_t end_ns = ftr_now_ns();

  // Record the flush itself as a duration event with inline name.
  static const char flush_name[] = "-flush-";
  size_t name_len = sizeof(flush_name) - 1;
  size_t name_words = (name_len + 7) / 8;
  size_t size_words = 1 + 3 + name_words + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 4;
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = (uint16_t)(0x8000 | name_len);
  ev.category_ref = 0;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, start_ns);
  rec_u64(&r, g_ftr_pid);
  rec_u64(&r, get_local_thread_id());
  rec_str_padded(&r, flush_name, name_len);
  rec_u64(&r, end_ns);
  buf_append_locked(r.data, r.pos);
}

static void commit_record(ftr_record_t *r) {
  if (!__atomic_load_n(&trace_enabled, __ATOMIC_RELAXED))
    return;
  buf_lock();
  buf_append_locked(r->data, r->pos);
  buf_unlock();
}

#if defined(__i386__) || defined(__x86_64__)
static inline uint64_t rdtsc(void) {
  uint32_t lo, hi;
  __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}
static uint64_t tsc_freq_calibrate(void) {
  // Warm up
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  // Take multiple samples and use the best (shortest wall time = least noise)
  uint64_t best_tsc_delta = 0;
  uint64_t best_ns_delta = 0;

  for (int i = 0; i < 50; i++) {
    struct timespec t1, t2;

    // Align to a clock tick boundary to reduce start jitter
    clock_gettime(CLOCK_MONOTONIC, &t1);
    struct timespec t1_check;
    do {
      clock_gettime(CLOCK_MONOTONIC, &t1_check);
    } while (t1_check.tv_nsec == t1.tv_nsec && t1_check.tv_sec == t1.tv_sec);
    t1 = t1_check;

    uint64_t tsc1 = rdtsc();

    // Sleep long enough to amortize clock granularity
    // 10ms gives ~0.01% accuracy at typical TSC frequencies
    struct timespec sleep = {0, 10000000}; // 10ms
    nanosleep(&sleep, NULL);

    uint64_t tsc2 = rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &t2);

    uint64_t ns_delta =
        (t2.tv_sec - t1.tv_sec) * 1000000000ULL + (t2.tv_nsec - t1.tv_nsec);
    uint64_t tsc_delta = tsc2 - tsc1;

    // Pick the iteration with smallest wall time (least scheduling noise)
    if (best_ns_delta == 0 || ns_delta < best_ns_delta) {
      best_ns_delta = ns_delta;
      best_tsc_delta = tsc_delta;
    }
  }

  // freq = ticks / seconds = tsc_delta / (ns_delta / 1e9)
  //      = tsc_delta * 1e9 / ns_delta
  return best_tsc_delta * 1000000000ULL / best_ns_delta;
}
#endif

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ftr_debug_dump(void) {}

static void file_write_fn(const void *data, size_t len, void *userdata) {
  fwrite(data, 1, len, (FILE *)userdata);
}

static void ftr_on_exit(void) {
  if (__atomic_load_n(&trace_enabled, __ATOMIC_RELAXED))
    ftr_close();
}

static void ftr_do_init(void) {
  g_ftr_pid = (uint64_t)getpid();
  intern_count = 0;

  uint64_t ticks_per_sec = 1000000000ULL;
#if defined(__i386__) || defined(__x86_64__)
  ticks_per_sec = tsc_freq_calibrate();
  printf("[ftr] Calibrated TSC frequency: %lu Hz\n",
         (unsigned long)ticks_per_sec);
#endif

  // Write header directly — trace_enabled is still 0, so commit_record would
  // drop it.
  ftr_record_t r = {.pos = 0};
  rec_u64(&r, FXT_MAGIC);
  fxt_init_hdr init = {0};
  init.type = 1;
  init.size_words = 2;
  rec_u64(&r, init.raw);
  rec_u64(&r, ticks_per_sec);
  g_write_fn(r.data, r.pos, g_write_userdata);

  __atomic_store_n(&trace_enabled, 1, __ATOMIC_RELEASE);
  ftr_set_process_name(os_getprogname());
  atexit(ftr_on_exit);
}

void ftr_init(ftr_write_fn write_fn, void *userdata) {
  if (__atomic_load_n(&trace_enabled, __ATOMIC_RELAXED))
    return;
  g_write_fn = write_fn;
  g_write_userdata = userdata;
  ftr_do_init();
}

void ftr_init_file(const char *path) {
  if (__atomic_load_n(&trace_enabled, __ATOMIC_RELAXED))
    return;
  if (!path)
    path = getenv("FTR_TRACE_PATH");
  if (!path)
    path = "trace.fxt.gz";

  size_t len = strlen(path);
  if (len > 3 && strcmp(path + len - 3, ".gz") == 0) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "gzip > '%s'", path);
    g_file_handle = popen(cmd, "w");
    g_file_is_pipe = 1;
  } else {
    g_file_handle = fopen(path, "wb");
    g_file_is_pipe = 0;
  }
  g_write_fn = file_write_fn;
  g_write_userdata = g_file_handle;
  ftr_do_init();
}

__attribute__((constructor)) static void ftr_auto_init(void) {
  if (getenv("FTR_DISABLE"))
    return;
  const char *path = getenv("FTR_TRACE_PATH");
  if (!path)
    return;
  ftr_init_file(path);
}

void ftr_close(void) {
  if (!__atomic_load_n(&trace_enabled, __ATOMIC_RELAXED))
    return;
  __atomic_store_n(&trace_enabled, 0, __ATOMIC_RELEASE);
  buf_lock();
  flush_locked();
  buf_unlock();
  if (g_file_handle) {
    if (g_file_is_pipe)
      pclose(g_file_handle);
    else
      fclose(g_file_handle);
    g_file_handle = NULL;
  }
  g_write_fn = NULL;
  g_write_userdata = NULL;
}

ftr_timestamp_t ftr_now_ns(void) {
#if defined(__i386__) || defined(__x86_64__)
  return rdtsc();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

  ftr_timestamp_t now =
      (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

  // Ensure each nanosecond measurement is unique, even if the clock doesn't
  // have nanosecond precision. This is a bit of a hack but it prevents issues
  // with nonsense zero-duration spans and strange ordering in perfetto.
  static uint64_t last = 0;
  if (now <= last)
    now = last + 1;
  last = now;
  return now;
#endif
}

void ftr_write_span(uint64_t pid, uint64_t tid, const char *name,
                    ftr_timestamp_t start_ns, ftr_timestamp_t end_ns) {

  const char *cat = "app";
  size_t cat_len = 3;
  size_t name_len = strlen(name);
  size_t cat_words = (cat_len + 7) / 8;
  size_t name_words = (name_len + 7) / 8;
  size_t size_words = 1 + 3 + name_words + cat_words + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 4;
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = (uint16_t)(0x8000 | name_len);
  ev.category_ref = (uint16_t)(0x8000 | cat_len);

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, start_ns);
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_str_padded(&r, cat, cat_len);
  rec_str_padded(&r, name, name_len);
  rec_u64(&r, end_ns);

  commit_record(&r);
}

uint16_t ftr_intern_string(const char *s) {
  if (!__atomic_load_n(&trace_enabled, __ATOMIC_RELAXED))
    return 0;
  for (uint16_t i = 0; i < intern_count; i++) {
    if (intern_pool[i].key == s)
      return i + 1;
  }

  assert(intern_count < FXT_MAX_STRINGS && "intern table full");

  size_t len = strlen(s);
  if (len > FXT_STRING_MAXLEN)
    len = FXT_STRING_MAXLEN;

  uint16_t idx = ++intern_count;
  intern_pool[idx - 1].key = s;

  size_t str_words = (len + 7) / 8;
  fxt_string_hdr sh = {0};
  sh.type = 2;
  sh.size_words = 1 + str_words;
  sh.str_index = idx;
  sh.str_len = (uint64_t)len;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, sh.raw);
  rec_str_padded(&r, s, len);
  commit_record(&r);

  return idx;
}

void ftr_write_spani(uint16_t name_ref, ftr_timestamp_t start_ns,
                     ftr_timestamp_t end_ns) {

  uint64_t pid = g_ftr_pid;
  uint64_t tid = get_local_thread_id();
  size_t size_words = 1 + 3 + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 4;
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = name_ref;
  ev.category_ref = 0;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, start_ns);
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_u64(&r, end_ns);

  commit_record(&r);
}

void ftr_write_counteri(uint16_t name_ref, int64_t value) {
  uint64_t pid = g_ftr_pid;
  uint64_t tid = get_local_thread_id();
  // header + timestamp + pid + tid + counter_id + arg_header + arg_value
  size_t size_words = 1 + 3 + 1 + 2;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 1; // counter
  ev.arg_count = 1;
  ev.thread_ref = 0;
  ev.name_ref = name_ref;
  ev.category_ref = 0;

  // Argument header: type=3 (int64), size=2 words, name_ref reuses name_ref
  uint64_t arg_hdr = 0;
  arg_hdr |= (uint64_t)3;              // type: int64
  arg_hdr |= (uint64_t)2 << 4;         // size_words: 2
  arg_hdr |= (uint64_t)name_ref << 16; // arg name

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, ftr_now_ns());
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_u64(&r, name_ref); // counter_id: use name_ref as stable id
  rec_u64(&r, value);
  rec_u64(&r, arg_hdr);

  commit_record(&r);
}

static _Atomic uint64_t next_flow_id = 1;

uint64_t ftr_new_flow_id(void) { return atomic_fetch_add(&next_flow_id, 1); }

static void ftr_write_flow_event(uint16_t name_ref, uint64_t flow_id,
                                 int event_type) {
  uint64_t pid = g_ftr_pid;
  uint64_t tid = get_local_thread_id();
  // header + timestamp + pid + tid + flow_correlation_id
  size_t size_words = 1 + 3 + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = event_type;
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = name_ref;
  ev.category_ref = 0;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, ftr_now_ns());
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_u64(&r, flow_id);

  commit_record(&r);
}

void ftr_write_flow_begini(uint16_t name_ref, uint64_t flow_id) {
  ftr_write_flow_event(name_ref, flow_id, 8);
}

void ftr_write_flow_stepi(uint16_t name_ref, uint64_t flow_id) {
  ftr_write_flow_event(name_ref, flow_id, 9);
}

void ftr_write_flow_endi(uint16_t name_ref, uint64_t flow_id) {
  ftr_write_flow_event(name_ref, flow_id, 10);
}

void ftr_write_marki(uint16_t name_ref) {
  uint64_t pid = g_ftr_pid;
  uint64_t tid = get_local_thread_id();
  size_t size_words = 1 + 3;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 0; // instant
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = name_ref;
  ev.category_ref = 0;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, ftr_now_ns());
  rec_u64(&r, pid);
  rec_u64(&r, tid);

  commit_record(&r);
}

void ftr_logf(const char *fmt, ...) {
  char msg[256];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  if (len < 0)
    return;
  if (len > 255)
    len = 255;

  uint64_t pid = g_ftr_pid;
  uint64_t tid = get_local_thread_id();

  size_t msg_words = (len + 7) / 8;
  size_t size_words = 1 + 3 + msg_words;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 0; // instant
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = (uint16_t)(0x8000 | len);
  ev.category_ref = 0;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, ftr_now_ns());
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_str_padded(&r, msg, len);

  commit_record(&r);
}

static inline void write_begin_end(int event_type, const char *cat,
                                   const char *msg) {
  uint64_t pid = g_ftr_pid;
  uint64_t tid = get_local_thread_id();

  int cat_len = (int)strlen(cat); // TODO: string wrapping/interning.
  int msg_len = (int)strlen(msg); // TODO: string wrapping/interning.
  size_t cat_words = (cat_len + 7) / 8;
  size_t msg_words = (msg_len + 7) / 8;
  size_t size_words = 1 + 3 + cat_words + msg_words;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.event_type = event_type;
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = (uint16_t)(0x8000 | msg_len);
  ev.category_ref = (uint16_t)(0x8000 | cat_len);
  ev.size_words = size_words;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, ftr_now_ns());
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_str_padded(&r, cat, cat_len);
  rec_str_padded(&r, msg, msg_len);

  commit_record(&r);
}

void ftr_begin(const char *cat, const char *msg) {
  write_begin_end(2, cat, msg);
}

void ftr_end(const char *cat, const char *msg) { write_begin_end(3, cat, msg); }

void ftr_set_process_name(const char *name) {
  if (!name)
    return;
  size_t name_len = strlen(name);
  if (name_len > 255)
    name_len = 255;
  size_t name_words = (name_len + 7) / 8;
  size_t size_words = 2 + name_words;

  uint64_t hdr = 0;
  hdr |= (uint64_t)7; // Record Type: Kernel Object
  hdr |= (uint64_t)size_words << 4;
  hdr |= (uint64_t)1 << 16;                   // Object Type: 1 (Process)
  hdr |= (uint64_t)(0x8000 | name_len) << 24; // Name string ref (inline)

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, hdr);
  rec_u64(&r, g_ftr_pid); // Word 1: Object ID
  rec_str_padded(&r, name, name_len);

  commit_record(&r);
}