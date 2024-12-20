#pragma once

#include "./template_lib.h"
#include <alaska/alaska.hpp>

namespace ck {

  template <typename ListType, typename ElementType>
  class single_listIterator {
   public:
    bool operator!=(const single_listIterator& other) const { return m_node != other.m_node; }
    single_listIterator& operator++() {
      m_prev = m_node;
      m_node = m_node->next;
      return *this;
    }
    ElementType& operator*() { return m_node->value; }
    ElementType* operator->() { return &m_node->value; }
    bool is_end() const { return !m_node; }
    static single_listIterator universal_end() { return single_listIterator(nullptr); }

   private:
    friend ListType;
    explicit single_listIterator(
        typename ListType::Node* node, typename ListType::Node* prev = nullptr)
        : m_node(node)
        , m_prev(prev) {}
    typename ListType::Node* m_node{nullptr};
    typename ListType::Node* m_prev{nullptr};
  };

  template <typename T>
  class single_list {
   private:
    struct Node : public alaska::InternalHeapAllocated {
      explicit Node(T&& v)
          : value(move(v)) {}
      explicit Node(const T& v)
          : value(v) {}
      T value;
      Node* next{nullptr};
    };

   public:
    single_list() {}
    ~single_list() { clear(); }

    bool is_empty() const { return !head(); }

    inline int size_slow() const {
      int size = 0;
      for (auto* node = m_head; node; node = node->next)
        ++size;
      return size;
    }

    void clear() {
      for (auto* node = m_head; node;) {
        auto* next = node->next;
        delete node;
        node = next;
      }
      m_head = nullptr;
      m_tail = nullptr;
    }

    T& first() {
      ALASKA_ASSERT(head(), "");
      return head()->value;
    }
    const T& first() const {
      ALASKA_ASSERT(head(), "");
      return head()->value;
    }
    T& last() {
      ALASKA_ASSERT(head(), "");
      return tail()->value;
    }
    const T& last() const {
      ALASKA_ASSERT(head(), "");
      return tail()->value;
    }

    T take_first() {
      ALASKA_ASSERT(m_head, "");
      auto* prev_head = m_head;
      T value = move(first());
      if (m_tail == m_head) m_tail = nullptr;
      m_head = m_head->next;
      delete prev_head;
      return value;
    }

    void append(const T& value) {
      auto* node = new Node(value);
      if (!m_head) {
        m_head = node;
        m_tail = node;
        return;
      }
      m_tail->next = node;
      m_tail = node;
    }

    void append(T&& value) {
      auto* node = new Node(move(value));
      if (!m_head) {
        m_head = node;
        m_tail = node;
        return;
      }
      m_tail->next = node;
      m_tail = node;
    }

    bool contains_slow(const T& value) const {
      for (auto* node = m_head; node; node = node->next) {
        if (node->value == value) return true;
      }
      return false;
    }

    using Iterator = single_listIterator<single_list, T>;
    friend Iterator;
    Iterator begin() { return Iterator(m_head); }
    Iterator end() { return Iterator::universal_end(); }

    using ConstIterator = single_listIterator<const single_list, const T>;
    friend ConstIterator;
    ConstIterator begin() const { return ConstIterator(m_head); }
    ConstIterator end() const { return ConstIterator::universal_end(); }

    template <typename Finder>
    ConstIterator find(Finder finder) const {
      Node* prev = nullptr;
      for (auto* node = m_head; node; node = node->next) {
        if (finder(node->value)) return ConstIterator(node, prev);
        prev = node;
      }
      return end();
    }

    template <typename Finder>
    Iterator find(Finder finder) {
      Node* prev = nullptr;
      for (auto* node = m_head; node; node = node->next) {
        if (finder(node->value)) return Iterator(node, prev);
        prev = node;
      }
      return end();
    }

    ConstIterator find(const T& value) const {
      return find([&](auto& other) {
        return value == other;
      });
    }

    Iterator find(const T& value) {
      return find([&](auto& other) {
        return value == other;
      });
    }

    void remove(Iterator iterator) {
      ALASKA_ASSERT(!iterator.is_end(), "");
      if (m_head == iterator.m_node) m_head = iterator.m_node->next;
      if (m_tail == iterator.m_node) m_tail = iterator.m_prev;
      if (iterator.m_prev) iterator.m_prev->next = iterator.m_node->next;
      delete iterator.m_node;
    }

   private:
    Node* head() { return m_head; }
    const Node* head() const { return m_head; }

    Node* tail() { return m_tail; }
    const Node* tail() const { return m_tail; }

    Node* m_head{nullptr};
    Node* m_tail{nullptr};
  };

}  // namespace ck
