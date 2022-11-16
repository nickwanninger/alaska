#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "splay-tree.c"

unsigned long num_keys;
unsigned long num_lookups;
long *keys;
long *lookups;

void print(splay_tree_key k, void *state) { printf("%ld\n", k->key); }

#define mallocz(n) memset(malloc(n), 0, n)


double rand_val(int seed) {
  const long a = 16807;       // Multiplier
  const long m = 2147483647;  // Modulus
  const long q = 127773;      // m div a
  const long r = 2836;        // m mod a
  static long x;              // Random int value
  long x_div_q;               // x divided by q
  long x_mod_q;               // x modulo q
  long x_new;                 // New x value

  // Set the seed if argument is non-zero and then return zero
  if (seed > 0) {
    x = seed;
    return (0.0);
  }

  // RNG using integer arithmetic
  x_div_q = x / q;
  x_mod_q = x % q;
  x_new = (a * x_mod_q) - (r * x_div_q);
  if (x_new > 0)
    x = x_new;
  else
    x = x_new + m;

  // Return a random value between 0.0 and 1.0
  return ((double)x / m);
}
int zipf(double alpha, int n) {
  static int first = 1;      // Static first time flag
  static double c = 0;       // Normalization constant
  static double *sum_probs;  // Pre-calculated sum of probabilities
  double z;                  // Uniform random number (0 < z < 1)
  int zipf_value;            // Computed exponential value to be returned
  int i;                     // Loop counter
  int low, high, mid;        // Binary-search bounds

  // Compute normalization constant on first call only
  if (first == 1) {
    for (i = 1; i <= n; i++)
      c = c + (1.0 / pow((double)i, alpha));
    c = 1.0 / c;

    sum_probs = malloc((n + 1) * sizeof(*sum_probs));
    sum_probs[0] = 0;
    for (i = 1; i <= n; i++) {
      sum_probs[i] = sum_probs[i - 1] + c / pow((double)i, alpha);
    }
    first = 1;
  }

  // Pull a uniform random number (0 < z < 1)
  do {
    z = rand_val(0);
  } while ((z == 0) || (z == 1));

  // Map z to the value
  low = 1, high = n;
  do {
    mid = floor((low + high) / 2);
    if (sum_probs[mid] >= z && sum_probs[mid - 1] < z) {
      zipf_value = mid;
      break;
    } else if (sum_probs[mid] >= z) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  } while (low <= high);

  return (zipf_value);
}

int main(int argc, char *argv[]) {
  unsigned long i;
  printf("Hello, world\n");

  for (int i = 0; i < 10; i++) {
    printf("%d\n", zipf(1, 10));
  }
  return 0;

  if (argc < 3) {
    fprintf(stderr, "usage: run-splay numkeys numlookups < stream\n");
    exit(-1);
  }

  num_keys = atol(argv[1]);
  num_lookups = atol(argv[2]);

  keys = malloc(num_keys * sizeof(long));
  lookups = malloc(num_lookups * sizeof(long));

  for (i = 0; i < num_keys; i++) {
    if (scanf("insert %ld\n", &keys[i]) != 1) {
      fprintf(stderr, "failed to read insert %lu\n", i);
      exit(-1);
    }
  }

  for (i = 0; i < num_lookups; i++) {
    if (scanf("lookup %ld\n", &lookups[i]) != 1) {
      fprintf(stderr, "failed to read lookup %lu\n", i);
      exit(-1);
    }
  }

  printf("data loads done\n");

  splay_tree s = mallocz(sizeof(*s));

  for (i = 0; i < num_keys; i++) {
    splay_tree_node n = mallocz(sizeof(*n));

    n->key.key = keys[i];
    splay_tree_insert(s, n);
  }

  printf("%lu keys inserted\n", num_keys);

  struct splay_tree_key_s lookup_key;

  for (i = 0; i < num_keys; i++) {
    lookup_key.key = lookups[i];
    volatile splay_tree_key result = splay_tree_lookup(s, &lookup_key);
  }

  printf("%lu lookups done\n", num_lookups);

  //  splay_tree_foreach(s,print,0);
}