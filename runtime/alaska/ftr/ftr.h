#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// FTR is a simple trace writer "Function TRace" that produces Fuchsia FXT
// trace files from a simple API. It is designed to be used in high performance
// code with many threads without a huge overhead.
//
// Tracing starts automatically on program init. Output goes to trace.fxt by
// default. Control via environment variables:
//   FTR_TRACE_PATH  — override the output file path
//   FTR_DISABLE     — set to any value to disable tracing entirely
#define FTR_MIN_SCOPE_DURATION_NS 0

extern void ftr_close();
extern void ftr_debug_dump(void);

// An FXT trace atom.
typedef uint64_t ftr_atom_t;
typedef uint64_t ftr_timestamp_t;
typedef uint8_t ftr_thread_t; // 1-based thread index
typedef uint16_t ftr_str_t;   // 1-based string index

extern void ftr_write_span(uint64_t pid, uint64_t tid, const char *name,
                           ftr_timestamp_t start_ns, ftr_timestamp_t end_ns);
extern void ftr_write_spani(uint16_t name_ref, ftr_timestamp_t start_ns,
                            ftr_timestamp_t end_ns);
extern void ftr_write_marki(uint16_t name_ref);
extern void ftr_write_counteri(uint16_t name_ref, int64_t value);
extern void ftr_write_flow_begini(uint16_t name_ref, uint64_t flow_id);
extern void ftr_write_flow_stepi(uint16_t name_ref, uint64_t flow_id);
extern void ftr_write_flow_endi(uint16_t name_ref, uint64_t flow_id);
extern uint64_t ftr_new_flow_id(void);
extern uint16_t ftr_intern_string(const char *s);

// Like printf, but emits to a mark point in the trace location.  This is useful
// for debugging and adding ad-hoc events to the trace.  The overhead is pretty
// high (~100ns on my mac). As such, FTR_NO_TRACE disables codegen entirely.
extern void ftr_logf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

extern void ftr_set_process_name(const char *name);

extern void ftr_begin(const char *cat, const char *msg);
extern void ftr_end(const char *cat, const char *msg);

// Nanosecond timestamp from a monotonic clock.
extern ftr_timestamp_t ftr_now_ns(void);

struct ftr_event_t {
  ftr_str_t name_ref;
  ftr_timestamp_t start_ns;
};

///
static inline struct ftr_event_t ftr_begin_event(ftr_str_t name_ref_cache) {
  return (struct ftr_event_t){name_ref_cache, ftr_now_ns()};
}

static inline void ftr_end_event(struct ftr_event_t *e) {
  ftr_timestamp_t end = ftr_now_ns();
  if (end - e->start_ns < FTR_MIN_SCOPE_DURATION_NS)
    return;
  ftr_write_spani(e->name_ref, e->start_ns, end);
}

#ifdef FTR_NO_TRACE
#define FTR_SCOPE(name)
#define FTR_FUNCTION()
#define FTR_MARK(name)
#define FTR_COUNTER(name, value)
#define FTR_SCOPE_FLOW_BEGIN(name, flow_id)
#define FTR_SCOPE_FLOW_STEP(name, flow_id)
#define FTR_SCOPE_FLOW_END(name, flow_id)
#define ftr_logf(msg, ...) /* nothing. */
#else
#define FTR_CONCAT_(a, b) a##b
#define FTR_CONCAT(a, b) FTR_CONCAT_(a, b)
#define FTR_SCOPE(name)                                                        \
  static ftr_str_t FTR_CONCAT(__idx_, __LINE__) = 0;                           \
  if (FTR_CONCAT(__idx_, __LINE__) == 0)                                       \
    FTR_CONCAT(__idx_, __LINE__) = ftr_intern_string(name);                    \
  __attribute__((cleanup(ftr_end_event))) struct ftr_event_t FTR_CONCAT(       \
      __event_, __LINE__) = ftr_begin_event(FTR_CONCAT(__idx_, __LINE__))

// __func__ has a stable per-function pointer in practice (it's a static local
// array), so we can use the same static-cache trick as FTR_SCOPE.
#define FTR_FUNCTION() FTR_SCOPE(__PRETTY_FUNCTION__)

#define FTR_MARK(name)                                                         \
  do {                                                                         \
    static ftr_str_t FTR_CONCAT(__idx_, __LINE__) = 0;                         \
    if (FTR_CONCAT(__idx_, __LINE__) == 0)                                     \
      FTR_CONCAT(__idx_, __LINE__) = ftr_intern_string(name);                  \
    ftr_write_marki(FTR_CONCAT(__idx_, __LINE__));                             \
  } while (0)

#define FTR_COUNTER(name, value)                                               \
  do {                                                                         \
    static ftr_str_t FTR_CONCAT(__idx_, __LINE__) = 0;                         \
    if (FTR_CONCAT(__idx_, __LINE__) == 0)                                     \
      FTR_CONCAT(__idx_, __LINE__) = ftr_intern_string(name);                  \
    ftr_write_counteri(FTR_CONCAT(__idx_, __LINE__), (int64_t)(value));        \
  } while (0)

#define FTR_SCOPE_FLOW_BEGIN(name, flow_id)                                    \
  static ftr_str_t FTR_CONCAT(__idx_, __LINE__) = 0;                           \
  if (FTR_CONCAT(__idx_, __LINE__) == 0)                                       \
    FTR_CONCAT(__idx_, __LINE__) = ftr_intern_string(name);                    \
  __attribute__((cleanup(ftr_end_event))) struct ftr_event_t FTR_CONCAT(       \
      __event_, __LINE__) = ftr_begin_event(FTR_CONCAT(__idx_, __LINE__));     \
  ftr_write_flow_begini(FTR_CONCAT(__idx_, __LINE__),                          \
                        (uint64_t)(uintptr_t)(flow_id))

#define FTR_SCOPE_FLOW_STEP(name, flow_id)                                     \
  static ftr_str_t FTR_CONCAT(__idx_, __LINE__) = 0;                           \
  if (FTR_CONCAT(__idx_, __LINE__) == 0)                                       \
    FTR_CONCAT(__idx_, __LINE__) = ftr_intern_string(name);                    \
  __attribute__((cleanup(ftr_end_event))) struct ftr_event_t FTR_CONCAT(       \
      __event_, __LINE__) = ftr_begin_event(FTR_CONCAT(__idx_, __LINE__));     \
  ftr_write_flow_stepi(FTR_CONCAT(__idx_, __LINE__),                           \
                       (uint64_t)(uintptr_t)(flow_id))

#define FTR_SCOPE_FLOW_END(name, flow_id)                                      \
  static ftr_str_t FTR_CONCAT(__idx_, __LINE__) = 0;                           \
  if (FTR_CONCAT(__idx_, __LINE__) == 0)                                       \
    FTR_CONCAT(__idx_, __LINE__) = ftr_intern_string(name);                    \
  __attribute__((cleanup(ftr_end_event))) struct ftr_event_t FTR_CONCAT(       \
      __event_, __LINE__) = ftr_begin_event(FTR_CONCAT(__idx_, __LINE__));     \
  ftr_write_flow_endi(FTR_CONCAT(__idx_, __LINE__),                            \
                      (uint64_t)(uintptr_t)(flow_id))

#endif
#ifdef __cplusplus
}
#endif