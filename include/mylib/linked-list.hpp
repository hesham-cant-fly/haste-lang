#ifndef __LINKED_LIST_HPP
#define __LINKED_LIST_HPP

#include <cassert>
template <typename T> struct LinkedList {
  struct Node {
    T *value;
    Node *next = nullptr;
  };

  Node *start = nullptr;
  Node *end = nullptr;

  auto tail() const -> Node * {
    if (start == nullptr) {
      return nullptr;
    }
    return start->next;
  }

  auto append(Node *new_node) -> void {
    if (start == nullptr) {
      assert(end == nullptr);
      start = new_node;
      end = new_node;
    }
    assert(end != nullptr);
    assert(end->next == nullptr);
    end->next = new_node;
    end = new_node;
  }

  auto prepend(Node *new_node) -> void {
    if (start == nullptr) {
      assert(end == nullptr);
      start = new_node;
      end = new_node;
    }
    assert(start != nullptr);
    new_node->next = start;
    start = new_node;
  }
};

#endif // !__LINKED_LIST_HPP
