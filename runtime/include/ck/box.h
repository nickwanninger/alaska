#pragma once

#include <ck/template_lib.h>
#include <alaska/alaska.hpp>


namespace ck {
  // Takes in a default deleter. unique_ptr_deleters operator will be invoked by
  // default.
  template <typename T>
  class box {
   protected:
    T* m_ptr = nullptr;

   public:
    // Safely constructs resource. Operator new is called by the user. Once
    // constructed the unique_ptr will own the resource. std::move is used because
    // it is used to indicate that an object may be moved from other resource.
    explicit box(T* raw_resource) noexcept
        : m_ptr(move(raw_resource)) {
    }

    box() noexcept
        : m_ptr(nullptr) {
    }

    // destroys the resource when object goes out of scope. This will either call
    // the default delete operator or a user-defined one.
    ~box() noexcept {
      delete m_ptr;
    }
    // Disables the copy/ctor and copy assignment operator. We cannot have two
    // copies exist or it'll bypass the RAII concept.
    box(const ck::box<T>&) noexcept = delete;
    box& operator=(const box&) noexcept = delete;

    template <typename U>
    box(const ck::box<U>&) = delete;

    template <typename U>
    box& operator=(const ck::box<U>&) = delete;

   public:
    // Allows move-semantics. While the unique_ptr cannot be copied it can be
    // safely moved.
    box(box<T>&& move) noexcept {
      move.swap(*this);
    }
    // ptr = std::move(resource)
    ck::box<T>& operator=(box<T>&& move) noexcept {
      move.swap(*this);
      return *this;
    }

    template <typename U>
    box(box<U>&& other) noexcept {
      if (this != static_cast<void*>(&other)) {
        delete m_ptr;
        m_ptr = other.leak_ptr();
      }
    }

    template <typename U>
    box& operator=(box<U>&& other) {
      if (this != static_cast<void*>(&other)) {
        delete m_ptr;
        m_ptr = other.leak_ptr();
      }
      return *this;
    }

    ck::box<T>& operator=(T* ptr) {
      if (m_ptr != ptr) delete m_ptr;
      m_ptr = ptr;
      return *this;
    }

    explicit operator bool() const noexcept {
      return this->m_ptr;
    }
    // releases the ownership of the resource. The user is now responsible for
    // memory clean-up.
    T* release() noexcept {
      return exchange(m_ptr, nullptr);
    }
    // returns a pointer to the resource
    T* get() const noexcept {
      return m_ptr;
    }

    // swaps the resources
    void swap(box<T>& resource_ptr) noexcept {
      ck::swap(m_ptr, resource_ptr.m_ptr);
    }
    // replaces the resource. the old one is destroyed and a new one will take
    // it's place.
    void reset(T* resource_ptr) noexcept(false) {
      // ensure a invalid resource is not passed or program will be terminated
      if (resource_ptr == nullptr) {
        ALASKA_ASSERT(false, "An invalid pointer was passed, resources will not be swapped");
      }

      delete m_ptr;

      m_ptr = nullptr;

      ck::swap(m_ptr, resource_ptr);
    }

    T* leak_ptr() {
      T* leaked_ptr = m_ptr;
      m_ptr = nullptr;
      return leaked_ptr;
    }

   public:
    // overloaded operators
    T* operator->() const noexcept {
      return this->m_ptr;
    }
    T& operator*() const noexcept {
      return *this->m_ptr;
    }
    // May be used to check for nullptr
  };

  template <typename T, typename... Args>
  ck::box<T> make_box(Args&&... args) {
    return ck::box<T>(new T(ck::forward<Args>(args)...));
  }

  // Like a box, but when you try to access it, it will default-construct the
  // resource if it doesn't already exist.
  template <typename T>
  class lazy_box {
   protected:
    T* m_ptr = nullptr;

   public:
    // Safely constructs resource. Operator new is called by the user. Once
    // constructed the unique_ptr will own the resource. std::move is used because
    // it is used to indicate that an object may be moved from other resource.
    explicit lazy_box(T* raw_resource) noexcept
        : m_ptr(move(raw_resource)) {
    }
    lazy_box(nullptr_t)
        : m_ptr(nullptr) {
    }

    lazy_box() noexcept
        : m_ptr(nullptr) {
    }

    // destroys the resource when object goes out of scope. This will either call
    // the default delete operator or a user-defined one.
    ~lazy_box() noexcept {
      if (m_ptr) delete m_ptr;
    }
    // Disables the copy/ctor and copy assignment operator. We cannot have two
    // copies exist or it'll bypass the RAII concept.
    lazy_box(const ck::lazy_box<T>&) noexcept = delete;
    lazy_box& operator=(const lazy_box&) noexcept = delete;

    template <typename U>
    lazy_box(const ck::lazy_box<U>&) = delete;

    template <typename U>
    lazy_box& operator=(const ck::lazy_box<U>&) = delete;




   public:
    // Allows move-semantics. While the unique_ptr cannot be copied it can be
    // safely moved.
    lazy_box(lazy_box<T>&& move) noexcept {
      move.swap(*this);
    }
    // ptr = std::move(resource)
    ck::lazy_box<T>& operator=(lazy_box<T>&& move) noexcept {
      move.swap(*this);
      return *this;
    }

    template <typename U>
    lazy_box(lazy_box<U>&& other) noexcept {
      if (this != static_cast<void*>(&other)) {
        if (m_ptr) delete m_ptr;
        m_ptr = other.leak_ptr();
      }
    }

    template <typename U>
    lazy_box(box<U>&& other) noexcept {
      if (this != static_cast<void*>(&other)) {
        if (m_ptr) delete m_ptr;
        m_ptr = other.leak_ptr();
      }
    }

    template <typename U>
    lazy_box& operator=(lazy_box<U>&& other) {
      if (this != static_cast<void*>(&other)) {
        if (m_ptr) delete m_ptr;
        m_ptr = other.leak_ptr();
      }
      return *this;
    }

    template <typename U>
    lazy_box& operator=(box<U>&& other) {
      if (this != static_cast<void*>(&other)) {
        if (m_ptr) delete m_ptr;
        m_ptr = other.leak_ptr();
      }
      return *this;
    }

    ck::lazy_box<T>& operator=(T* ptr) {
      if (m_ptr != ptr && m_ptr) delete m_ptr;
      m_ptr = ptr;
      return *this;
    }

    explicit operator bool() const noexcept {
      return this->m_ptr;
    }
    // releases the ownership of the resource. The user is now responsible for
    // memory clean-up.
    T* release() noexcept {
      return exchange(m_ptr, nullptr);
    }
    // returns a pointer to the resource
    T* get(void) {
      if (m_ptr == nullptr) m_ptr = new T();
      return m_ptr;
    }
    // swaps the resources
    void swap(lazy_box<T>& resource_ptr) noexcept {
      ck::swap(m_ptr, resource_ptr.m_ptr);
    }
    // replaces the resource. the old one is destroyed and a new one will take
    // it's place.
    void reset(T* resource_ptr) noexcept(false) {
      // ensure a invalid resource is not passed or program will be terminated
      if (resource_ptr == nullptr) {
        ALASKA_ASSERT(false, "An invalid pointer was passed, resources will not be swapped");
      }

      if (m_ptr) delete m_ptr;

      m_ptr = nullptr;

      ck::swap(m_ptr, resource_ptr);
    }

    T* leak_ptr() {
      T* leaked_ptr = m_ptr;
      m_ptr = nullptr;
      return leaked_ptr;
    }

    // overloaded operators
    T* operator->() {
      return get();
    }
    T& operator*() {
      return *get();
    }
    // May be used to check for nullptr
  };

}  // namespace ck