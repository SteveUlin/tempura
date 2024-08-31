#pragma once

#include <atomic>
#include <functional>
#include <thread>

#include "matrix/storage/dense.h"
#include "matrix/matrix.h"

namespace tempura::matrix {

template <typename Lhs, typename Rhs>
concept Conformable =
    MatrixT<Lhs> and MatrixT<Rhs> and
    (Lhs::kExtent.col == Rhs::kExtent.row or Lhs::kExtent.col == kDynamic or
     Rhs::kExtent.row == kDynamic);

template <MatrixT Lhs, MatrixT Rhs>
  requires Conformable<Lhs, Rhs>
auto naiveMultiply(const Lhs& left, const Rhs& right) {
  using Scalar = decltype(std::declval<typename Lhs::ScalarT>() *
                          std::declval<typename Rhs::ScalarT>());
  CHECK(left.shape().col == right.shape().row);
  Dense<Scalar, {Lhs::kExtent.row, Lhs::kExtent.col}, Lhs::kIndexOrder> out{
      RowCol{Lhs::kExtent.row, Rhs::kExtent.col}};

  for (size_t i = 0; i < left.shape().row; ++i) {
    for (size_t j = 0; j < right.shape().col; ++j) {
      for (size_t k = 0; k < left.shape().col; ++k) {
        out[i, j] += left[i, k] * right[k, j];
      }
    }
  }

  return out;
}

template <size_t block_size = 16, MatrixT Lhs, MatrixT Rhs>
  requires Conformable<Lhs, Rhs>
auto blockMultiply(const Lhs& left, const Rhs& right) {
  using Scalar = decltype(std::declval<typename Lhs::ScalarT>() *
                          std::declval<typename Rhs::ScalarT>());
  CHECK(left.shape().col == right.shape().row);
  Dense<Scalar, {Lhs::kExtent.row, Lhs::kExtent.col}, Lhs::kIndexOrder> out{
      RowCol{Lhs::kExtent.row, Rhs::kExtent.col}};

  for (size_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
    for (size_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      for (size_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
        for (size_t i = iblock;
             i < std::min(iblock + block_size, left.shape().row); ++i) {
          for (size_t j = jblock;
               j < std::min(jblock + block_size, right.shape().col); ++j) {
            for (size_t k = kblock;
                 k < std::min(kblock + block_size, left.shape().col); ++k) {
              out[i, j] += left[i, k] * right[k, j];
            }
          }
        }
      }
    }
  }

  return out;
}

template <size_t block_size = 16, MatrixT Lhs, MatrixT Rhs>
  requires Conformable<Lhs, Rhs>
auto revBlockMultiply(const Lhs& left, const Rhs& right) {
  using Scalar = decltype(std::declval<typename Lhs::ScalarT>() *
                          std::declval<typename Rhs::ScalarT>());
  CHECK(left.shape().col == right.shape().row);
  Dense<Scalar, {Lhs::kExtent.row, Lhs::kExtent.col}, Lhs::kIndexOrder> out{
      RowCol{Lhs::kExtent.row, Rhs::kExtent.col}};

  for (size_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
    for (size_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      for (size_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
        for (size_t i = iblock;
             i < std::min(iblock + block_size, left.shape().row); ++i) {
          for (size_t j = jblock;
               j < std::min(jblock + block_size, right.shape().col); ++j) {
            for (size_t k = kblock;
                 k < std::min(kblock + block_size, left.shape().col); ++k) {
              out[i, j] += left[i, k] * right[k, j];
            }
          }
        }
      }
    }
  }

  return out;
}

template <size_t block_size = 128, MatrixT Lhs, MatrixT Rhs>
  requires Conformable<Lhs, Rhs>
auto bufMultiply(const Lhs& left, const Rhs& right) {
  using Scalar = decltype(std::declval<typename Lhs::ScalarT>() *
                          std::declval<typename Rhs::ScalarT>());
  CHECK(left.shape().col == right.shape().row);
  Dense<Scalar, {Lhs::kExtent.row, Lhs::kExtent.col}, Lhs::kIndexOrder> out{
      RowCol{Lhs::kExtent.row, Rhs::kExtent.col}};

  for (size_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
    for (size_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      alignas(64) static thread_local std::array<typename Rhs::ScalarT,
                                                 block_size * block_size>
          r_buf = {};
      for (size_t j = 0;
           j + jblock < std::min(jblock + block_size, right.shape().col); ++j) {
        for (size_t k = 0;
             k + kblock < std::min(kblock + block_size, left.shape().col);
             ++k) {
          // Transpose the buffer
          r_buf[k + j * block_size] = right[k + kblock, j + jblock];
        }
      }

      for (size_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
        alignas(64) static thread_local auto l_buf =
            std::array<typename Lhs::ScalarT, block_size * block_size>{};
        for (size_t i = 0;
             i + iblock < std::min(iblock + block_size, left.shape().row);
             ++i) {
          for (size_t k = 0;
               k + kblock < std::min(kblock + block_size, left.shape().col);
               ++k) {
            l_buf[k + i * block_size] = left[i + iblock, k + kblock];
          }
        }

        for (size_t i = 0;
             i + iblock < std::min(iblock + block_size, left.shape().row);
             ++i) {
          for (size_t j = 0;
               j + jblock < std::min(jblock + block_size, right.shape().col);
               ++j) {
            for (size_t k = 0;
                 k + kblock < std::min(kblock + block_size, left.shape().col);
                 ++k) {
              out[i + iblock, j + jblock] +=
                  l_buf[k + i * block_size] * r_buf[k + j * block_size];
            }
          }
        }
      }
    }
  }

  return out;
}

template <size_t N>
void parDo(std::span<std::function<void()>> tasks) {
  std::atomic<size_t> next_task = 0;

  auto work = [&next_task, &tasks]() {
    while (true) {
      size_t index = next_task++;
      if (index >= tasks.size()) {
        return;
      }
      tasks[index]();
    }
  };
  std::vector<std::thread> threads;
  for (size_t i = 0; i < N; ++i) {
    threads.push_back(std::thread(work));
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

template <size_t N = 32, size_t block_size = 128, MatrixT Lhs, MatrixT Rhs>
  requires Conformable<Lhs, Rhs>
auto bufMultiplyThreadpool(const Lhs& left, const Rhs& right) {
  using Scalar = decltype(std::declval<typename Lhs::ScalarT>() *
                          std::declval<typename Rhs::ScalarT>());
  CHECK(left.shape().col == right.shape().row);
  Dense<Scalar, {Lhs::kExtent.row, Lhs::kExtent.col}, Lhs::kIndexOrder> out{
      RowCol{Lhs::kExtent.row, Rhs::kExtent.col}};

  std::vector<std::function<void()>> tasks;

  for (size_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
    for (size_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      tasks.push_back([iblock, jblock, &left, &right, &out]() {
        for (size_t kblock = 0; kblock < left.shape().col;
             kblock += block_size) {
          alignas(64) static thread_local std::array<typename Rhs::ScalarT,
                                                     block_size * block_size>
              r_buf = {};

          for (size_t j = 0;
               j + jblock < std::min(jblock + block_size, right.shape().col);
               ++j) {
            for (size_t k = 0;
                 k + kblock < std::min(kblock + block_size, left.shape().col);
                 ++k) {
              // Transpose the buffer
              r_buf[k + j * block_size] = right[k + kblock, j + jblock];
            }
          }

          alignas(64) static thread_local auto l_buf =
              std::array<typename Lhs::ScalarT, block_size * block_size>{};
          for (size_t i = 0;
               i + iblock < std::min(iblock + block_size, left.shape().row);
               ++i) {
            for (size_t k = 0;
                 k + kblock < std::min(kblock + block_size, left.shape().col);
                 ++k) {
              l_buf[k + i * block_size] = left[i + iblock, k + kblock];
            }
          }

          for (size_t i = 0;
               i + iblock < std::min(iblock + block_size, left.shape().row);
               ++i) {
            for (size_t j = 0;
                 j + jblock < std::min(jblock + block_size, right.shape().col);
                 ++j) {
              for (size_t k = 0;
                   k + kblock < std::min(kblock + block_size, left.shape().col);
                   ++k) {
                out[i + iblock, j + jblock] +=
                    l_buf[k + i * block_size] * r_buf[k + j * block_size];
              }
            }
          }
        }
      });
    }
  }

  parDo<N>(tasks);

  return out;
}

}  // namespace tempura::matrix
