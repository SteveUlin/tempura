#include "profiler.h"

int main() {
  tempura::Profiler::beginTracing();
  {
    TEMPURA_TRACE("Scope 0");
    for (int i = 0; i < 10; i++) {
      TEMPURA_TRACE("Scope 1");
      for (int j = 0; j < 10; j++) {
        TEMPURA_TRACE("Scope 2");
        for (int k = 0; k < 10; k++) {
          TEMPURA_TRACE("Scope 3");
        }
      }
    }
  }
  tempura::Profiler::endAndPrintStats();
  return 0;
}
