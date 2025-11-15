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
