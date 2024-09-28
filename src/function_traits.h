#pragma once

#include <cmath>
#include <functional>

namespace tempura {

// Type traits for getting arguments from functions

template <typename T>
struct FunctionTraits;

template <typename R, typename... Args>
struct FunctionTraits<R(Args...)> {
  using ResultType = R;

  template <size_t i>
  using ArgT = typename std::tuple_element<i, std::tuple<Args...>>::type;
};

template <typename R, typename... Args>
struct FunctionTraits<R (*)(Args...)> : public FunctionTraits<R(Args...)> {};

template <typename R, typename... Args>
struct FunctionTraits<std::function<R(Args...)>>
    : public FunctionTraits<R(Args...)> {};

template <typename ClassType, typename R, typename... Args>
struct FunctionTraits<R (ClassType::*)(Args...)>
    : public FunctionTraits<R(Args...)> {};

template <typename ClassType, typename R, typename... Args>
struct FunctionTraits<R (ClassType::*)(Args...) const>
    : public FunctionTraits<R(Args...)> {};

// For lambdas and other callable objects
template <typename T>
struct FunctionTraits : public FunctionTraits<decltype(&T::operator())> {};

}  // namespace tempura
