#include "client.h"
#include "core_workload.h"
#include "db.h"
#include "timer.h"
#include "utils.h"

// sds.h uses 'template' as a parameter name and void* implicit casts — work around both.
#define template sds_template_param
extern "C" {
#include "dict.h"
#include "sds.h"
}
#undef template

#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <getopt.h>
#include <string>
#include <sys/resource.h>
#include <vector>

size_t get_rss_bytes(void) {
  long rss_pages = 0;
  FILE *f = fopen("/proc/self/statm", "r");
  if (f) {
    if (fscanf(f, "%*s%ld", &rss_pages) != 1)
      rss_pages = 0;
    fclose(f);
  }
  return (size_t)rss_pages * (size_t)0x1000;
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

static void change_roi(int enabled) {
  typedef void (*yukon_switch_roi_t)(int enabled);
  static yukon_switch_roi_t yukon_change_roi =
      (yukon_switch_roi_t)dlsym(RTLD_DEFAULT, "yukon_change_roi");
  if (yukon_change_roi != NULL) {
    yukon_change_roi(enabled);
  }
}

static inline uint64_t read_instret() {
  uint64_t instret;
#ifdef __riscv
  asm volatile("rdinstret %0" : "=r"(instret));
#else
  instret = 0;
#endif
  return instret;
}

namespace ycsbc {

// Each field/value pair is stored as a malloc'd node in a linked list.
struct FieldNode {
  char *name;
  char *value;
  FieldNode *next;
};

static FieldNode *make_field_node(const std::string &name, const std::string &value) {
  FieldNode *node = (FieldNode *)malloc(sizeof(FieldNode));
  node->name  = (char *)malloc(name.size() + 1);
  node->value = (char *)malloc(value.size() + 1);
  memcpy(node->name,  name.data(),  name.size() + 1);
  memcpy(node->value, value.data(), value.size() + 1);
  node->next = nullptr;
  return node;
}

static void free_field_list(FieldNode *head) {
  while (head) {
    FieldNode *next = head->next;
    free(head->name);
    free(head->value);
    free(head);
    head = next;
  }
}

static uint64_t sds_hash(const void *key) {
  return dictGenHashFunction((const unsigned char *)key, sdslen((const sds)key));
}

static int sds_compare(dict *, const void *a, const void *b) {
  return sdscmp((sds)a, (sds)b) == 0;
}

static dictType ycsbDictType = {
  sds_hash,
  NULL,
  NULL,
  sds_compare,
  [](dict*, void *k){ sdsfree((sds)k); },
  [](dict*, void *v){ free_field_list((FieldNode*)v); },
  NULL
};

class InMemoryDB : public DB {
public:
  InMemoryDB()  { table_ = dictCreate(&ycsbDictType); }
  ~InMemoryDB() { dictRelease(table_); }

  void Init() {}

  int Read(const std::string &table, const std::string &key,
           const std::vector<std::string> *fields,
           std::vector<KVPair> &result) {
    sds k = sdsnewlen(key.data(), key.size());
    dictEntry *de = dictFind(table_, k);
    sdsfree(k);
    if (!de)
      return kErrorNoData;

    for (FieldNode *n = (FieldNode *)dictGetVal(de); n != nullptr; n = n->next) {
      if (!fields) {
        result.emplace_back(n->name, n->value);
      } else {
        for (const auto &f : *fields) {
          if (f == n->name) {
            result.emplace_back(n->name, n->value);
            break;
          }
        }
      }
    }
    return kOK;
  }

  int Scan(const std::string &table, const std::string &key, int len,
           const std::vector<std::string> *fields,
           std::vector<std::vector<KVPair>> &result) {
    throw "Scan: function not implemented!";
    return 0;
  }

  int Update(const std::string &table, const std::string &key,
             std::vector<KVPair> &values) {
    sds k = sdsnewlen(key.data(), key.size());
    dictEntry *de = dictFind(table_, k);

    FieldNode *head = de ? (FieldNode *)dictGetVal(de) : nullptr;

    for (auto &kv : values) {
      const std::string &fname = kv.first;
      const std::string &fval  = kv.second.empty() ? std::string("X") : kv.second;

      for (FieldNode *n = head; n != nullptr; n = n->next) {
        if (strcmp(n->name, fname.c_str()) == 0) {
          free(n->value);
          n->value = (char *)malloc(fval.size() + 1);
          memcpy(n->value, fval.data(), fval.size() + 1);
          goto next_pair;
        }
      }

      {
        FieldNode *node = make_field_node(fname, fval);
        node->next = head;
        head = node;
      }

    next_pair:;
    }

    if (de) {
      dictSetVal(table_, de, head);
      sdsfree(k);
    } else {
      dictAdd(table_, k, head);  // dict takes ownership of k
    }
    return kOK;
  }

  int Insert(const std::string &table, const std::string &key,
             std::vector<KVPair> &values) {
    return Update(table, key, values);
  }

  int Delete(const std::string &table, const std::string &key) {
    sds k = sdsnewlen(key.data(), key.size());
    int ret = dictDelete(table_, k);
    sdsfree(k);
    return ret == DICT_OK ? kOK : kErrorNoData;
  }

private:
  dict *table_;
};

} // namespace ycsbc

typedef void (*yukon_enable_localization_t)(bool enable);

int DelegateClient(ycsbc::DB *db, ycsbc::CoreWorkload *wl, const int num_ops,
                   bool is_loading) {
  yukon_enable_localization_t yukon_enable_localization =
      (yukon_enable_localization_t)dlsym(RTLD_DEFAULT, "yukon_enable_localization");

  db->Init();
  ycsbc::Client client(*db, *wl);
  int oks = 0;
  int bads = 0;

  if (yukon_enable_localization != NULL) {
    yukon_enable_localization(!is_loading);
  }

  if (is_loading) {
    printf("Loading %d keys...\n", num_ops);
  } else {
    printf("Performing %d ops...\n", num_ops);
    change_roi(1);
  }

  int reporting_interval = num_ops / 100;
  int next_report = reporting_interval;
  auto start_inst = read_instret();
  auto report_start_inst = start_inst;
  auto report_start_cycles = read_cycle_counter();

  for (int i = 0; i < num_ops; ++i) {
    next_report--;
    if (next_report == 0) {
      next_report = reporting_interval;
      float rss_mb = get_rss_bytes() / 1024.0f / 1024.0f;

      auto report_end_inst   = read_instret();
      auto report_end_cycles = read_cycle_counter();
      size_t insts   = report_end_inst   - report_start_inst;
      size_t cycles  = report_end_cycles - report_start_cycles;

      report_start_inst   = report_end_inst;
      report_start_cycles = report_end_cycles;

      printf("   ");
      printf("progress=%6.1f%%, ", (i + 1) * 100.0 / num_ops);
      printf("rss=%fmb, ", rss_mb);
      printf("C/OP=%10lf, ", cycles / (float)reporting_interval);
      // printf("CPI=%5.2f, ", cycles / (insts + 1e-9f));
      printf("TPUT=%8zu, ", (size_t)(reporting_interval / (cycles / 1e9f)));
      printf("bads=%u, ", bads);
      printf("\n");
      fflush(stdout);
    }

    if (is_loading) {
      if (client.DoInsert()) oks++;
      else                   bads++;
    } else {
      if (client.DoTransaction()) oks++;
      else                        bads++;
    }
  }

  if (yukon_enable_localization != NULL) {
    yukon_enable_localization(false);
  }

  db->Close();

  if (!is_loading) {
    change_roi(0);
  }
  return oks;
}

int main(int argc, char **argv) {
  float scaleInsert = 1.0f, scaleWorkload = 1.0f;

  static struct option long_opts[] = {
    {"repeat", required_argument, nullptr, 'n'},
    {"records", required_argument, nullptr, 'r'},
    {nullptr, 0, nullptr, 0},
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "n:r:", long_opts, nullptr)) != -1) {
    if (opt == 'n')
      scaleWorkload = atof(optarg);
    else if (opt == 'r')
      scaleInsert = atof(optarg);
    else {
      fprintf(stderr, "Usage: %s [-n <workload-multiplier>] [-r <record-multiplier>] <workload>\n", argv[0]);
      return 1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Usage: %s [-n <workload-multiplier>] [-r <record-multiplier>] <workload>\n", argv[0]);
    return 1;
  }

  utils::Properties props;
  try {
    std::ifstream file(argv[optind]);
    props.Load(file);
  } catch (const std::exception &e) {
    fprintf(stderr, "Failed to load workload: %s\n", e.what());
    return 1;
  }

  ycsbc::InMemoryDB db;
  ycsbc::CoreWorkload wl;
  wl.Init(props);

  utils::Timer<double> timer;

  timer.Start();
  int insert_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]) * scaleInsert;
  DelegateClient(&db, &wl, insert_ops, true);
  double insert_duration = timer.End();

  timer.Start();
  auto roi_start = read_cycle_counter();
  int workload_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]) * scaleWorkload;
  DelegateClient(&db, &wl, workload_ops, false);
  auto roi_end = read_cycle_counter();
  double workload_duration = timer.End();

  printf("YUKON_YCSB_ROI_CYCLES=%zu\n", roi_end - roi_start);
  printf("YUKON_YCSB_WORKLOAD=%s\n", argv[optind]);
  printf("YUKON_YCSB_INSERT_OPS=%d\n", insert_ops);
  printf("YUKON_YCSB_WORKLOAD_OPS=%d\n", workload_ops);
  printf("YUKON_YCSB_INSERT_DURATION=%f\n", insert_duration);
  printf("YUKON_YCSB_LOOKUP_DURATION=%f\n", workload_duration);
  printf("YUKON_YCSB_TPUT=%f\n", workload_ops / workload_duration);
  return 0;
}
