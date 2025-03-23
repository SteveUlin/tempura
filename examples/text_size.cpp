#include <print>

auto main() -> int {
  std::println("\e]66;s=2;Double sized text\a\e");
  std::println("\e]66;s=3;Triple sized text\a\e");
  std::println("\e]66;n=1:d=2:s=2;Half sized text\a\e");
  return 0;
}

