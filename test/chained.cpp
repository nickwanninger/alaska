#include <iostream>
#include <vector>
#include <dlfcn.h>
#include <stdint.h>

static void localize(void* ptr) {
  typedef void (*yukon_localize_t)(void* ptr);
  static yukon_localize_t yukon_localize =
      (yukon_localize_t)dlsym(RTLD_DEFAULT, "localize_structure");
  if (yukon_localize != NULL) {
    yukon_localize(ptr);
  }
}


static inline uint64_t read_cycle_counter() {
#if defined(__x86_64__) || defined(__i386__)
  uint32_t lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
#elif defined(__riscv)
  uint64_t cycles;
  __asm__ volatile("rdcycle %0" : "=r"(cycles));
  return cycles;
#else
#error "Unsupported architecture"
#endif
}

static inline uint64_t read_instret() {
#if defined(__x86_64__) || defined(__i386__)
  uint32_t lo, hi;
  __asm__ volatile("rdpmc" : "=a"(lo), "=d"(hi) : "c"(1));
  return ((uint64_t)hi << 32) | lo;
#elif defined(__riscv)
  uint64_t instret;
  __asm__ volatile("rdinstret %0" : "=r"(instret));
  return instret;
#else
#error "Unsupported architecture"
#endif
}

class ChainedHashTable {
 private:
  struct Node {
    int key;
    int value;
    Node* next;
    Node(int k, int v, Node* n = nullptr)
        : key(k)
        , value(v)
        , next(n) {}
  };

  std::vector<Node*> table;
  size_t bucket_count;

 public:
  ChainedHashTable(size_t buckets)
      : bucket_count(buckets)
      , table(buckets, nullptr) {}

  ~ChainedHashTable() {
    for (size_t i = 0; i < bucket_count; ++i) {
      Node* cur = table[i];
      while (cur) {
        Node* tmp = cur;
        cur = cur->next;
        delete tmp;
      }
    }
  }

  void insert(int hashed_key, int value) {
    size_t index = hashed_key % bucket_count;
    for (Node* cur = table[index]; cur; cur = cur->next) {
      if (cur->key == hashed_key) {
        cur->value = value;
        return;
      }
    }
    table[index] = new Node(hashed_key, value, table[index]);
  }

  bool lookup(int hashed_key, int& out_value) {
    size_t index = hashed_key % bucket_count;
    for (Node* cur = table[index]; cur; cur = cur->next) {
      if (cur->key == hashed_key) {
        out_value = cur->value;
        return true;
      }
    }
    return false;
  }

  void print_table() {
    for (size_t i = 0; i < bucket_count; ++i) {
      std::cout << "Bucket " << i << ": ";
      for (Node* cur = table[i]; cur; cur = cur->next) {
        std::cout << "(" << cur->key << " -> " << cur->value << ") ";
      }
      std::cout << "\n";
    }
  }

  void localize(void) {
    for (size_t i = 0; i < bucket_count; ++i) {
      if (table[i] != nullptr) ::localize(table[i]);
    }
  }
};

int main() {
  int buckets = 512;
  ChainedHashTable hash_table(buckets);

  printf("inserting... ");
  fflush(stdout);
  for (int i = 0; i < 200000; i++) {
    hash_table.insert(i, i);
  }
  printf("DONE\n");

  typedef void (*runtime_fn_t)(void);

  runtime_fn_t trigger_sweep = (runtime_fn_t)dlsym(RTLD_DEFAULT, "alaska_sweep");

  if (trigger_sweep) {
    printf("Triggering sweep...\n");
    auto start_cycles = read_cycle_counter();
    auto start_instr = read_instret();
    trigger_sweep();
    auto end_instr = read_instret();
    auto end_cycles = read_cycle_counter();
    uint64_t dc = end_cycles - start_cycles;
    uint64_t di = end_instr - start_instr;
    printf("Sweep took %zu cycles, %zu instrs, IPC=%.2f\n",
           dc, di, (double)di / (double)dc);
  }

  // hash_table.localize();


  // change_roi(1);
  for (int trial = 0; trial < 10; trial++) {
    auto start = read_cycle_counter();
    for (int i = 0; i < 200000; i++) {
      int out;
      hash_table.lookup(i, out);
    }
    auto end = read_cycle_counter();
    printf("%d, %zu\n", buckets, end - start);
  }
  // change_roi(0);
  // hash_table.print_table();
  return 0;
}
