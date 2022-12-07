#pragma once

#include <vector>

namespace alaska {
  static std::vector<const char *> wrapped_functions = {
      // these functions get replaced with `alaska_wrap_*` versions
      "puts",
  };
}
