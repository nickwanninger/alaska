#include <stdio.h>
#include <stdlib.h>
#include <set>
#include <string>
#include <map>
#include <vector>

#if defined(__aarch64__)
static const char *arch = "aarch64";
#elif defined(__x86_64__)
static const char *arch = "x86_64";
#endif


static const char *local = ALASKA_INSTALL_PREFIX;

typedef void (*flag_handler_t)(void);


void prefix(void) {
  printf("%s\n", local);
}
void linkdir(void) {
  printf("%s/lib\n", local);
}
void includedir(void) {
  printf("%s/include\n", local);
}


void ldflags(void) {
  printf("-T\n%s/share/alaska/ldscripts/alaska-%s.ld\n", local, arch);
  printf("-L%s/lib\n", local);
  // printf("%s/lib/libalaska_core.a\n", local);
  printf("-Wl,-rpath=%s/lib\n", local);
  printf("-Wl,-rpath-link=%s/lib\n", local);
  printf("-lm\n");
  // printf("%s/lib/libalaska.a\n", local);
  printf("-lalaska\n"); // HACK: load alaska before pthread
  printf("-lpthread\n"); // ... for some reason
  // printf("-lomp\n");
}


void includeflags(void) {
  printf("-I%s/include\n", local);
}

void cflags(void) {
  printf("-march=native\n");
  includeflags();
}

void cxxflags(void) {
  cflags();
  printf("-fno-exceptions\n");
}

void defs(void) {
  // enable a bunch of feature flags
  printf("-D_ALASKA\n");
  // printf("-D_GNU_SOURCE\n");
  printf("-D__STDC_CONSTANT_MACROS\n");
  printf("-D__STDC_FORMAT_MACROS\n");
  printf("-D__STDC_LIMIT_MACROS\n");
}


// clang-format off
std::map<std::string, std::vector<flag_handler_t>> arg_required = {
    {"--prefix", {prefix}},
    {"--linkdir", {linkdir}},
    {"--includedir", {includedir}},
    {"--ldflags", {ldflags}},
    {"--cflags", {cflags, defs}},
    {"--cxxflags", {cxxflags, defs}},
};
// clang-format on

void usage(void) {
  printf("usage: alaska-config <OPTION>...\n");
  for (auto &[key, val] : arg_required) {
    printf("  %s: ...\n", key.data());
  }
}

int main(int argc, char **argv) {
  std::set<flag_handler_t> required;


  if (argc == 1) {
    usage();
    return EXIT_FAILURE;
  }

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    auto f = arg_required.find(arg);
    if (f == arg_required.end()) {
      usage();
      return EXIT_FAILURE;
    }

    for (auto flag : f->second) {
      required.insert(flag);
    }
  }

  for (auto req : required) {
    req();
  }
  return EXIT_SUCCESS;
}
