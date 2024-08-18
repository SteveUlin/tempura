#include <iostream>
#include "profiler.h"

auto main() -> int {
  {
    TEMPURA_TRACE();
    for (int i = 0; i < 10; i++) {
      TEMPURA_TRACE();
      for (int j = 0; j < 10; j++) {
        TEMPURA_TRACE();
        for (int k = 0; k < 10; k++) {
          TEMPURA_TRACE();
        }
      }
    }
  }
  return 0;
}

