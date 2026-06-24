// Glyph gallery — verify the mosaic repertoire visually. For every character in
// the library, shows the glyph next to a Braille thumbnail of its programmatic
// 8×24 fill pattern, so you can (a) check your font actually renders it (no tofu)
// and (b) see whether the wedge/diagonal patterns step smoothly. Build with
// -DTEMPURA_BUILD_DEMOS=ON, run ./glyphs_demo (pipe to `less -R` to scroll).

#include <array>
#include <print>
#include <string>
#include <vector>

#include "block_graphics.h"

using namespace tempura;

namespace {

// Render an 8×24 fill pattern as a 4×6 grid of Braille cells (2×4 dots each),
// reusing plot.h's octant table — a compact, faithful thumbnail of the pattern.
auto patternThumbnail(const FillPattern& p) -> std::array<std::string, 6> {
  std::array<std::string, 6> rows;
  for (int br = 0; br < 6; ++br) {
    std::string line;
    for (int bc = 0; bc < 4; ++bc) {
      int octant = 0;
      for (int dy = 0; dy < 4; ++dy) {
        if (p[static_cast<size_t>(subpixelIndex(bc * 2, br * 4 + dy))]) octant |= 1 << dy;
        if (p[static_cast<size_t>(subpixelIndex(bc * 2 + 1, br * 4 + dy))]) octant |= 1 << (dy + 4);
      }
      line += kOctant[static_cast<size_t>(octant)].bytes;
    }
    rows[static_cast<size_t>(br)] = line;
  }
  return rows;
}

}  // namespace

auto main() -> int {
  const auto& lib = getCharacterLibrary();
  std::println("Mosaic glyph gallery — {} glyphs. Each shows the character above a", lib.size());
  std::println("Braille thumbnail of its 8×24 fill pattern. Tofu (□) = your font lacks it.\n");

  constexpr int kPerRow = 12;
  for (size_t start = 0; start < lib.size(); start += kPerRow) {
    const size_t end = std::min(start + kPerRow, lib.size());

    // Index + glyph header, each centered over its 4-wide thumbnail (+1 gutter).
    std::string idx_line, glyph_line;
    for (size_t i = start; i < end; ++i) {
      idx_line += std::format("{:<5}", i);
      glyph_line += std::format(" {}   ", lib[i].utf8);
    }
    std::println("{}", idx_line);
    std::println("\033[1m{}\033[0m", glyph_line);

    // Six Braille rows of the thumbnails, side by side.
    std::vector<std::array<std::string, 6>> thumbs;
    for (size_t i = start; i < end; ++i) thumbs.push_back(patternThumbnail(lib[i].pattern));
    for (int br = 0; br < 6; ++br) {
      std::string line;
      for (const auto& t : thumbs) line += t[static_cast<size_t>(br)] + " ";
      std::println("{}", line);
    }
    std::println("");
  }
  return 0;
}
