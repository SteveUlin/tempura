#pragma once

namespace tempura {

// Overloaded is a class that combines a set of functions into one function
// object.
//
// This is particularly useful for std::visit, which requires a single callable
// object that can handle multiple types.
template <typename... Ts>
struct Overloaded : public Ts... {
  using Ts::operator()...;
};

// Ranked Overload Resolution for template dispatching.
//
// You can defined implementation functions with the tag type as the first
// arg. The Arg witht he highest rank will be preferred.
//
// Example:
//   template <typename T>
//   void foo(Rank3, T&& arg) {...}
struct Rank0 {};
struct Rank1 : Rank0 {};
struct Rank2 : Rank1 {};
struct Rank3 : Rank2 {};
struct Rank4 : Rank3 {};
struct Rank5 : Rank4 {};
struct Rank6 : Rank5 {};

}  // namespace tempurastruct
