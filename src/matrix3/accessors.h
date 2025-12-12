#pragma once

#include <ranges>
#include <vector>

namespace tempura::matrix3 {

template <typename Range>
class RangeAccessor {
 public:
  constexpr RangeAccessor()
    requires(std::default_initializable<Range>)
  = default;

  template <std::ranges::viewable_range Rng>
    requires(std::ranges::random_access_range<Rng>)
  constexpr RangeAccessor(Rng&& range) : data_(std::forward<Rng>(range)) {}

  template <typename IndexType>
  constexpr auto operator()(this auto&& self, IndexType index)
      -> decltype(auto) {
    return self.data_[index];
  }

  constexpr auto data(this auto&& self) -> decltype(auto) { return self.data_; }

 private:
  Range data_;
};

template <typename Scalar>
class IdentityAccessor {
public:
  // Rely on copy elision to make this efficient.
  template <typename... Indices>
  static constexpr auto operator()(std::tuple<Indices...> tuple) -> Scalar {
    return std::apply(
        [](auto&& first, auto&&... indices) {
          return ((first == indices) && ...) ? Scalar{1} : Scalar{0};
        },
        tuple);
  }
};


}  // namespace tempura::matrix3
