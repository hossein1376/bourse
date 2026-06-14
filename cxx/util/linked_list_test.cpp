#include "util/linked_list.hpp"
#include <gtest/gtest.h>

using namespace engine;

TEST(LinkedListTest, EmptyOnConstruction) {
  DoublyLinkedList<int> list;
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.head(), nullptr);
  EXPECT_EQ(list.tail(), nullptr);
}

TEST(LinkedListTest, PushFront) {
  DoublyLinkedList<int> list;
  list.push_front(1);
  list.push_front(2);

  EXPECT_EQ(list.head()->data, 2);
  EXPECT_EQ(list.tail()->data, 1);
  EXPECT_FALSE(list.empty());
}

TEST(LinkedListTest, PushBack) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(2);

  EXPECT_EQ(list.head()->data, 1);
  EXPECT_EQ(list.tail()->data, 2);
}

TEST(LinkedListTest, InsertBeforeMiddle) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(3);
  list.insert_before(list.tail(), 2);

  EXPECT_EQ(list.head()->data, 1);
  EXPECT_EQ(list.head()->next->data, 2);
  EXPECT_EQ(list.tail()->prev->data, 2);
  EXPECT_EQ(list.tail()->data, 3);
}

TEST(LinkedListTest, InsertBeforeNull) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.insert_before(nullptr, 2);

  EXPECT_EQ(list.head()->data, 1);
  EXPECT_EQ(list.tail()->data, 2);
}

TEST(LinkedListTest, InsertAfterHead) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(3);
  list.insert_after(list.head(), 2);

  EXPECT_EQ(list.head()->data, 1);
  EXPECT_EQ(list.head()->next->data, 2);
  EXPECT_EQ(list.tail()->prev->data, 2);
  EXPECT_EQ(list.tail()->data, 3);
}

TEST(LinkedListTest, InsertAfterTail) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.insert_after(list.tail(), 2);

  EXPECT_EQ(list.head()->data, 1);
  EXPECT_EQ(list.tail()->data, 2);
}

TEST(LinkedListTest, EraseHead) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(2);
  list.erase(list.head());

  EXPECT_EQ(list.head()->data, 2);
}

TEST(LinkedListTest, EraseMiddle) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(2);
  list.push_back(3);
  list.erase(list.head()->next);

  EXPECT_EQ(list.head()->next, list.tail());
  EXPECT_EQ(list.tail()->prev, list.head());
}

TEST(LinkedListTest, EraseTail) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(2);
  list.erase(list.tail());

  EXPECT_EQ(list.tail()->data, 1);
  EXPECT_EQ(list.tail(), list.head());
}

TEST(LinkedListTest, Clear) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(2);
  list.push_back(3);
  list.clear();

  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.head(), nullptr);
  EXPECT_EQ(list.tail(), nullptr);
}

TEST(LinkedListTest, IteratorForward) {
  DoublyLinkedList<int> list;
  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  int expected = 1;
  for (const auto &val : list) {
    EXPECT_EQ(val, expected);
    ++expected;
  }
  EXPECT_EQ(expected, 4);
}
