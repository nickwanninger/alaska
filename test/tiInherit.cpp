#include <stdlib.h>
#include <vector>

struct Node {};

struct Inner : Node {
  Inner(Node *left, Node *right)
      : left(left)
      , right(right) {}
  Node *left, *right;
};


struct Leaf : Node {
  Leaf(int data)
      : data(data), other(rand()) {}
  int data;
  int other;
};


extern "C" Node *make_tree(void) {
  return new Inner(new Leaf(4), new Inner(new Leaf(2), new Leaf(3)));
}

int main() { return 0; }
