#pragma once

// Defines the FnRecord class template for recording function call results.

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tempura {

// FnRecord records the result of applying a function or callable object to a
// set of arguments.
//
// It stores a copy (or potentially moved values) of the input arguments in a
// std::tuple and the corresponding output value. It mimics std::invoke behavior
// for the function call during construction.
//
// Provides accessors for inputs and output, and supports structured
// bindings via a custom get<I>() method and std namespace specializations.
template <typename OutputT, typename... Args>
class FnRecord {
 public:
  // Constructs an FnRecord by invoking a function and storing args and result.
  template <typename Func, typename... CtorArgs>
  constexpr FnRecord(Func&& func, CtorArgs&&... args)
      : input_{std::forward<CtorArgs>(args)...},
        output_{std::apply(std::forward<Func>(func), input_)} {}

  auto input() const -> const std::tuple<Args...>& { return input_; }

  auto output() const -> const OutputT& { return output_; }

  // Provides access to elements for structured binding support.
  template <std::size_t I>
    requires(I < sizeof...(Args) + 1)
  auto get(this auto&& self) -> decltype(auto) {
    if constexpr (I == sizeof...(Args)) {
      return (self.output_);
    } else {
      return std::get<I>(self.input_);
    }
  }

 private:
  std::tuple<Args...> input_;
  OutputT output_;
};

// Deduction guide for FnRecord class template arguments (CTAD).
template <typename Func, typename... CtorArgs>
FnRecord(Func&& func, CtorArgs&&... args)
    -> FnRecord<std::invoke_result_t<Func&, std::decay_t<CtorArgs>&...>,
                std::decay_t<CtorArgs>...>;

}  // namespace tempura

// --- std::tuple_size and std::tuple_element specializations ---
// These are necessary for structured binding support with FnRecord.
// They must be defined in the `std` namespace.
namespace std {

// Reports the number of elements accessible via get<I> (inputs + output).
template <typename OutputT, typename... Args>
struct tuple_size<tempura::FnRecord<OutputT, Args...>>
    : std::integral_constant<std::size_t, sizeof...(Args) + 1> {};

// Specialization of std::tuple_element for FnRecord (Input Arguments).
template <std::size_t I, typename OutputT, typename... Args>
  requires(I < sizeof...(Args))
struct tuple_element<I, tempura::FnRecord<OutputT, Args...>> {
  using type = std::tuple_element_t<I, std::tuple<Args...>>;
};

// Specialization of std::tuple_element for FnRecord (Output Value).
template <std::size_t I, typename OutputT, typename... Args>
  requires(I == sizeof...(Args))
struct tuple_element<I, tempura::FnRecord<OutputT, Args...>> {
  using type = OutputT;
};

}  // namespace std
