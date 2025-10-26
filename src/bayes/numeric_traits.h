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

// Extension point for custom numeric types to provide quiet NaN value
// Default implementation uses std::numeric_limits<T>::quiet_NaN()
//
// Users can override for custom types via ADL by defining in their type's namespace:
//
// Example:
//   namespace mylib {
//     struct MyFloat { ... };
//
//     constexpr auto numeric_quiet_nan(MyFloat) -> MyFloat {
//       return MyFloat::not_a_number();
//     }
//   }
//
// Then Cauchy<mylib::MyFloat> will find numeric_quiet_nan via ADL
//
template <typename T>
constexpr auto numeric_quiet_nan(T) -> T {
  return std::numeric_limits<T>::quiet_NaN();
}

}  // namespace tempura::bayes
