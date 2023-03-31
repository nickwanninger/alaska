#include <stdio.h>
#include <stdlib.h>
#include <alaska.h>
#include <string.h>

struct node {
  struct node* next;
  volatile int val;
};


int main(int argc, char **argv) {

	volatile char buf[512];
	strcpy((char*)buf, "Hello, world");

	puts((char*)buf);

 //  int count = 16;
 //  if (argc == 2) count = atoi(argv[1]);
	//
 //  struct node* root = NULL;
 //  for (int i = 0; i < count; i++) {
 //    struct node* n = (struct node*)malloc(sizeof(*n));
 //    n->next = root;
 //    n->val = i;
 //    root = n;
 //  }
	//
	// printf("v = %d\n", root->val);
 //  anchorage_manufacture_locality((void*)root);
	// printf("v = %d\n", root->val);

	return 0;
}
