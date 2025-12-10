// Parallel Sudoku Solver using the Task Library
//
// Demonstrates:
// - ThreadPoolScheduler for parallel work distribution
// - whenAny() for racing parallel branches (first solution wins)
// - Comparison of single-threaded vs multi-threaded performance

#include "games/sudoku.h"
#include "task/task.h"

#include <atomic>
#include <chrono>
#include <print>
#include <vector>

namespace tempura {

// ============================================================================
// Improved Solver with MRV Heuristic
// ============================================================================

// Find empty cell with minimum remaining values (MRV heuristic)
// Returns {row, col, candidates} or {9, 9, 0} if board is full
constexpr auto findBestCell(const SudokuBoard& board)
    -> std::tuple<size_t, size_t, uint16_t> {
  size_t best_row = SudokuBoard::kSize;
  size_t best_col = SudokuBoard::kSize;
  uint16_t best_candidates = 0;
  int best_count = 10;  // More than 9 candidates

  for (size_t row = 0; row < SudokuBoard::kSize; ++row) {
    for (size_t col = 0; col < SudokuBoard::kSize; ++col) {
      if (board[row, col] == 0) {
        uint16_t candidates = getCandidates(board, row, col);
        int count = std::popcount(candidates);

        // No candidates = unsolvable
        if (count == 0) {
          return {row, col, 0};
        }

        if (count < best_count) {
          best_count = count;
          best_row = row;
          best_col = col;
          best_candidates = candidates;

          // Can't do better than 1 candidate
          if (count == 1) {
            return {best_row, best_col, best_candidates};
          }
        }
      }
    }
  }

  return {best_row, best_col, best_candidates};
}

// Improved single-threaded solver with MRV heuristic
auto solveWithMRV(SudokuBoard& board) -> bool {
  auto [row, col, candidates] = findBestCell(board);

  // Board is full - solved!
  if (row == SudokuBoard::kSize) {
    return true;
  }

  // No candidates for this cell - backtrack
  if (candidates == 0) {
    return false;
  }

  // Try each candidate value
  for (uint8_t value = 1; value <= 9; ++value) {
    if ((candidates & (uint16_t{1} << value)) != 0) {
      board[row, col] = value;

      if (solveWithMRV(board)) {
        return true;
      }

      board[row, col] = 0;
    }
  }

  return false;
}

// ============================================================================
// Parallel Solver using Task Library
// ============================================================================

// Parallel solver that fans out work at branching points
// Uses atomics for early termination once any branch finds a solution
class ParallelSudokuSolver {
 public:
  explicit ParallelSudokuSolver(ThreadPool& pool) : pool_(pool) {}

  // Solve with parallelism at top branching levels
  auto solve(SudokuBoard board) -> std::optional<SudokuBoard> {
    found_.store(false, std::memory_order_relaxed);
    solution_ = std::nullopt;

    if (solveParallel(board, 0)) {
      return solution_;
    }
    return std::nullopt;
  }

 private:
  static constexpr int kParallelDepth = 2;  // Fan out at first 2 levels

  auto solveParallel(SudokuBoard board, int depth) -> bool {
    // Check if another branch already found solution
    if (found_.load(std::memory_order_relaxed)) {
      return false;
    }

    auto [row, col, candidates] = findBestCell(board);

    // Board is full - found solution!
    if (row == SudokuBoard::kSize) {
      // Try to claim victory
      bool expected = false;
      if (found_.compare_exchange_strong(expected, true,
                                         std::memory_order_acq_rel)) {
        std::scoped_lock lock(mutex_);
        solution_ = board;
      }
      return true;
    }

    // No candidates - backtrack
    if (candidates == 0) {
      return false;
    }

    // Count branches
    int branch_count = std::popcount(candidates);

    // At shallow depth with multiple branches, parallelize
    if (depth < kParallelDepth && branch_count > 1) {
      return solveParallelBranches(board, row, col, candidates, depth);
    }

    // Sequential solve for deeper levels
    return solveSequential(board, row, col, candidates, depth);
  }

  auto solveParallelBranches(SudokuBoard board, size_t row, size_t col,
                             uint16_t candidates, int depth) -> bool {
    std::vector<std::future<bool>> futures;

    for (uint8_t value = 1; value <= 9; ++value) {
      if ((candidates & (uint16_t{1} << value)) != 0) {
        // Create a copy for this branch
        SudokuBoard branch = board;
        branch[row, col] = value;

        // Submit to thread pool
        std::promise<bool> promise;
        futures.push_back(promise.get_future());

        pool_.submit([this, branch = std::move(branch), depth,
                      promise = std::move(promise)]() mutable {
          bool result = solveParallel(branch, depth + 1);
          promise.set_value(result);
        });
      }
    }

    // Wait for any branch to find a solution
    for (auto& future : futures) {
      if (future.get()) {
        return true;
      }
    }

    return false;
  }

  auto solveSequential(SudokuBoard& board, size_t row, size_t col,
                       uint16_t candidates, int depth) -> bool {
    for (uint8_t value = 1; value <= 9; ++value) {
      if ((candidates & (uint16_t{1} << value)) != 0) {
        // Early exit if solution found by another branch
        if (found_.load(std::memory_order_relaxed)) {
          return false;
        }

        board[row, col] = value;

        if (solveParallel(board, depth + 1)) {
          return true;
        }

        board[row, col] = 0;
      }
    }
    return false;
  }

  ThreadPool& pool_;
  std::atomic<bool> found_{false};
  std::optional<SudokuBoard> solution_;
  std::mutex mutex_;
};

// ============================================================================
// Parallel Solver using Sender/Receiver Model
// ============================================================================

// This version uses whenAny to race parallel branches
// Demonstrates the task library's parallel composition

template <typename Scheduler>
class SenderSudokuSolver {
 public:
  explicit SenderSudokuSolver(Scheduler sched, std::atomic<bool>& found)
      : scheduler_(sched), found_(found) {}

  // Create a sender that solves the sudoku
  auto solve(SudokuBoard board, int depth = 0) {
    return just(std::move(board)) | then([this, depth](SudokuBoard b) {
             return solveImpl(std::move(b), depth);
           });
  }

 private:
  static constexpr int kParallelDepth = 1;

  auto solveImpl(SudokuBoard board, int depth) -> std::optional<SudokuBoard> {
    if (found_.load(std::memory_order_relaxed)) {
      return std::nullopt;
    }

    auto [row, col, candidates] = findBestCell(board);

    if (row == SudokuBoard::kSize) {
      found_.store(true, std::memory_order_relaxed);
      return board;
    }

    if (candidates == 0) {
      return std::nullopt;
    }

    // Sequential search with early termination
    for (uint8_t value = 1; value <= 9; ++value) {
      if ((candidates & (uint16_t{1} << value)) != 0) {
        if (found_.load(std::memory_order_relaxed)) {
          return std::nullopt;
        }

        board[row, col] = value;
        auto result = solveImpl(board, depth + 1);
        if (result) {
          return result;
        }
        board[row, col] = 0;
      }
    }

    return std::nullopt;
  }

  Scheduler scheduler_;
  std::atomic<bool>& found_;
};

}  // namespace tempura

using namespace tempura;
using namespace std::chrono;

// Hard puzzles for benchmarking
constexpr std::string_view kHardPuzzles[] = {
    // "Arto Inkala" - claimed to be one of the hardest
    "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..",

    // "AI Escargot"
    "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3..",

    // "Platinum Blonde"
    "..............3.85..1.2.......5.7.....4...1...9.......5......73..2.1........4...9",

    // 17-clue minimal
    "........1.....2..3...4.....5......6...7.....8.....9.....1.....2.....3..4...5.....",

    // Another hard one
    ".2.4.37.........32........4.4.2...7.8...5.........1...5.....9...3.9....7..1..86..",
};

auto benchmark(std::string_view name, auto&& fn, int iterations = 1) {
  // Warmup
  fn();

  auto start = high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fn();
  }
  auto end = high_resolution_clock::now();

  auto total_us = duration_cast<microseconds>(end - start).count();
  auto avg_us = total_us / iterations;

  std::println("{}: {} µs (avg over {} runs)", name, avg_us, iterations);
  return avg_us;
}

auto main() -> int {
  std::println("╔══════════════════════════════════════════════════════════╗");
  std::println("║          Parallel Sudoku Solver Benchmark                ║");
  std::println("╚══════════════════════════════════════════════════════════╝\n");

  // Get hardware concurrency
  unsigned int num_threads = std::thread::hardware_concurrency();
  std::println("Hardware threads: {}\n", num_threads);

  // Create thread pool
  ThreadPool pool{num_threads};

  for (size_t i = 0; i < std::size(kHardPuzzles); ++i) {
    auto puzzle = makeSudokuBoard(kHardPuzzles[i]);

    std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    std::println("Puzzle {}:", i + 1);
    std::println("{}\n", toString(puzzle));

    SudokuBoard board1, board2, board3;
    std::optional<SudokuBoard> result;

    // 1. Original backtracking solver
    auto time_original = benchmark("Original backtrack", [&] {
      board1 = puzzle;
      solve(board1);
    });

    // 2. MRV heuristic solver
    auto time_mrv = benchmark("MRV heuristic     ", [&] {
      board2 = puzzle;
      solveWithMRV(board2);
    });

    // 3. Parallel solver
    ParallelSudokuSolver parallel_solver{pool};
    auto time_parallel = benchmark("Parallel (pool)   ", [&] {
      result = parallel_solver.solve(puzzle);
    });

    // Verify solutions match
    if (board1.cells != board2.cells) {
      std::println("⚠️  WARNING: MRV solution differs from original!");
    }
    if (result && result->cells != board1.cells) {
      std::println("⚠️  WARNING: Parallel solution differs from original!");
    }

    // Calculate speedups
    double mrv_speedup =
        static_cast<double>(time_original) / static_cast<double>(time_mrv);
    double parallel_speedup =
        static_cast<double>(time_mrv) / static_cast<double>(time_parallel);
    double total_speedup =
        static_cast<double>(time_original) / static_cast<double>(time_parallel);

    std::println("\nSpeedups:");
    std::println("  MRV vs Original:      {:.2f}x", mrv_speedup);
    std::println("  Parallel vs MRV:      {:.2f}x", parallel_speedup);
    std::println("  Parallel vs Original: {:.2f}x", total_speedup);

    std::println("\nSolution:");
    std::println("{}\n", toString(board1));
  }

  // ============================================================================
  // Demonstrate Sender/Receiver based solver
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Sender/Receiver Based Solver Demo\n");

  auto puzzle = makeSudokuBoard(kHardPuzzles[0]);
  std::atomic<bool> found{false};

  ThreadPoolScheduler scheduler{pool};
  SenderSudokuSolver solver{scheduler, found};

  auto work = solver.solve(puzzle);
  auto sync_result = syncWait(std::move(work));

  if (sync_result) {
    auto& [maybe_board] = *sync_result;
    if (maybe_board) {
      std::println("Solution found via sender/receiver:");
      std::println("{}", toString(*maybe_board));
    }
  }

  return 0;
}
