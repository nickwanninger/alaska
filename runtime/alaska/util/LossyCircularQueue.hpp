#include <ck/util.h>
#include <ck/utility.h>
#include <alaska/alaska.hpp>

namespace alaska {
  template <typename T, int N>
  class LossyCircularQueue {
   public:
    LossyCircularQueue()
        : head_(0)
        , tail_(0)
        , size_(0) {}

    // Push to back — drops oldest item if full
    void push(const T& value) {
      if (full()) drop_oldest();
      data_[tail_] = value;
      tail_ = (tail_ + 1) % N;
      ++size_;
    }

    void push(T&& value) {
      if (full()) drop_oldest();
      data_[tail_] = ck::move(value);
      tail_ = (tail_ + 1) % N;
      ++size_;
    }

    // Pop from front — returns false if empty
    bool pop(T& out) {
      if (empty()) return false;
      out = ck::move(data_[head_]);
      head_ = (head_ + 1) % N;
      --size_;
      return true;
    }


    // Peek at front without removing
    T* front() {
      if (empty()) return nullptr;
      return &data_[head_];
    }

    const T* front() const {
      if (empty()) return nullptr;
      return &data_[head_];
    }

    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == N; }
    int size() const { return size_; }
    int capacity() const { return N; }

    void clear() { head_ = tail_ = size_ = 0; }


    bool contains_slow(const T& value) const {
      for (int i = 0; i < size_; ++i) {
        if (data_[(head_ + i) % N] == value) return true;
      }
      return false;
    }

   private:
    void drop_oldest() {
      // Advance head, discarding the oldest element
      head_ = (head_ + 1) % N;
      --size_;
    }

    T data_[N];
    int head_;
    int tail_;
    int size_;
  };
}  // namespace alaska