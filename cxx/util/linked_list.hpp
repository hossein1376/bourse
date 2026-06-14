#pragma once

#include <cstddef>
#include <iterator>

namespace engine {

template <typename T> class DoublyLinkedList {
public:
  struct Node {
    T data;
    Node *prev = nullptr;
    Node *next = nullptr;
  };

  class iterator {
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    iterator() : node_(nullptr) {}
    explicit iterator(Node *node) : node_(node) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &node_->data; }

    iterator &operator++() {
      node_ = node_->next;
      return *this;
    }
    iterator &operator--() {
      node_ = node_->prev;
      return *this;
    }
    iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }
    iterator operator--(int) {
      auto tmp = *this;
      --(*this);
      return tmp;
    }

    bool operator==(const iterator &other) const {
      return node_ == other.node_;
    }
    bool operator!=(const iterator &other) const {
      return node_ != other.node_;
    }

  private:
    Node *node_;
  };

  class const_iterator {
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T *;
    using reference = const T &;

    const_iterator() : node_(nullptr) {}
    explicit const_iterator(const Node *node) : node_(node) {}
    const_iterator(const iterator &it) : node_(it.node_) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &node_->data; }

    const_iterator &operator++() {
      node_ = node_->next;
      return *this;
    }
    const_iterator &operator--() {
      node_ = node_->prev;
      return *this;
    }

    bool operator==(const const_iterator &other) const {
      return node_ == other.node_;
    }
    bool operator!=(const const_iterator &other) const {
      return node_ != other.node_;
    }

  private:
    const Node *node_;
  };

  DoublyLinkedList() : head_(nullptr), tail_(nullptr) {}
  ~DoublyLinkedList() { clear(); }

  DoublyLinkedList(const DoublyLinkedList &) = delete;
  DoublyLinkedList &operator=(const DoublyLinkedList &) = delete;

  Node *push_front(const T &data) {
    Node *new_node = new Node{
        .data = data,
        .prev = nullptr,
        .next = nullptr,
    };
    if (head_ == nullptr)
      head_ = tail_ = new_node;
    else {
      new_node->next = head_;
      head_->prev = new_node;
      head_ = new_node;
    }
    return new_node;
  }
  Node *push_back(const T &data) {
    Node *new_node = new Node{
        .data = data,
        .prev = nullptr,
        .next = nullptr,
    };
    if (tail_ == nullptr)
      head_ = tail_ = new_node;
    else {
      new_node->prev = tail_;
      tail_->next = new_node;
      tail_ = new_node;
    }
    return new_node;
  }
  Node *insert_before(Node *pos, const T &data) {
    if (pos == nullptr)
      return push_back(data);
    if (pos == head_)
      return push_front(data);
    else {
      Node *new_node = new Node{
          .data = data,
          .prev = pos->prev,
          .next = pos,
      };
      pos->prev->next = new_node;
      pos->prev = new_node;
      return new_node;
    }
  }
  Node *insert_after(Node *pos, const T &data) {
    if (pos == nullptr || pos == tail_)
      return push_back(data);
    else {
      Node *new_node = new Node{
          .data = data,
          .prev = pos,
          .next = pos->next,
      };
      pos->next->prev = new_node;
      pos->next = new_node;
      return new_node;
    }
  }

  void erase(Node *node) {
    if (node == nullptr)
      return;

    if (node == head_) {
      head_ = node->next;
      if (head_ != nullptr)
        head_->prev = nullptr;
      else
        tail_ = nullptr;
      delete node;
      return;
    }
    if (node == tail_) {
      tail_ = node->prev;
      if (tail_ != nullptr)
        tail_->next = nullptr;
      else
        head_ = nullptr;
      delete node;
      return;
    }
    node->prev->next = node->next;
    node->next->prev = node->prev;
    delete node;
  }
  void pop_front() { erase(head_); }
  void clear() {
    while (head_ != nullptr) {
      Node *temp = head_;
      head_ = head_->next;
      delete temp;
    }
    tail_ = nullptr;
  }

  Node *head() { return head_; }
  const Node *head() const { return head_; }
  Node *tail() { return tail_; }
  const Node *tail() const { return tail_; }
  bool empty() const { return head_ == nullptr; }

  iterator begin() { return iterator(head_); }
  iterator end() { return iterator(nullptr); }
  const_iterator begin() const { return const_iterator(head_); }
  const_iterator end() const { return const_iterator(nullptr); }

private:
  Node *head_ = nullptr;
  Node *tail_ = nullptr;
};

} // namespace engine
