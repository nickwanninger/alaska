#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "./config.h"

#ifdef ALASKA_SERVICE_NONE
#include <alaska/service/none.h>
#endif

#ifdef ALASKA_SERVICE_ANCHORAGE
#include <alaska/service/anchorage.h>
#endif

#ifdef ALASKA_SERVICE_NONE
#include <alaska/service/none.h>
#endif

#include <alaska/list_head.h>


// extra fields to place in the handle table
#ifndef ALASKA_SERVICE_FIELDS
#define ALASKA_SERVICE_FIELDS  // ... nothing ...
#endif


#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

#define ALASKA_EXPORT __attribute__((visibility("default")))

#define ALASKA_INLINE __attribute__((always_inline))




// This file contains structures that the the translation subsystem uses
#ifdef __cplusplus
extern "C" {
#endif


extern void alaska_dump_backtrace(void);

#ifdef ALASKA_SANITY_CHECK
#define ALASKA_SANITY(c, msg, ...)                                                              \
  do {                                                                                          \
    if (!(c)) { /* if the check is not true... */                                               \
      fprintf(stderr, "\x1b[31m-----------[ Alaska Sanity Check Failed ]-----------\x1b[0m\n"); \
      fprintf(stderr, "%s line %d\n", __FILE__, __LINE__);                                      \
      fprintf(stderr, "Check, `%s`, failed\n", #c, ##__VA_ARGS__);                              \
      fprintf(stderr, msg "\n", ##__VA_ARGS__);                                                 \
      alaska_dump_backtrace();                                                                  \
      fprintf(stderr, "\x1b[31m                      Bailing!\x1b[0m\n");                       \
      fprintf(stderr, "\x1b[31m----------------------------------------------------\x1b[0m\n"); \
      exit(EXIT_FAILURE);                                                                       \
    }                                                                                           \
  } while (0);
#else
#define ALASKA_SANITY(c, msg, ...) /* do nothing if it's disabled */
#endif

#define ALASKA_ASSERT(c, msg, ...)                                                              \
  do {                                                                                          \
    if (!(c)) { /* if the check is not true... */                                               \
      fprintf(stderr, "\x1b[31m-----------[ Alaska Assert Failed ]-----------\x1b[0m\n");       \
      fprintf(stderr, "%s line %d\n", __FILE__, __LINE__);                                      \
      fprintf(stderr, "Check, `%s`, failed\n", #c, ##__VA_ARGS__);                              \
      fprintf(stderr, msg "\n", ##__VA_ARGS__);                                                 \
      alaska_dump_backtrace();                                                                  \
      fprintf(stderr, "\x1b[31mExiting.\x1b[0m\n");                                             \
      exit(EXIT_FAILURE);                                                                       \
      fprintf(stderr, "\x1b[31m----------------------------------------------------\x1b[0m\n"); \
    }                                                                                           \
  } while (0);


// The definition of what a handle's bits mean when they are used like a pointer
typedef union {
  struct {
    unsigned long offset : ALASKA_OFFSET_BITS;  // the offset into the handle
    unsigned long handle
        : (64 - 1 - ALASKA_OFFSET_BITS);  // the translation in the translation table
    unsigned flag : 1;                    // the high bit indicates the `ptr` is a handle
  };
  void *ptr;
} handle_t;

typedef struct {
  // Cache line 1:
  void *ptr;  // backing memory
  union {
    // If ptr is not 0, then size represents the size of the handle
    uint64_t size;
    // If ptr is zero, then the handle has been swapped out, and the following structure is used
    // instead:
    struct {
      uint64_t id;
    } swap;
  }
  // Cache line 2: Service specific fields
  ALASKA_SERVICE_FIELDS;
} alaska_mapping_t;

// src/lock.c
void *alaska_translate(void *ptr);
void alaska_release(void *ptr);

// Ensure a handle is present.
void *alaska_ensure_present(alaska_mapping_t *m);

alaska_mapping_t *alaska_lookup(void *ptr);

// src/halloc.c
void alaska_halloc_init(void);
void alaska_halloc_deinit(void);

// src/table.c
void alaska_table_init(void);    // initialize the global table
void alaska_table_deinit(void);  // teardown the global table
unsigned alaska_table_get_canonical(alaska_mapping_t *);
alaska_mapping_t *alaska_table_from_canonical(unsigned canon);
// allocate a handle id
alaska_mapping_t *alaska_table_get(void);
// free a handle id
void alaska_table_put(alaska_mapping_t *ent);
alaska_mapping_t *alaska_table_begin(void);
alaska_mapping_t *alaska_table_end(void);


// Generate a hash that can be displayed
// extern char *alaska_randomart(const unsigned char *raw, size_t raw_len, size_t fldbase, const
// char *palette);

typedef uint32_t alaska_spinlock_t;
#define ALASKA_SPINLOCK_INIT 0
inline void alaska_spin_lock(volatile alaska_spinlock_t *lock) {
  while (__sync_lock_test_and_set(lock, 1)) {
    // spin away
  }
}
inline int alaska_spin_try_lock(volatile alaska_spinlock_t *lock) {
  return __sync_lock_test_and_set(lock, 1) ? -1 : 0;
}
inline void alaska_spin_unlock(volatile alaska_spinlock_t *lock) {
  __sync_lock_release(lock);
}

#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_mapping_t *)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))
#define GET_OFFSET(handle) ((off_t)(handle)&0xFFFFFFFF)



struct alaska_lock_frame {
  struct alaska_lock_frame *prev;
  uint64_t count;
  void *locked[];
};
// In barrier.c
extern __thread struct alaska_lock_frame *alaska_lock_root_chain;

void alaska_barrier_add_thread(pthread_t *thread);
void alaska_barrier_remove_thread(pthread_t *thread);


// Signal from the leader that everyone needs to commit their locks and wait (From the leader)
void alaska_barrier_begin(void);
// release everyone and uncommit all your locks (From the leader)
void alaska_barrier_end(void);



// stolen from redis, it's just a nicer interface :)
#define atomic_inc(var, count) __atomic_add_fetch(&var, (count), __ATOMIC_SEQ_CST)
#define atomic_get_inc(var, oldvalue_var, count)                        \
  do {                                                                  \
    oldvalue_var = __atomic_fetch_add(&var, (count), __ATOMIC_RELAXED); \
  } while (0)
#define atomic_dec(var, count) __atomic_sub_fetch(&var, (count), __ATOMIC_SEQ_CST)
#define atomic_get(var, dstvar)                       \
  do {                                                \
    dstvar = __atomic_load_n(&var, __ATOMIC_RELAXED); \
  } while (0)
#define atomic_set(var, value) __atomic_store_n(&var, value, __ATOMIC_RELAXED)
#define atomic_get_sync(var, dstvar)                  \
  do {                                                \
    dstvar = __atomic_load_n(&var, __ATOMIC_SEQ_CST); \
  } while (0)
#define atomic_set_sync(var, value) __atomic_store_n(&var, value, __ATOMIC_SEQ_CST)


// nice kconfig info from linux
//
#define __ARG_PLACEHOLDER_1 0,
#define __take_second_arg(__ignored, val, ...) val

/*
 * The use of "&&" / "||" is limited in certain expressions.
 * The following enable to calculate "and" / "or" with macro expansion only.
 */
#define __and(x, y) ___and(x, y)
#define ___and(x, y) ____and(__ARG_PLACEHOLDER_##x, y)
#define ____and(arg1_or_junk, y) __take_second_arg(arg1_or_junk y, 0)

#define __or(x, y) ___or(x, y)
#define ___or(x, y) ____or(__ARG_PLACEHOLDER_##x, y)
#define ____or(arg1_or_junk, y) __take_second_arg(arg1_or_junk 1, y)

/*
 * Helper macros to use CONFIG_ options in C/CPP expressions. Note that
 * these only work with boolean and tristate options.
 */

/*
 * Getting something that works in C and CPP for an arg that may or may
 * not be defined is tricky.  Here, if we have "#define CONFIG_BOOGER 1"
 * we match on the placeholder define, insert the "0," for arg1 and generate
 * the triplet (0, 1, 0).  Then the last step cherry picks the 2nd arg (a one).
 * When CONFIG_BOOGER is not defined, we generate a (... 1, 0) pair, and when
 * the last step cherry picks the 2nd arg, we get a zero.
 */
#define __is_defined(x) ___is_defined(x)
#define ___is_defined(val) ____is_defined(__ARG_PLACEHOLDER_##val)
#define ____is_defined(arg1_or_junk) __take_second_arg(arg1_or_junk 1, 0)

/*
 * IS_MODULE(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'm', 0
 * otherwise.  CONFIG_FOO=m results in "#define CONFIG_FOO_MODULE 1" in
 * autoconf.h.
 */
#define IS_MODULE(option) __is_defined(option##_MODULE)

/*
 * IS_REACHABLE(CONFIG_FOO) evaluates to 1 if the currently compiled
 * code can call a function defined in code compiled based on CONFIG_FOO.
 * This is similar to IS_ENABLED(), but returns false when invoked from
 * built-in code when CONFIG_FOO is set to 'm'.
 */
#define IS_REACHABLE(option) \
  __or(IS_BUILTIN(option), __and(IS_MODULE(option), __is_defined(MODULE)))

/*
 * IS_ENABLED(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'y' or 'm',
 * 0 otherwise.  Note that CONFIG_FOO=y results in "#define CONFIG_FOO 1" in
 * autoconf.h, while CONFIG_FOO=m results in "#define CONFIG_FOO_MODULE 1".
 */
#define IS_ENABLED(option) __or(IS_BUILTIN(option), IS_MODULE(option))


#ifdef __cplusplus
}

namespace alaska {
  using Mapping = alaska_mapping_t;
};


#endif
