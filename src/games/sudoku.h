#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace tempura {

// ============================================================================
// SudokuBoard
// ============================================================================
// Compact storage for a 9×9 sudoku board. Each cell holds a value 0-9:
//   0 = empty cell
//   1-9 = filled cell with that digit
//
// Uses uint8_t array for fast access and constexpr compatibility.
// No validation or game logic - pure storage.

struct SudokuBoard {
  static constexpr size_t kSize = 9;
  static constexpr size_t kCellCount = kSize * kSize;

  std::array<uint8_t, kCellCount> cells{};

  // C++26 multidimensional subscript - access cell at [row, col]
  constexpr auto operator[](size_t row, size_t col) -> uint8_t& {
    return cells[row * kSize + col];
  }

  constexpr auto operator[](size_t row, size_t col) const -> uint8_t {
    return cells[row * kSize + col];
  }
};

// ============================================================================
// Free Functions
// ============================================================================

// Create board from nicely formatted string literal
//
// Supports multiple formats:
//
// 1. ASCII box format with | and -:
//    "5 3 . | . 7 . | . . ."
//    "6 . . | 1 9 5 | . . ."
//    ...
//
// 2. Unicode box drawing (┌─┬─┐│├─┼─┤└─┴─┘):
//    "┌─────┬─────┬─────┐"
//    "│5 3 .│. 7 .│. . .│"
//    ...
//
// 3. Simple 81-character string:
//    "53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79"
//
// Empty cells: '.'
// Filled cells: '1'-'9'
// Ignored: spaces, |, -, box drawing chars, newlines, other whitespace
constexpr auto makeSudokuBoard(std::string_view str) -> SudokuBoard {
  SudokuBoard board;
  size_t cell_index = 0;

  for (char c : str) {
    if (cell_index >= SudokuBoard::kCellCount) {
      break;
    }

    // Parse digit or empty cell
    if (c >= '1' && c <= '9') {
      board.cells[cell_index++] = static_cast<uint8_t>(c - '0');
    } else if (c == '.') {
      board.cells[cell_index++] = 0;
    }
    // Skip everything else: spaces, |, -, newlines, box drawing chars, etc.
  }

  return board;
}

// Check if a value appears in the given row (ignoring empty cells)
constexpr auto hasValueInRow(const SudokuBoard& board, size_t row,
                              uint8_t value) -> bool {
  for (size_t col = 0; col < SudokuBoard::kSize; ++col) {
    if (board[row, col] == value) {
      return true;
    }
  }
  return false;
}

// Check if a value appears in the given column (ignoring empty cells)
constexpr auto hasValueInCol(const SudokuBoard& board, size_t col,
                              uint8_t value) -> bool {
  for (size_t row = 0; row < SudokuBoard::kSize; ++row) {
    if (board[row, col] == value) {
      return true;
    }
  }
  return false;
}

// Check if a value appears in the 3×3 box containing (row, col)
constexpr auto hasValueInBox(const SudokuBoard& board, size_t row, size_t col,
                              uint8_t value) -> bool {
  size_t box_row = (row / 3) * 3;
  size_t box_col = (col / 3) * 3;

  for (size_t r = box_row; r < box_row + 3; ++r) {
    for (size_t c = box_col; c < box_col + 3; ++c) {
      if (board[r, c] == value) {
        return true;
      }
    }
  }
  return false;
}

// Check if placing a value at (row, col) would be valid
// Does not check if the cell is already occupied - caller should verify
// Returns true if the value doesn't conflict with row/column/box rules
//
// Efficient: checks exactly 20 unique peers, no redundant cell accesses
constexpr auto isValidMove(const SudokuBoard& board, size_t row, size_t col,
                            uint8_t value) -> bool {
  // Empty cells (0) are always "valid" as a move (clearing a cell)
  if (value == 0) {
    return true;
  }

  // Value must be 1-9
  if (value < 1 || value > 9) {
    return false;
  }

  // Check row (8 peers)
  for (size_t c = 0; c < SudokuBoard::kSize; ++c) {
    if (c != col && board[row, c] == value) {
      return false;
    }
  }

  // Check column (8 peers)
  for (size_t r = 0; r < SudokuBoard::kSize; ++r) {
    if (r != row && board[r, col] == value) {
      return false;
    }
  }

  // Check box (4 peers not already checked in row/col)
  size_t box_row = (row / 3) * 3;
  size_t box_col = (col / 3) * 3;
  for (size_t r = box_row; r < box_row + 3; ++r) {
    for (size_t c = box_col; c < box_col + 3; ++c) {
      if (r != row && c != col && board[r, c] == value) {
        return false;
      }
    }
  }

  return true;
}

// Check if the entire board state is valid
// A valid board has no duplicate values in any row, column, or 3×3 box
// Empty cells (0) are ignored and don't count as duplicates
//
// Efficient O(81) implementation: single pass over board checking all constraints
// Uses bitset arrays to track seen values (1-9) for each row/col/box
constexpr auto isValid(const SudokuBoard& board) -> bool {
  uint16_t rows[9] = {};   // Bitset for each row
  uint16_t cols[9] = {};   // Bitset for each column
  uint16_t boxes[9] = {};  // Bitset for each 3×3 box

  for (size_t row = 0; row < SudokuBoard::kSize; ++row) {
    for (size_t col = 0; col < SudokuBoard::kSize; ++col) {
      uint8_t value = board[row, col];
      if (value == 0) continue;

      uint16_t bit = uint16_t{1} << value;
      size_t box_idx = (row / 3) * 3 + (col / 3);

      // Check if value already seen in this row, column, or box
      if ((rows[row] & bit) || (cols[col] & bit) || (boxes[box_idx] & bit)) {
        return false;
      }

      // Mark value as seen
      rows[row] |= bit;
      cols[col] |= bit;
      boxes[box_idx] |= bit;
    }
  }

  return true;
}

// Find next empty cell (value == 0) in row-major order
// Returns {row, col} of empty cell, or {9, 9} if board is full
constexpr auto findEmptyCell(const SudokuBoard& board)
    -> std::pair<size_t, size_t> {
  for (size_t row = 0; row < SudokuBoard::kSize; ++row) {
    for (size_t col = 0; col < SudokuBoard::kSize; ++col) {
      if (board[row, col] == 0) {
        return {row, col};
      }
    }
  }
  return {SudokuBoard::kSize, SudokuBoard::kSize};
}

// Solve sudoku puzzle using backtracking
// Modifies board in-place, returns true if solution found
// Uses depth-first search with constraint checking
constexpr auto solve(SudokuBoard& board) -> bool {
  // Find next empty cell
  auto [row, col] = findEmptyCell(board);

  // Base case: no empty cells means puzzle is solved
  if (row == SudokuBoard::kSize) {
    return true;
  }

  // Try values 1-9
  for (uint8_t value = 1; value <= 9; ++value) {
    if (isValidMove(board, row, col, value)) {
      // Place value
      board[row, col] = value;

      // Recurse
      if (solve(board)) {
        return true;
      }

      // Backtrack
      board[row, col] = 0;
    }
  }

  return false;
}

// Count number of solutions (up to max_count)
// Returns count of valid solutions found
// Useful for verifying puzzle has unique solution (count == 1)
//
// Returns 0 if initial board is invalid
// max_count limits search depth (default 2 is enough to check uniqueness)
constexpr auto countSolutions(SudokuBoard board, size_t max_count = 2)
    -> size_t {
  // Early exit if initial board has conflicts
  if (!isValid(board)) {
    return 0;
  }

  size_t count = 0;

  auto countHelper = [&](auto& self, SudokuBoard& b) -> void {
    if (count >= max_count) {
      return;  // Early exit if we've found enough solutions
    }

    auto [row, col] = findEmptyCell(b);

    // Base case: found a solution
    if (row == SudokuBoard::kSize) {
      ++count;
      return;
    }

    // Try all valid values
    for (uint8_t value = 1; value <= 9; ++value) {
      if (isValidMove(b, row, col, value)) {
        b[row, col] = value;
        self(self, b);
        b[row, col] = 0;
      }
    }
  };

  countHelper(countHelper, board);
  return count;
}

// Check if puzzle has exactly one unique solution
constexpr auto hasUniqueSolution(const SudokuBoard& board) -> bool {
  return countSolutions(board, 2) == 1;
}

// Get all possible valid values for a cell
// Returns bitset where bit N is set if value N (1-9) is valid
// Bit 0 is unused. Empty if cell is already filled.
constexpr auto getCandidates(const SudokuBoard& board, size_t row, size_t col)
    -> uint16_t {
  // If cell is filled, no candidates
  if (board[row, col] != 0) {
    return 0;
  }

  uint16_t candidates = 0;
  for (uint8_t value = 1; value <= 9; ++value) {
    if (isValidMove(board, row, col, value)) {
      candidates |= uint16_t{1} << value;
    }
  }
  return candidates;
}

// Pretty print sudoku board with Unicode box drawing
// Returns a nicely formatted string with grid lines separating 3x3 boxes
auto toString(const SudokuBoard& board) -> std::string {
  std::string result;

  // Top border
  result += "┌───────┬───────┬───────┐\n";

  for (size_t row = 0; row < 9; ++row) {
    result += "│ ";

    for (size_t col = 0; col < 9; ++col) {
      uint8_t value = board[row, col];

      // Print value or empty dot
      if (value == 0) {
        result += ". ";
      } else {
        result += std::format("{} ", value);
      }

      // Add vertical separator after columns 2 and 5
      if (col == 2 || col == 5) {
        result += "│ ";
      }
    }

    result += "│\n";

    // Add horizontal separator after rows 2 and 5
    if (row == 2 || row == 5) {
      result += "├───────┼───────┼───────┤\n";
    }
  }

  // Bottom border
  result += "└───────┴───────┴───────┘";

  return result;
}

}  // namespace tempura
