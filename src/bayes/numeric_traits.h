#pragma once

#include <limits>

namespace tempura::bayes {

// Extension point for custom numeric types to provide infinity value
// Default implementation uses std::numeric_limits<T>::infinity()
//
// Users can override for custom types via ADL by defining in their type's namespace:
//
// Example:
//   namespace mylib {
//     struct MyFloat { ... };
//
//     constexpr auto numeric_infinity(MyFloat) -> MyFloat {
//       return MyFloat::positive_infinity();
//     }
//   }
//
// Then Uniform<mylib::MyFloat> will find numeric_infinity via ADL
//
template <typename T>
constexpr auto numeric_infinity(T) -> T {
  return std::numeric_limits<T>::infinity();
}

}  // namespace tempura::bayes
