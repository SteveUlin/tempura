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
  template <typename... Indicies>
  static constexpr auto operator()(std::tuple<Indicies...> tuple) -> Scalar {
    return std::apply(
        [](auto&& first, auto&&... indicies) {
          return ((first == indicies) && ...) ? Scalar{1} : Scalar{0};
        },
        tuple);
  }
};

// template <typename T>
// class CompressedSparseAccessor {
//  public:
//   void insert(T value, std::size_t a, std::size_t b) {
//     // if index a is in the table, we also need to access index a + 1 for
//     // consistency
//     while (a + 2 < primary_index_.size()) {
//       primary_index_.push_back(secondary_index_.size());
//     }
//
//     auto itr =
//         std::ranges::next(secondary_index_.begin(), primary_index_[a]);
//     const auto last =
//       std::ranges::next(secondary_index_.begin(), primary_index_[a + 1]);
//
//     for (; itr < last; ++itr) {
//       if (itr->first >= b) {
//         break;
//       }
//     }
//     std::size_t index;
//     if (itr == last) {
//
//     }
//     if (itr->first == b) {
//       data_[itr->second] = value;
//       return;
//     }
//     if (itr->first == b) {
//       data_[itr->second] = value;
//       return;
//     }
//     data_.insert(index, value);
//     secondary_index_.insert(itr, {b, index});
//
//     for () {
//       auto& [_, data_index] : secondary_index_) {
//         if (data_index >= itr->second) {
//           ++data_index;
//         }
//       }
//     }
//   }
//
//  private:
//   // Lookup indexes into the secondary lookup table, index i points
//   // to where M[i, *] values start.
//   std::vector<std::size_t> primary_index_;
//   // Lookups pairs of <j in M[*, j], index into data>
//   std::vector<std::pair<std::size_t, std::size_t>> secondary_index_;
//   std::vector<T> data_;
// };

}  // namespace tempura::matrix3
