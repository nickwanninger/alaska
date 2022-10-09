#include "../src/rt/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include "./miniz.c"

struct node {
  struct node *next;
  int val;
};

// alaska_rt void hexdump(const void *data, size_t size) {
//   char ascii[17];
//   size_t i, j;
//   ascii[16] = '\0';
//   for (i = 0; i < size; ++i) {
//     printf("%02X ", ((unsigned char *)data)[i]);
//     if (((unsigned char *)data)[i] >= ' ' && ((unsigned char *)data)[i] <= '~') {
//       ascii[i % 16] = ((unsigned char *)data)[i];
//     } else {
//       ascii[i % 16] = '.';
//     }
//     if ((i + 1) % 8 == 0 || i + 1 == size) {
//       printf(" ");
//       if ((i + 1) % 16 == 0) {
//         printf("|  %s \n", ascii);
//       } else if (i + 1 == size) {
//         ascii[(i + 1) % 16] = '\0';
//         if ((i + 1) % 16 <= 8) {
//           printf(" ");
//         }
//         for (j = (i + 1) % 16; j < 16; ++j) {
//           printf("   ");
//         }
//         printf("|  %s \n", ascii);
//       }
//     }
//   }
// }
//
// alaska_arena_t fs_arena;
//
//
// alaska_rt FILE *get_handle_file(alaska_handle_t *handle) {
//   FILE *f;
//
//   char path[64];
//   snprintf(path, 64, "/home/nick/data/%p", (void *)handle->handle);
//   f = fopen(path, "w");
//
//   return f;
// }
//
// alaska_rt int fs_pinned(alaska_arena_t *arena, alaska_handle_t *handle) {
// 	return 0;
// }
//
//
// alaska_rt int fs_unpinned(alaska_arena_t *arena, alaska_handle_t *handle) {
//   // FILE *f = get_handle_file(handle);
//   // fwrite(handle->backing_memory, 1, handle->size, f);
//   // fclose(f);
//   return 0;
// }
//
//
// alaska_rt void fs_free(alaska_arena_t *arena, void *ptr) {
//   if (ptr) {
//     free(ptr);
//   }
// }

int main() {
  // alaska_arena_init(&fs_arena);
  // fs_arena.pinned = fs_pinned;
  // fs_arena.unpinned = fs_unpinned;
  // fs_arena.free = fs_free;
  // system("rm -rf ~/data/*");
  // char cmd[128];
  // snprintf(cmd, 128, "mkdir -p ~/data/");
  // system(cmd);


struct node *root = NULL;
  root = (struct node *)alaska_alloc(sizeof(struct node));

  root->next = NULL;
  root->val = rand();

  alaska_free(root);


  return 0;

  // int count = 10;
  // for (int i = 0; i < count; i++) {
  //   struct node *n = (struct node *)alaska_alloc(sizeof(struct node)); // , &fs_arena);
  //   n->val = i;
  //   n->next = root;
  //   root = n;
  // 	printf("root: %p\n", root);
  // }

  // int sum = 0;
  // struct node *cur = root;
  // while (cur != NULL) {
  //   sum += cur->val;
  //   struct node *prev = cur;
  //   cur = cur->next;
  // 	alaska_free(prev);
  // }
  // printf("sum=%d\n", sum);
  // return 0;
}
