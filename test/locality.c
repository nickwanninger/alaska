#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alaska.h>

struct node {
  struct node *next;
  int val;
	// char *msg;
};

void print_list(struct node *root) {
  struct node *cur = root;
  while (cur != NULL) {
		printf("0x%x ", cur->val);
    cur = cur->next;
  }
	printf("\n");
}

// int sum_list(struct node *root) {
//   int sum = 0;
//   struct node *cur = root;
//   while (cur != NULL) {
//     sum += cur->val;
//     cur = cur->next;
//   }
//   return sum;
// }

int main(int argc, char **argv) {
  struct node *root = NULL;

  int len = 10;

  if (argc == 2) {
    len = atoi(argv[1]);
  }

  for (int i = 0; i < len; i++) {
    struct node *n = (struct node *)malloc(sizeof(struct node));
    n->next = root;
    n->val = i;
    root = n;
  }

	print_list(root);

#ifdef ALASKA_SERVICE_ANCHORAGE
  anchorage_manufacture_locality((void *)root);
#endif

  // printf("sum = %d\n", sum_list(root));
  // alaska_barrier();
  // printf("sum = %d\n", sum_list(root));
  print_list(root);


  struct node *cur = root;
  while (cur != NULL) {
    struct node *prev = cur;
    cur = cur->next;
    free(prev);
  }

  return 0;
}
