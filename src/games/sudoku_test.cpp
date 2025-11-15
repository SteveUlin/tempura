#include "games/sudoku.h"

#include "unit.h"

#include <print>

using namespace tempura;

auto main() -> int {
  "SudokuBoard default construction"_test = [] {
    constexpr SudokuBoard board;
    static_assert(board[0, 0] == 0);
    static_assert(board[8, 8] == 0);
    static_assert(board[4, 4] == 0);
  };

  "SudokuBoard getter/setter with [i,j]"_test = [] {
    SudokuBoard board;

    // Set some values
    board[0, 0] = 5;
    board[0, 1] = 3;
    board[8, 8] = 9;
    board[4, 4] = 7;

    // Verify getters (runtime checks, not compile-time)
    expectEq(board[0, 0], uint8_t{5});
    expectEq(board[0, 1], uint8_t{3});
    expectEq(board[8, 8], uint8_t{9});
    expectEq(board[4, 4], uint8_t{7});

    // Verify other cells are still empty
    expectEq(board[0, 2], uint8_t{0});
    expectEq(board[1, 0], uint8_t{0});
  };

  "SudokuBoard const access"_test = [] {
    SudokuBoard board;
    board[2, 3] = 8;

    const SudokuBoard& const_board = board;
    expectEq(const_board[2, 3], uint8_t{8});
    expectEq(const_board[0, 0], uint8_t{0});
  };

  "makeSudokuBoard simple format"_test = [] {
    constexpr auto board = makeSudokuBoard(
        "53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419.."
        "5....8..79");

    // First row
    static_assert(board[0, 0] == 5);
    static_assert(board[0, 1] == 3);
    static_assert(board[0, 2] == 0);
    static_assert(board[0, 3] == 0);
    static_assert(board[0, 4] == 7);

    // Some cells from middle
    static_assert(board[4, 4] == 0);

    // Last row
    static_assert(board[8, 7] == 7);
    static_assert(board[8, 8] == 9);
  };

  "makeSudokuBoard mixed format"_test = [] {
    constexpr auto board = makeSudokuBoard("53..7....6..195....98....6.");

    static_assert(board[0, 0] == 5);
    static_assert(board[0, 1] == 3);
    static_assert(board[0, 2] == 0);
    static_assert(board[0, 3] == 0);
    static_assert(board[0, 4] == 7);
    static_assert(board[0, 5] == 0);
  };

  "makeSudokuBoard ASCII box format"_test = [] {
    constexpr auto board = makeSudokuBoard(R"(
5 3 . | . 7 . | . . .
6 . . | 1 9 5 | . . .
. 9 8 | . . . | . 6 .
------+-------+------
8 . . | . 6 . | . . 3
4 . . | 8 . 3 | . . 1
7 . . | . 2 . | . . 6
------+-------+------
. 6 . | . . . | 2 8 .
. . . | 4 1 9 | . . 5
. . . | . 8 . | . 7 9
)");

    // First row
    static_assert(board[0, 0] == 5);
    static_assert(board[0, 1] == 3);
    static_assert(board[0, 2] == 0);
    static_assert(board[0, 3] == 0);
    static_assert(board[0, 4] == 7);
    static_assert(board[0, 5] == 0);

    // Second row
    static_assert(board[1, 0] == 6);
    static_assert(board[1, 1] == 0);
    static_assert(board[1, 2] == 0);
    static_assert(board[1, 3] == 1);
    static_assert(board[1, 4] == 9);
    static_assert(board[1, 5] == 5);

    // Middle cell
    static_assert(board[4, 4] == 0);

    // Last row
    static_assert(board[8, 4] == 8);
    static_assert(board[8, 7] == 7);
    static_assert(board[8, 8] == 9);
  };

  "makeSudokuBoard Unicode box format"_test = [] {
    constexpr auto board = makeSudokuBoard(R"(
┌─────────┬─────────┬─────────┐
│ 5 3 . │ . 7 . │ . . . │
│ 6 . . │ 1 9 5 │ . . . │
│ . 9 8 │ . . . │ . 6 . │
├─────────┼─────────┼─────────┤
│ 8 . . │ . 6 . │ . . 3 │
│ 4 . . │ 8 . 3 │ . . 1 │
│ 7 . . │ . 2 . │ . . 6 │
├─────────┼─────────┼─────────┤
│ . 6 . │ . . . │ 2 8 . │
│ . . . │ 4 1 9 │ . . 5 │
│ . . . │ . 8 . │ . 7 9 │
└─────────┴─────────┴─────────┘
)");

    // First row
    static_assert(board[0, 0] == 5);
    static_assert(board[0, 1] == 3);
    static_assert(board[0, 2] == 0);

    // Verify some key positions
    static_assert(board[1, 3] == 1);
    static_assert(board[1, 4] == 9);
    static_assert(board[1, 5] == 5);
    static_assert(board[4, 3] == 8);
    static_assert(board[8, 8] == 9);
  };

  "makeSudokuBoard compact ASCII"_test = [] {
    constexpr auto board = makeSudokuBoard(R"(
53.|.7.|...
6..|195|...
.98|...|.6.
---+---+---
8..|.6.|..3
4..|8.3|..1
7..|.2.|..6
---+---+---
.6.|...|28.
...|419|..5
...|.8.|.79
)");

    static_assert(board[0, 0] == 5);
    static_assert(board[0, 1] == 3);
    static_assert(board[0, 2] == 0);
    static_assert(board[4, 4] == 0);
    static_assert(board[8, 8] == 9);
  };

  "makeSudokuBoard all digits"_test = [] {
    constexpr auto board = makeSudokuBoard("123456789234567891345678912");

    static_assert(board[0, 0] == 1);
    static_assert(board[0, 1] == 2);
    static_assert(board[0, 2] == 3);
    static_assert(board[0, 8] == 9);
    static_assert(board[1, 0] == 2);
    static_assert(board[2, 0] == 3);
  };

  "makeSudokuBoard empty board"_test = [] {
    constexpr auto board = makeSudokuBoard(
        "................................................................................."
        );

    static_assert(board[0, 0] == 0);
    static_assert(board[4, 4] == 0);
    static_assert(board[8, 8] == 0);
  };

  "makeSudokuBoard partial input"_test = [] {
    // Should fill remaining cells with 0
    constexpr auto board = makeSudokuBoard("123");

    static_assert(board[0, 0] == 1);
    static_assert(board[0, 1] == 2);
    static_assert(board[0, 2] == 3);
    static_assert(board[0, 3] == 0);
    static_assert(board[8, 8] == 0);
  };

  "SudokuBoard size constants"_test = [] {
    static_assert(SudokuBoard::kSize == 9);
    static_assert(SudokuBoard::kCellCount == 81);
  };

  "SudokuBoard all positions accessible"_test = [] {
    SudokuBoard board;

    // Fill diagonal with values
    for (size_t i = 0; i < 9; ++i) {
      board[i, i] = static_cast<uint8_t>(i + 1);
    }

    // Verify diagonal
    for (size_t i = 0; i < 9; ++i) {
      expectEq(board[i, i], static_cast<uint8_t>(i + 1));
    }
  };

  "toString empty board"_test = [] {
    SudokuBoard board;
    std::string output = toString(board);

    // Should contain box drawing characters
    expectTrue(output.find("┌") != std::string::npos);
    expectTrue(output.find("└") != std::string::npos);
    expectTrue(output.find("│") != std::string::npos);

    // Should contain dots for empty cells
    expectTrue(output.find(".") != std::string::npos);
  };

  "toString filled board"_test = [] {
    auto board = makeSudokuBoard(R"(
5 3 . | . 7 . | . . .
6 . . | 1 9 5 | . . .
. 9 8 | . . . | . 6 .
------+-------+------
8 . . | . 6 . | . . 3
4 . . | 8 . 3 | . . 1
7 . . | . 2 . | . . 6
------+-------+------
. 6 . | . . . | 2 8 .
. . . | 4 1 9 | . . 5
. . . | . 8 . | . 7 9
)");

    std::string output = toString(board);

    // Should contain some of the digits
    expectTrue(output.find("5") != std::string::npos);
    expectTrue(output.find("3") != std::string::npos);
    expectTrue(output.find("7") != std::string::npos);

    // Should still have empty cells
    expectTrue(output.find(".") != std::string::npos);

    // Print for visual verification during testing
    std::println("\n{}", output);
  };

  return TestRegistry::result();
}
