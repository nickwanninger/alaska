struct node {
  struct node *next, *prev;
  int val;
};


struct node *get_next(struct node *n) {
  return n->next;
}


int main() {
  return 0;
}
