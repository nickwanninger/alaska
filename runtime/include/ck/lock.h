#pragma once

#include <pthread.h>

namespace ck {

  class mutex {
   private:
    pthread_mutex_t m_mutex;

   public:
    mutex(void) {
      pthread_mutex_init(&m_mutex, NULL);
    }

    void init(void) {
      pthread_mutex_init(&m_mutex, NULL);
    }
    ~mutex(void) {
      pthread_mutex_destroy(&m_mutex);
    }
    int try_lock(void) {
      return pthread_mutex_trylock(&m_mutex);
    }

    void lock(void) {
      pthread_mutex_lock(&m_mutex);
    }
    void unlock(void) {
      pthread_mutex_unlock(&m_mutex);
    }
  };


  class scoped_lock {
    ck::mutex &lck;
    bool locked = false;

   public:
    inline scoped_lock(ck::mutex &lck)
        : lck(lck) {
      locked = true;
      lck.lock();
    }

    inline ~scoped_lock(void) {
      unlock();
    }

    inline void unlock(void) {
      if (locked) lck.unlock();
    }
  };
}  // namespace ck
