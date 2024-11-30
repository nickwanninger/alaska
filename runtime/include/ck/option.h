#pragma once


#include <stdint.h>
#include <assert.h>
#include <ck/utility.h>

namespace ck {
#define ALWAYS_INLINE [[gnu::always_inline]] inline

  template <typename T>
  class alignas(T) opt {
   public:
    opt() {}

    opt(const T& value)
        : m_has_value(true) {
      new (&m_storage) T(value);
    }

    template <typename U>
    opt(const U& value)
        : m_has_value(true) {
      new (&m_storage) T(value);
    }

    opt(T&& value)
        : m_has_value(true) {
      new (&m_storage) T(move(value));
    }

    opt(opt&& other)
        : m_has_value(other.m_has_value) {
      if (other.has_value()) {
        new (&m_storage) T(other.take());
        other.m_has_value = false;
      }
    }

    opt(const opt& other)
        : m_has_value(other.m_has_value) {
      if (m_has_value) {
        new (&m_storage) T(other.value_without_consume_state());
      }
    }

    opt& operator=(const opt& other) {
      if (this != &other) {
        clear();
        m_has_value = other.m_has_value;
        if (m_has_value) {
          new (&m_storage) T(other.unwrap());
        }
      }
      return *this;
    }

    opt& operator=(opt&& other) {
      if (this != &other) {
        clear();
        m_has_value = other.m_has_value;
        if (other.has_value()) new (&m_storage) T(other.take());
      }
      return *this;
    }

    ALWAYS_INLINE ~opt() { clear(); }

    ALWAYS_INLINE void clear() {
      if (m_has_value) {
        unwrap().~T();
        m_has_value = false;
      }
    }

    ALWAYS_INLINE bool has_value() const { return m_has_value; }
    ALWAYS_INLINE operator bool(void) const { return m_has_value; }


    template <typename Fn>
    ALWAYS_INLINE auto map(Fn cb) -> ck::opt<decltype(cb(T{}))> {
      if (m_has_value) {
        return cb(unwrap());
      }
      return {};
    }

    T take() {
      assert(m_has_value);

      T released_value = move(unwrap());
      unwrap().~T();
      m_has_value = false;
      return released_value;
    }
    ALWAYS_INLINE T or_default(const T fallback) {
      if (m_has_value) return take();
      return fallback;
    }

    ALWAYS_INLINE T& unwrap() {
      assert(m_has_value);
      return *reinterpret_cast<T*>(&m_storage);
    }

    ALWAYS_INLINE const T& unwrap() const {
      assert(m_has_value);
      return value_without_consume_state();
    }


    ALWAYS_INLINE T& unwrap_or(const T& fallback) const {
      if (m_has_value) return unwrap();
      return fallback;
    }



   private:
    // Call when we don't want to alter the consume state
    ALWAYS_INLINE const T& value_without_consume_state() const {
      // ASSERT(m_has_value);
      return *reinterpret_cast<const T*>(&m_storage);
    }
    uint8_t m_storage[sizeof(T)]{0};
    bool m_has_value = false;
  };


#define Some(val) ck::opt<typename ck::remove_reference<decltype(val)>::type>(val)
#define None \
  {}

#undef ALWAYS_INLINE
}  // namespace ck
