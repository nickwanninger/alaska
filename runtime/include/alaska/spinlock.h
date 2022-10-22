/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the
 * United States National  Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SPINLOCK_INITIALIZER 0

typedef unsigned int spinlock_t;

static inline void spinlock_init(volatile spinlock_t *lock) {
  *lock = SPINLOCK_INITIALIZER;
}

static inline void spinlock_deinit(volatile spinlock_t *lock) {
  *lock = 0;
}

static inline void spin_lock(volatile spinlock_t *lock) {
  while (__sync_lock_test_and_set(lock, 1)) {
    // spin away
  }
}

// returns zero on successful lock acquisition, -1 otherwise
static inline int spin_try_lock(volatile spinlock_t *lock) {
  return __sync_lock_test_and_set(lock, 1) ? -1 : 0;
}

void spin_lock_nopause(volatile spinlock_t *lock);

static inline void spin_unlock(volatile spinlock_t *lock) {
  // TX_PROFILE_ENTRY();
  __sync_lock_release(lock);
  // TX_PROFILE_EXIT();
}

// this expects the struct, not the pointer to it
#define TX_LOCK_GLBINIT(l) ((l) = SPINLOCK_INITIALIZER)
#define TX_LOCK_T spinlock_t
#define TX_LOCK_INIT(l) spinlock_init(l)
#define TX_LOCK(l) spin_lock(l)
#define TX_TRY_LOCK(l) spin_try_lock(l)
#define TX_UNLOCK(l) spin_unlock(l)
#define TX_LOCK_DEINIT(l) spinlock_deinit(l)

#ifdef __cplusplus
}
#endif

#endif
