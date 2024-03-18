#include <stdlib.h>


struct Patient;


typedef struct List {
  struct List *forward;
  struct Patient *patient;
  struct List *back;
} List;

__attribute__((noinline))
List *addList(List *list, struct Patient *pt) {
  struct List *b = NULL;
  while (list != NULL) {
    b = list;
    list = list->forward;
  }
  list = (List *)malloc(sizeof(List));
  list->patient = pt;
  list->forward = NULL;
  list->back = b;
  b->forward = list;
  return list;
}

int main() {

  List *l = (List*)malloc(sizeof(List));
  l->forward = NULL;
  l->patient = NULL;
  l->back = NULL;
  addList(l, NULL);
  return 0;
}
