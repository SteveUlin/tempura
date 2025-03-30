#pragma once

#include <utility>

#include "matrix2/matrix.h"

namespace tempura::matrix {

template <MatrixT First, MatrixT... Rest>
  requires(
      std::is_same_v<typename First::ValueType, typename Rest::ValueType> and
      ...)
class BlockRow {
 public:
  using ValueType = typename First::ValueType;

  constexpr static Extent kRow = First::kRow;
  constexpr static Extent kCol = (First::kCol + ... + Rest::kCol);

  constexpr explicit BlockRow(auto&&... data)
      : data_{std::forward<decltype(data)>(data)...} {}

  constexpr auto operator[](this auto&& self, int64_t i, int64_t j)
      -> decltype(auto) {
    if (std::is_constant_evaluated()) {
      CHECK(i < kRow);
      CHECK(j < kCol);
    }
    return std::apply(
        [&](MatrixT auto&... mats) -> decltype(auto) {
          int64_t offset = 0;
          std::conditional_t<
              std::is_const_v<std::remove_reference_t<decltype(self)>>,
              const ValueType*, ValueType*>
              result = nullptr;
          ((j < offset + mats.shape().col
                ? (result = &mats[i, j - offset], true)
                : (offset += mats.shape().col, false)) or
           ...);
          CHECK(result != nullptr);
          return *result;
        },
        self.data_);
  }

  constexpr auto shape() const -> RowCol { return {.row = kRow, .col = kCol}; }

 private:
  std::tuple<First, Rest...> data_;
};

template <typename... Ts>
BlockRow(Ts...) -> BlockRow<std::remove_cvref_t<Ts>...>;

}  // namespace tempura::matrix
