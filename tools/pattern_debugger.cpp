// Pattern Debugger - Interactive tool to review Unicode block characters
// against their programmatic patterns
//
// Controls:
//   j/↓     - Next character
//   k/↑     - Previous character
//   J       - Jump forward 10
//   K       - Jump backward 10
//   g       - Go to first character
//   G       - Go to last character
//   1-4     - Jump to category (1=sextants, 2=mosaics, 3=octants, 4=sixteenths)
//   /       - Search by codepoint (hex)
//   q       - Quit

#include <algorithm>
#include <cstdio>
#include <print>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include "block_gfx_patterns.h"
#include "block_graphics.h"

using namespace tempura;
using namespace tempura::block_gfx;

// Terminal raw mode handling
struct RawMode {
  termios original_;

  RawMode() {
    tcgetattr(STDIN_FILENO, &original_);
    termios raw = original_;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  }

  ~RawMode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_); }
};

// Read a single keypress (handles arrow keys)
auto readKey() -> int {
  char c;
  if (read(STDIN_FILENO, &c, 1) != 1) return -1;

  if (c == '\x1b') {
    char seq[2];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      switch (seq[1]) {
        case 'A': return 'k';  // Up
        case 'B': return 'j';  // Down
        case 'C': return 'l';  // Right
        case 'D': return 'h';  // Left
      }
    }
    return '\x1b';
  }
  return c;
}

// Character info for display
struct DebugCharInfo {
  uint32_t codepoint;
  std::string utf8;
  std::string name;
  std::string category;
  FillPattern pattern;
};

// Get category name and character info
auto buildCharacterList() -> std::vector<DebugCharInfo> {
  std::vector<DebugCharInfo> chars;

  for (const auto& ch : getCharacterLibrary()) {
    DebugCharInfo info;
    info.utf8 = ch.utf8;
    info.pattern = ch.pattern;

    // Decode UTF-8 to codepoint
    const auto* utf8 = reinterpret_cast<const unsigned char*>(ch.utf8);
    if ((utf8[0] & 0x80) == 0) {
      info.codepoint = utf8[0];
    } else if ((utf8[0] & 0xE0) == 0xC0) {
      info.codepoint = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
    } else if ((utf8[0] & 0xF0) == 0xE0) {
      info.codepoint = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) |
                       (utf8[2] & 0x3F);
    } else if ((utf8[0] & 0xF8) == 0xF0) {
      info.codepoint = ((utf8[0] & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) |
                       ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
    }

    // Categorize (skip Space, Blocks, and Other categories)
    uint32_t cp = info.codepoint;
    if (cp == ' ') {
      continue;  // Skip space
    } else if (cp >= 0x2580 && cp <= 0x259F) {
      continue;  // Skip blocks (cause vertical striping)
    } else if (cp >= kFirstSextant && cp <= kLastSextant) {
      info.category = "Sextants";
      info.name = "SEXTANT";
    } else if (cp >= kFirstSmoothMosaic && cp <= kLastSmoothMosaic) {
      info.category = "Smooth Mosaics";
      info.name = "SMOOTH MOSAIC";
    } else if (cp >= kFirstOctant && cp <= kLastOctant) {
      info.category = "Octants";
      info.name = "OCTANT";
    } else if (cp >= kFirstSixteenth && cp <= kLastSixteenth) {
      info.category = "Sixteenths";
      info.name = "SIXTEENTH BLOCK";
    } else if (cp >= 0x2800 && cp <= 0x28FF) {
      info.category = "Braille";
      info.name = "BRAILLE";
    } else {
      // Skip "Other" category
      continue;
    }

    chars.push_back(info);
  }

  return chars;
}

// Clear screen and move cursor to top
void clearScreen() { std::print("\x1b[2J\x1b[H"); }

// Display character info
void displayChar(const std::vector<DebugCharInfo>& chars, size_t idx,
                 size_t total_in_category, size_t category_start) {
  clearScreen();

  const auto& ch = chars[idx];

  std::println("╔══════════════════════════════════════════════════════════════════════╗");
  std::println("║  Pattern Debugger - Review Unicode Block Characters                  ║");
  std::println("╠══════════════════════════════════════════════════════════════════════╣");
  std::println("║  j/↓:next  k/↑:prev  J/K:±10  g/G:first/last  1-4:category  q:quit   ║");
  std::println("╚══════════════════════════════════════════════════════════════════════╝\n");

  std::println("Character {} of {} (category: {} - {} of {})\n",
               idx + 1, chars.size(), ch.category,
               idx - category_start + 1, total_in_category);

  // Vertical layout - avoids alignment issues with variable-width Unicode chars

  // Section 1: Single character (no box - just the char with clear labeling)
  std::println("  Glyph:  {}", ch.utf8);
  std::println("");

  // Section 2: Tessellation (8×4 grid, chars only - no mixed borders)
  std::println("  Tessellation (8×4):");
  std::println("  {}{}{}{}{}{}{}{}", ch.utf8, ch.utf8, ch.utf8, ch.utf8,
               ch.utf8, ch.utf8, ch.utf8, ch.utf8);
  std::println("  {}{}{}{}{}{}{}{}", ch.utf8, ch.utf8, ch.utf8, ch.utf8,
               ch.utf8, ch.utf8, ch.utf8, ch.utf8);
  std::println("  {}{}{}{}{}{}{}{}", ch.utf8, ch.utf8, ch.utf8, ch.utf8,
               ch.utf8, ch.utf8, ch.utf8, ch.utf8);
  std::println("  {}{}{}{}{}{}{}{}", ch.utf8, ch.utf8, ch.utf8, ch.utf8,
               ch.utf8, ch.utf8, ch.utf8, ch.utf8);
  std::println("");

  // Section 3: Pattern visualization (actual subpixel grid)
  std::println("  Pattern ({}×{} subpixels):", kSubpixelWidth, kSubpixelHeight);
  std::println("  ┌────────────────┐");
  for (int row = 0; row < kSubpixelHeight; ++row) {
    std::string pattern_row;
    for (int col = 0; col < kSubpixelWidth; ++col) {
      bool filled = ch.pattern.test(subpixelIndex(col, row));
      pattern_row += filled ? "██" : "··";
    }
    std::println("  │{}│", pattern_row);
  }
  std::println("  └────────────────┘");
  std::println("");

  // Character details
  std::println("  Codepoint:  U+{:04X} (decimal: {})", ch.codepoint, ch.codepoint);
  std::println("  UTF-8:      {:02X} {:02X} {:02X} {:02X}",
               static_cast<unsigned char>(ch.utf8[0]),
               ch.utf8.size() > 1 ? static_cast<unsigned char>(ch.utf8[1]) : 0,
               ch.utf8.size() > 2 ? static_cast<unsigned char>(ch.utf8[2]) : 0,
               ch.utf8.size() > 3 ? static_cast<unsigned char>(ch.utf8[3]) : 0);
  std::println("  Category:   {}", ch.category);
  std::println("  Name:       {}", ch.name);

  // Pattern statistics
  size_t filled = ch.pattern.count();
  size_t total = kSubpixelWidth * kSubpixelHeight;
  double coverage = 100.0 * filled / total;
  std::println("  Coverage:   {:.1f}% ({}/{} subpixels)", coverage, filled, total);
}

int main() {
  auto chars = buildCharacterList();
  if (chars.empty()) {
    std::println("No characters in library!");
    return 1;
  }

  // Sort by codepoint for easier navigation
  std::sort(chars.begin(), chars.end(),
            [](const auto& a, const auto& b) { return a.codepoint < b.codepoint; });

  // Build category indices
  struct CategoryInfo {
    std::string name;
    size_t start;
    size_t count;
  };
  std::vector<CategoryInfo> categories;

  std::string current_cat;
  for (size_t i = 0; i < chars.size(); ++i) {
    if (chars[i].category != current_cat) {
      if (!current_cat.empty()) {
        categories.back().count = i - categories.back().start;
      }
      current_cat = chars[i].category;
      categories.push_back({current_cat, i, 0});
    }
  }
  if (!categories.empty()) {
    categories.back().count = chars.size() - categories.back().start;
  }

  size_t idx = 0;
  RawMode raw;

  // Find category info for current index
  auto getCategoryInfo = [&](size_t i) -> std::pair<size_t, size_t> {
    for (const auto& cat : categories) {
      if (i >= cat.start && i < cat.start + cat.count) {
        return {cat.count, cat.start};
      }
    }
    return {chars.size(), 0};
  };

  auto [cat_count, cat_start] = getCategoryInfo(idx);
  displayChar(chars, idx, cat_count, cat_start);

  while (true) {
    int key = readKey();

    switch (key) {
      case 'q':
      case 'Q':
        clearScreen();
        std::println("Goodbye!");
        return 0;

      case 'j':  // Next
        if (idx < chars.size() - 1) ++idx;
        break;

      case 'k':  // Previous
        if (idx > 0) --idx;
        break;

      case 'J':  // Forward 10
        idx = std::min(idx + 10, chars.size() - 1);
        break;

      case 'K':  // Back 10
        idx = idx > 10 ? idx - 10 : 0;
        break;

      case 'g':  // First
        idx = 0;
        break;

      case 'G':  // Last
        idx = chars.size() - 1;
        break;

      case '1':  // Sextants
      case '2':  // Mosaics
      case '3':  // Octants
      case '4':  // Sixteenths
      {
        const char* targets[] = {"Sextants", "Smooth Mosaics",
                                  "Octants", "Sixteenths"};
        int target_idx = key - '1';
        if (target_idx >= 0 && target_idx < 4) {
          for (const auto& cat : categories) {
            if (cat.name == targets[target_idx]) {
              idx = cat.start;
              break;
            }
          }
        }
        break;
      }

      case '/':  // Search by codepoint
      {
        // Exit raw mode temporarily for input
        std::print("\n  Enter codepoint (hex, e.g., 2588): ");
        std::fflush(stdout);

        // Temporarily restore normal terminal mode
        termios original;
        tcgetattr(STDIN_FILENO, &original);
        termios normal = original;
        normal.c_lflag |= (ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &normal);

        char buf[32];
        if (std::fgets(buf, sizeof(buf), stdin)) {
          uint32_t cp = 0;
          if (std::sscanf(buf, "%x", &cp) == 1) {
            for (size_t i = 0; i < chars.size(); ++i) {
              if (chars[i].codepoint == cp) {
                idx = i;
                break;
              }
            }
          }
        }

        // Restore raw mode
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        break;
      }

      default:
        continue;  // Unknown key, don't redraw
    }

    auto [cc, cs] = getCategoryInfo(idx);
    displayChar(chars, idx, cc, cs);
  }

  return 0;
}
