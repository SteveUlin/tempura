#pragma once

#include <atomic>
#include <functional>
#include <thread>

#include "matrix/addition.h"
#include "matrix/matrix.h"
#include "matrix/printer.h"
#include "matrix/storage/dense.h"

namespace tempura::matrix {

template <typename Lhs, typename Rhs>
concept Conformable =
    MatrixT<Lhs> and MatrixT<Rhs> and
    (Lhs::kExtent.col == Rhs::kExtent.row or Lhs::kExtent.col == kDynamic or
     Rhs::kExtent.row == kDynamic);

template <MatrixT Lhs, MatrixT Rhs, MatrixT Out>
  requires(Conformable<Lhs, Rhs> &&
           matchExtent(Out::kExtent, {Lhs::kExtent.row, Rhs::kExtent.col}))
auto naiveMultiplyAdd(const Lhs& left, const Rhs& right, Out& out) -> void {
  CHECK(left.shape().col == right.shape().row);
  for (int64_t i = 0; i < left.shape().row; ++i) {
    for (int64_t j = 0; j < right.shape().col; ++j) {
      for (int64_t k = 0; k < left.shape().col; ++k) {
        out[i, j] += left[i, k] * right[k, j];
      }
    }
  }
}

template <int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs, MatrixT Out>
  requires(Conformable<Lhs, Rhs> &&
           matchExtent(Out::kExtent, {Lhs::kExtent.row, Rhs::kExtent.col}))
auto blockMultiply(const Lhs& left, const Rhs& right, Out& out) -> void {
  CHECK(left.shape().col == right.shape().row);
  for (int64_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
    for (int64_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      for (int64_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
        for (int64_t i = iblock;
             i < std::min(iblock + block_size, left.shape().row); ++i) {
          for (int64_t j = jblock;
               j < std::min(jblock + block_size, right.shape().col); ++j) {
            for (int64_t k = kblock;
                 k < std::min(kblock + block_size, left.shape().col); ++k) {
              out[i, j] += left[i, k] * right[k, j];
            }
          }
        }
      }
    }
  }
}

template <int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs, MatrixT Out>
  requires(Conformable<Lhs, Rhs> &&
           matchExtent(Out::kExtent, {Lhs::kExtent.row, Rhs::kExtent.col}))
auto revBlockMultiply(const Lhs& left, const Rhs& right, Out& out) -> void {
  CHECK(left.shape().col == right.shape().row);
  for (int64_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
    for (int64_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      for (int64_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
        for (int64_t i = iblock;
             i < std::min(iblock + block_size, left.shape().row); ++i) {
          for (int64_t j = jblock;
               j < std::min(jblock + block_size, right.shape().col); ++j) {
            for (int64_t k = kblock;
                 k < std::min(kblock + block_size, left.shape().col); ++k) {
              out[i, j] += left[i, k] * right[k, j];
            }
          }
        }
      }
    }
  }
}

template <int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs, MatrixT Out>
  requires(Conformable<Lhs, Rhs> &&
           matchExtent(Out::kExtent, {Lhs::kExtent.row, Rhs::kExtent.col}))
auto bufMultiply(const Lhs& left, const Rhs& right, Out& out) -> void {
  CHECK(left.shape().col == right.shape().row);

  for (int64_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
    for (int64_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      alignas(64) static thread_local std::array<typename Rhs::ScalarT,
                                                 block_size * block_size>
          r_buf = {};
      for (int64_t j = 0;
           j + jblock < std::min(jblock + block_size, right.shape().col); ++j) {
        for (int64_t k = 0;
             k + kblock < std::min(kblock + block_size, left.shape().col);
             ++k) {
          // Transpose the buffer
          r_buf[k + j * block_size] = right[k + kblock, j + jblock];
        }
      }

      for (int64_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
        alignas(64) static thread_local auto l_buf =
            std::array<typename Lhs::ScalarT, block_size * block_size>{};
        for (int64_t i = 0;
             i + iblock < std::min(iblock + block_size, left.shape().row);
             ++i) {
          for (int64_t k = 0;
               k + kblock < std::min(kblock + block_size, left.shape().col);
               ++k) {
            l_buf[k + i * block_size] = left[i + iblock, k + kblock];
          }
        }

        for (int64_t i = 0;
             i + iblock < std::min(iblock + block_size, left.shape().row);
             ++i) {
          for (int64_t j = 0;
               j + jblock < std::min(jblock + block_size, right.shape().col);
               ++j) {
            for (int64_t k = 0;
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
}

template <int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs, MatrixT Out>
  requires(Conformable<Lhs, Rhs> &&
           matchExtent(Out::kExtent, {Lhs::kExtent.row, Rhs::kExtent.col}))
auto tileMultiply(const Lhs& left, const Rhs& right, Out& out) -> void {
  CHECK(left.shape().col == right.shape().row);
  for (int64_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
    for (int64_t i = 0; i < left.shape().row; ++i) {
      for (int64_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
        for (int64_t j = jblock;
             j < std::min(jblock + block_size, right.shape().col); ++j) {
          for (int64_t k = kblock;
               k < std::min(kblock + block_size, left.shape().col); ++k) {
            out[i, j] += left[i, k] * right[k, j];
          }
        }
      }
    }
  }
}

template <int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs, MatrixT Out>
  requires(Conformable<Lhs, Rhs> &&
           matchExtent(Out::kExtent, {Lhs::kExtent.row, Rhs::kExtent.col}))
auto bufMultiplySlice(const Lhs& left, const Rhs& right, Out& out) -> void {
  CHECK(left.shape().col == right.shape().row);

  for (int64_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
    for (int64_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      Dense<typename Rhs::ScalarT, {kDynamic, kDynamic}, IndexOrder::kColMajor>
          r_buf = Slice({std::min(block_size, right.shape().col - kblock),
                         std::min(block_size, right.shape().row - jblock)},
                        {kblock, jblock}, right);
      for (int64_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
        Dense<typename Rhs::ScalarT, {kDynamic, kDynamic},
              IndexOrder::kRowMajor>
            l_buf = Slice({std::min(block_size, left.shape().col - iblock),
                           std::min(block_size, left.shape().row - kblock)},
                          {iblock, kblock}, left);
        Slice slice({std::min(block_size, left.shape().col - iblock),
                     std::min(block_size, right.shape().row - jblock)},
                    {iblock, jblock}, out);
        naiveMultiplyAdd(l_buf, r_buf, slice);

        // naiveMultiply(l_buf, r_buf);
      }
    }
  }
}

template <int64_t N>
void parDo(std::span<std::function<void()>> tasks) {
  std::atomic<int64_t> next_task = 0;

  auto work = [&next_task, &tasks]() {
    while (true) {
      int64_t index = next_task++;
      if (index >= tasks.size()) {
        return;
      }
      tasks[index]();
    }
  };
  std::vector<std::thread> threads;
  for (int64_t i = 0; i < N; ++i) {
    threads.push_back(std::thread(work));
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

template <int64_t N = 32, int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs,
          MatrixT Out>
  requires(Conformable<Lhs, Rhs> &&
           matchExtent(Out::kExtent, {Lhs::kExtent.row, Rhs::kExtent.col}))
auto bufMultiplyThreadpool(const Lhs& left, const Rhs& right,
                           Out& out) -> void {
  CHECK(left.shape().col == right.shape().row);

  std::vector<std::function<void()>> tasks;

  for (int64_t iblock = 0; iblock < left.shape().row; iblock += block_size) {
    for (int64_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
      tasks.push_back([iblock, jblock, &left, &right, &out]() {
        for (int64_t kblock = 0; kblock < left.shape().col;
             kblock += block_size) {
          alignas(64) static thread_local std::array<typename Rhs::ScalarT,
                                                     block_size * block_size>
              r_buf = {};

          for (int64_t j = 0;
               j + jblock < std::min(jblock + block_size, right.shape().col);
               ++j) {
            for (int64_t k = 0;
                 k + kblock < std::min(kblock + block_size, left.shape().col);
                 ++k) {
              // Transpose the buffer
              r_buf[k + j * block_size] = right[k + kblock, j + jblock];
            }
          }

          alignas(64) static thread_local auto l_buf =
              std::array<typename Lhs::ScalarT, block_size * block_size>{};
          for (int64_t i = 0;
               i + iblock < std::min(iblock + block_size, left.shape().row);
               ++i) {
            for (int64_t k = 0;
                 k + kblock < std::min(kblock + block_size, left.shape().col);
                 ++k) {
              l_buf[k + i * block_size] = left[i + iblock, k + kblock];
            }
          }

          for (int64_t i = 0;
               i + iblock < std::min(iblock + block_size, left.shape().row);
               ++i) {
            for (int64_t j = 0;
                 j + jblock < std::min(jblock + block_size, right.shape().col);
                 ++j) {
              for (int64_t k = 0;
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
}

template <MatrixT Lhs, MatrixT Rhs>
  requires(Conformable<Lhs, Rhs>)
auto operator*(const Lhs& left, const Rhs& right) {
  using ScalarT = decltype(left[0, 0] * right[0, 0]);
  Dense<ScalarT, {Lhs::kExtent.row, Rhs::kExtent.col}> out;
  bufMultiply(left, right, out);
  return out;
}

template <MatrixT Lhs, typename Rhs>
  requires(!MatrixT<Rhs>)
auto operator*=(Lhs& left, const Rhs& right) {
  for (int64_t i = 0; i < left.shape().col; ++i) {
    for (int64_t j = 0; j < left.shape().row; ++j) {
      left[i, j] *= right;
    }
  }
  return left;
}

template <MatrixT Lhs, typename Rhs>
  requires(!MatrixT<Rhs>)
auto operator*=(Lhs&& left, const Rhs& right) {
  for (int64_t i = 0; i < left.shape().col; ++i) {
    for (int64_t j = 0; j < left.shape().row; ++j) {
      left[i, j] *= right;
    }
  }
  return left;
}

template <MatrixT Lhs, typename Rhs>
  requires(!MatrixT<Rhs>)
auto operator/=(Lhs& left, const Rhs& right) {
  for (int64_t i = 0; i < left.shape().col; ++i) {
    for (int64_t j = 0; j < left.shape().row; ++j) {
      left[i, j] /= right;
    }
  }
  return left;
}

template <MatrixT Lhs, typename Rhs>
  requires(!MatrixT<Rhs>)
auto operator/=(Lhs&& left, const Rhs& right) {
  for (int64_t i = 0; i < left.shape().col; ++i) {
    for (int64_t j = 0; j < left.shape().row; ++j) {
      left[i, j] /= right;
    }
  }
  return left;
}

template <typename Lhs, MatrixT Rhs>
  requires(!MatrixT<Lhs>)
auto operator*(const Lhs& left, const Rhs& right) {
  using ScalarT = decltype(left * right[0, 0]);
  Dense<ScalarT, {Rhs::kExtent.row, Rhs::kExtent.col}> out;
  for (int64_t i = 0; i < right.shape().row; ++i) {
    for (int64_t j = 0; j < right.shape().row; ++j) {
      out[i, j] = left * right[i, j];
    }
  }
  return out;
}

}  // namespace tempura::matrix
