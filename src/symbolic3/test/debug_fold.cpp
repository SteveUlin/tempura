// Test fold expression pairwise expansion
#include <iostream>

template <typename... Ts, typename... Us>
struct test_fold {
  static void print() {
    ((std::cout << sizeof(Ts) << ":" << sizeof(Us) << " "), ...);
    std::cout << "\n";
  }
};

int main() {
  test_fold<int, char, double, short, long>::print();
  return 0;
}
