#include <stdlib.h>
#include <stdio.h>

typedef struct node {
  int data;
  struct node *n;
} * List;


int get_data(List x) {
  return x->data;
}

List reverse(List x) {
  List y = NULL;
  List t;
  while (x != NULL) {
    x->data++;
    t = y;
    y = x;
    x = x->n;
    y->n = NULL;
    y->n = t;
  }
  return y;
}
int main() {
  List x = malloc(sizeof(*x));
  x->data = 0;
  x->n = 0;
  get_data(x);
}



// struct node1 {
//   int data;
//   struct node1 *n;
// };


struct node2 {
  struct node2 *n;
  int data;
};



// int test(struct node1 *a) {
//   int sum = 0;
//
//   while (a != NULL) {
//     sum += a->data;
//     a = a->n;
//   }
//   return sum;
// }
// int main() { return 0; }



// typedef struct list {
//   struct list *Next;
//   int Data;
// } list;
//
//
// int Global = 10;
// void do_all(list *L, void (*FP)(int *)) {
//   do {
//     FP(&L->Data);
//     L = L->Next;
//   } while (L);
// }
//
// void addG(int *X) { (*X) += Global; }
// void addGToList(list *L) { do_all(L, addG); }
// list *makeList(int Num) {
//   list *New = malloc(sizeof(list));
//   New->Next = Num ? makeList(Num - 1) : NULL;
//   New->Data = Num;
//   return New;
// }
//
// int main() {
//   list *X = makeList(10);
//   list *Y = makeList(100);
//   addGToList(X);
//   Global = 20;
//   addGToList(Y);
//   return 0;
// }



// struct thing {
//   int val1;
//   double val2;
// };
//
// __attribute__((noinline)) struct thing *new_thing(int i, double d) {
//   struct thing *t = malloc(sizeof(*t));
//   t->val1 = i;
//   t->val2 = d;
//   return t;
// }
//
//
// __attribute__((noinline)) int use(struct thing *thing) {
//   int x = thing->val1 + thing->val2;
//   printf("%p\n", thing);
//   return x;
// }
//
// int main() {
//   struct thing *t = new_thing(0, 1.4);
//   printf("%p\n", t);
//   // struct thing *t = malloc(sizeof(*t));
//   use(t);
//   return 0;
// }
