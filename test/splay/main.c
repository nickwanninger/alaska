#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <alaska.h>

// Include the C file so its easy to compile this in one command
#include "splay-tree.c"

long *keys;
long *lookups;

void print(splay_tree_key k, void *state) {
  printf("%ld\n", k->key);
}

#define mallocz(n) memset(malloc(n), 0, n)


// double rand_val(int seed) {
//   return rand() / (double)INT_MAX;
//   const long a = 16807;       // Multiplier
//   const long m = 2147483647;  // Modulus
//   const long q = 127773;      // m div a
//   const long r = 2836;        // m mod a
//   static long x;              // Random int value
//   long x_div_q;               // x divided by q
//   long x_mod_q;               // x modulo q
//   long x_new;                 // New x value
//
//   // Set the seed if argument is non-zero and then return zero
//   if (seed > 0) {
//     x = seed;
//     return (0.0);
//   }
//
//   // RNG using integer arithmetic
//   x_div_q = x / q;
//   x_mod_q = x % q;
//   x_new = (a * x_mod_q) - (r * x_div_q);
//   if (x_new > 0)
//     x = x_new;
//   else
//     x = x_new + m;
//
//   // Return a random value between 0.0 and 1.0
//   return ((double)x / m);
// }
// int zipf(double alpha, int n) {
//   static int first = 1;      // Static first time flag
//   static double c = 0;       // Normalization constant
//   static double *sum_probs;  // Pre-calculated sum of probabilities
//   double z;                  // Uniform random number (0 < z < 1)
//   int zipf_value;            // Computed exponential value to be returned
//   int i;                     // Loop counter
//   int low, high, mid;        // Binary-search bounds
//   zipf_value = 0;
//
//   // Compute normalization constant on first call only
//   if (first == 1) {
//     for (i = 1; i <= n; i++)
//       c = c + (1.0 / pow((double)i, alpha));
//     c = 1.0 / c;
//
//     sum_probs = malloc((n + 1) * sizeof(*sum_probs));
//     sum_probs[0] = 0;
//     for (i = 1; i <= n; i++) {
//       sum_probs[i] = sum_probs[i - 1] + c / pow((double)i, alpha);
//     }
//     first = 1;
//   }
//
//   // Pull a uniform random number (0 < z < 1)
//   do {
//     z = rand_val(0);
//     printf("z=%f\n", z);
//   } while ((z == 0) || (z == 1));
//
//   // Map z to the value
//   low = 1, high = n;
//   do {
//     mid = floor((low + high) / 2.0);
//     if (sum_probs[mid] >= z && sum_probs[mid - 1] < z) {
//       zipf_value = mid;
//       break;
//     } else if (sum_probs[mid] >= z) {
//       high = mid - 1;
//     } else {
//       low = mid + 1;
//     }
//   } while (low <= high);
//
//   return (zipf_value);
// }

int main(int argc, char *argv[]) {
  // Allocate a splay tree
  splay_tree s = mallocz(sizeof(*s));
  char buf[512];
  // read each line from stdin
  while (!feof(stdin)) {
    long value = 0;
    buf[0] = '\0';  // be "safe"
    fgets(buf, 512, stdin);

    if (sscanf(buf, "insert %ld\n", &value) == 1) {
      splay_tree_node n = mallocz(sizeof(*n));
      n->key.key = value;

      printf("ins %ld\n", value);
      splay_tree_insert(s, n);
      continue;
    }


    if (sscanf(buf, "lookup %ld\n", &value) == 1) {
      struct splay_tree_key_s lookup_key;
      lookup_key.key = value;
      volatile splay_tree_key result = splay_tree_lookup(s, &lookup_key);
      (void)result;
      // printf("get %ld\n", value);
      continue;
    }

    fprintf(stderr, "unknown input: %s\n", buf);


#ifdef ALASKA_SERVICE_ANCHORAGE
    anchorage_manufacture_locality((void *)s);
#endif

    // alaska_barrier();
    // coninue and hope it is fine
  }
}
