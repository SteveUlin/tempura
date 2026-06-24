#pragma once

// Block Graphics Patterns - Based on Ghostty's sprite rendering
// https://github.com/ghostty-org/ghostty/blob/main/src/font/sprite/draw/symbols_for_legacy_computing.zig
// https://github.com/ghostty-org/ghostty/blob/main/src/font/sprite/draw/symbols_for_legacy_computing_supplement.zig

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>

namespace tempura {

constexpr int kSubpixelWidth = 8;
constexpr int kSubpixelHeight = 24;  // LCM(3,4,8) for sextants, octants, sixteenths, eighths
constexpr int kTotalSubpixels = kSubpixelWidth * kSubpixelHeight;  // 192

using FillPattern = std::bitset<kTotalSubpixels>;

constexpr auto subpixelIndex(int x, int y) -> int {
  return y * kSubpixelWidth + x;
}

// Character definition - used by scorer and renderer
struct BlockChar {
  const char* utf8;        // UTF-8 encoded character
  FillPattern pattern;     // Which subpixels are filled
  double fill_fraction;    // Fraction of cell filled (cached)
};

// =============================================================================
// Helper: Fill rectangular region
// =============================================================================
inline auto fillRect(double x0, double y0, double x1, double y1) -> FillPattern {
  FillPattern pattern;
  for (int y = 0; y < kSubpixelHeight; ++y) {
    double py = static_cast<double>(y) / kSubpixelHeight;
    for (int x = 0; x < kSubpixelWidth; ++x) {
      double px = static_cast<double>(x) / kSubpixelWidth;
      if (px >= x0 && px < x1 && py >= y0 && py < y1) {
        pattern.set(subpixelIndex(x, y));
      }
    }
  }
  return pattern;
}

// =============================================================================
// Helper: Point-in-polygon test for arbitrary polygons
// =============================================================================
struct Vertex {
  double x, y;
};

inline auto fillPolygon(const Vertex* verts, int n) -> FillPattern {
  if (n < 3) return FillPattern{};

  FillPattern pattern;
  for (int y = 0; y < kSubpixelHeight; ++y) {
    double py = (y + 0.5) / kSubpixelHeight;
    for (int x = 0; x < kSubpixelWidth; ++x) {
      double px = (x + 0.5) / kSubpixelWidth;

      // Ray casting algorithm
      bool inside = false;
      for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = verts[i].x, yi = verts[i].y;
        double xj = verts[j].x, yj = verts[j].y;

        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
          inside = !inside;
        }
      }

      if (inside) pattern.set(subpixelIndex(x, y));
    }
  }
  return pattern;
}

// =============================================================================
// Helper: Fill circle or arc
// =============================================================================
inline auto fillCircle(double cx, double cy, double r, bool filled = true)
    -> FillPattern {
  FillPattern pattern;
  for (int y = 0; y < kSubpixelHeight; ++y) {
    double py = (y + 0.5) / kSubpixelHeight;
    for (int x = 0; x < kSubpixelWidth; ++x) {
      double px = (x + 0.5) / kSubpixelWidth;
      double dx = px - cx;
      double dy = py - cy;
      double dist = std::sqrt(dx * dx + dy * dy);
      if (filled) {
        if (dist <= r) pattern.set(subpixelIndex(x, y));
      } else {
        // Outline only - thickness ~1 subpixel
        double thickness = 0.5 / kSubpixelWidth;
        if (std::abs(dist - r) <= thickness) pattern.set(subpixelIndex(x, y));
      }
    }
  }
  return pattern;
}

// =============================================================================
// Helper: Diagonal stripe fill
// =============================================================================
inline auto fillDiagonalStripes(bool upper_left_to_lower_right) -> FillPattern {
  FillPattern pattern;
  // Create alternating diagonal stripes
  for (int y = 0; y < kSubpixelHeight; ++y) {
    for (int x = 0; x < kSubpixelWidth; ++x) {
      int diag = upper_left_to_lower_right ? (x + y) : (x - y + kSubpixelHeight);
      if ((diag / 2) % 2 == 0) {
        pattern.set(subpixelIndex(x, y));
      }
    }
  }
  return pattern;
}

// =============================================================================
// Helper: Checkerboard pattern
// =============================================================================
inline auto fillCheckerboard(int cell_size = 2) -> FillPattern {
  FillPattern pattern;
  for (int y = 0; y < kSubpixelHeight; ++y) {
    for (int x = 0; x < kSubpixelWidth; ++x) {
      if (((x / cell_size) + (y / cell_size)) % 2 == 0) {
        pattern.set(subpixelIndex(x, y));
      }
    }
  }
  return pattern;
}

// =============================================================================
// Helper: Horizontal stripes
// =============================================================================
inline auto fillHorizontalStripes(int stripe_height = 2) -> FillPattern {
  FillPattern pattern;
  for (int y = 0; y < kSubpixelHeight; ++y) {
    if ((y / stripe_height) % 2 == 0) {
      for (int x = 0; x < kSubpixelWidth; ++x) {
        pattern.set(subpixelIndex(x, y));
      }
    }
  }
  return pattern;
}

// =============================================================================
// Smooth Mosaic Patterns (U+1FB3C to U+1FB67)
// =============================================================================
// Smooth Mosaic Patterns - Generated from Ghostty source
// Format: 12-char perimeter encoding mapping to:
//   [0]   [1]   [2]     <- top (y=0)
//   [3]         [5]     <- y=1/3
//   [6]         [8]     <- y=2/3
//   [9]  [10]  [11]     <- bottom (y=1)
inline constexpr const char* kMosaicPatterns[] = {
    "......#..##.",  // 0x1FB3C 🬼
    "......#..###",  // 0x1FB3D 🬽
    "...#..#..##.",  // 0x1FB3E 🬾
    "...#..#..###",  // 0x1FB3F 🬿
    "#..#..#..##.",  // 0x1FB40 🭀
    ".###.##.####",  // 0x1FB41 🭁
    "..##.##.####",  // 0x1FB42 🭂
    ".##..##.####",  // 0x1FB43 🭃
    "..#..##.####",  // 0x1FB44 🭄
    ".##..#..####",  // 0x1FB45 🭅
    ".....##.####",  // 0x1FB46 🭆
    "........#.##",  // 0x1FB47 🭇
    "........####",  // 0x1FB48 🭈
    ".....#..#.##",  // 0x1FB49 🭉
    ".....#..####",  // 0x1FB4A 🭊
    "..#..#..#.##",  // 0x1FB4B 🭋
    "##.#.##.####",  // 0x1FB4C 🭌
    "#..#.##.####",  // 0x1FB4D 🭍
    "##.#..#.####",  // 0x1FB4E 🭎
    "#..#..#.####",  // 0x1FB4F 🭏
    "##.#..#..###",  // 0x1FB50 🭐
    "...#..#.####",  // 0x1FB51 🭑
    "####.##.#.##",  // 0x1FB52 🭒
    "####.##.#..#",  // 0x1FB53 🭓
    "####.#..#.##",  // 0x1FB54 🭔
    "####.#..#..#",  // 0x1FB55 🭕
    "###..#..#.##",  // 0x1FB56 🭖
    "##.#........",  // 0x1FB57 🭗
    "####........",  // 0x1FB58 🭘
    "##.#..#.....",  // 0x1FB59 🭙
    "####..#.....",  // 0x1FB5A 🭚
    "##.#..#..#..",  // 0x1FB5B 🭛
    "####.##.....",  // 0x1FB5C 🭜
    "####.##.###.",  // 0x1FB5D 🭝
    "####.##.##..",  // 0x1FB5E 🭞
    "####.##..##.",  // 0x1FB5F 🭟
    "####.##..#..",  // 0x1FB60 🭠
    "####..#..##.",  // 0x1FB61 🭡
    ".##..#......",  // 0x1FB62 🭢
    "###..#......",  // 0x1FB63 🭣
    ".##..#..#...",  // 0x1FB64 🭤
    "###..#..#...",  // 0x1FB65 🭥
    ".##..#..#..#",  // 0x1FB66 🭦
    "####.#..#...",  // 0x1FB67 🭧
};

inline auto patternStringToFill(const char* p) -> FillPattern {
  Vertex verts[12];
  int n = 0;

  // Clockwise traversal of potential vertices
  if (p[0] == '#') verts[n++] = {0.0, 0.0};
  if (p[3] == '#' && (p[0] != '#' || p[6] != '#')) verts[n++] = {0.0, 1.0 / 3.0};
  if (p[6] == '#' && (p[3] != '#' || p[9] != '#')) verts[n++] = {0.0, 2.0 / 3.0};
  if (p[9] == '#') verts[n++] = {0.0, 1.0};
  if (p[10] == '#' && (p[9] != '#' || p[11] != '#')) verts[n++] = {0.5, 1.0};
  if (p[11] == '#') verts[n++] = {1.0, 1.0};
  if (p[8] == '#' && (p[11] != '#' || p[5] != '#')) verts[n++] = {1.0, 2.0 / 3.0};
  if (p[5] == '#' && (p[8] != '#' || p[2] != '#')) verts[n++] = {1.0, 1.0 / 3.0};
  if (p[2] == '#') verts[n++] = {1.0, 0.0};
  if (p[1] == '#' && (p[2] != '#' || p[0] != '#')) verts[n++] = {0.5, 0.0};

  return fillPolygon(verts, n);
}

constexpr uint32_t kFirstSmoothMosaic = 0x1FB3C;
constexpr uint32_t kLastSmoothMosaic = 0x1FB67;

inline auto getSmoothMosaicPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstSmoothMosaic || codepoint > kLastSmoothMosaic) {
    return FillPattern{};
  }
  int idx = codepoint - kFirstSmoothMosaic;
  if (idx >= 0 && idx < static_cast<int>(std::size(kMosaicPatterns))) {
    return patternStringToFill(kMosaicPatterns[idx]);
  }
  return FillPattern{};
}

// =============================================================================
// Sextant Patterns (U+1FB00 to U+1FB3B) - 2×3 grid
// =============================================================================
constexpr uint32_t kFirstSextant = 0x1FB00;
constexpr uint32_t kLastSextant = 0x1FB3B;

inline auto getSextantBits(uint32_t codepoint) -> uint8_t {
  if (codepoint < kFirstSextant || codepoint > kLastSextant) return 0;
  uint32_t idx = codepoint - kFirstSextant;
  return static_cast<uint8_t>(idx + (idx / 0x14) + 1);
}

inline auto sextantBitsToPattern(uint8_t bits) -> FillPattern {
  FillPattern pattern;
  for (int y = 0; y < kSubpixelHeight; ++y) {
    double py = static_cast<double>(y) / kSubpixelHeight;
    for (int x = 0; x < kSubpixelWidth; ++x) {
      double px = static_cast<double>(x) / kSubpixelWidth;

      bool left = px < 0.5;
      bool top = py < (1.0 / 3.0);
      bool mid = py >= (1.0 / 3.0) && py < (2.0 / 3.0);
      bool bot = py >= (2.0 / 3.0);

      bool filled = false;
      if (left && top && (bits & 0x01)) filled = true;
      if (!left && top && (bits & 0x02)) filled = true;
      if (left && mid && (bits & 0x04)) filled = true;
      if (!left && mid && (bits & 0x08)) filled = true;
      if (left && bot && (bits & 0x10)) filled = true;
      if (!left && bot && (bits & 0x20)) filled = true;

      if (filled) pattern.set(subpixelIndex(x, y));
    }
  }
  return pattern;
}

inline auto getSextantPattern(uint32_t codepoint) -> FillPattern {
  return sextantBitsToPattern(getSextantBits(codepoint));
}

// =============================================================================
// Edge Triangles (U+1FB68 to U+1FB6F)
// =============================================================================
// Triangles from cell center to opposing edge
constexpr uint32_t kFirstEdgeTriangle = 0x1FB68;
constexpr uint32_t kLastEdgeTriangle = 0x1FB6F;

inline auto getEdgeTrianglePattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstEdgeTriangle || codepoint > kLastEdgeTriangle) {
    return FillPattern{};
  }

  // Center of cell
  constexpr double cx = 0.5, cy = 0.5;

  Vertex verts[3];
  switch (codepoint) {
    case 0x1FB68:  // 🭨 Left edge triangle (inverted = right half + left triangle)
      verts[0] = {cx, cy};
      verts[1] = {0.0, 0.0};
      verts[2] = {0.0, 1.0};
      break;
    case 0x1FB69:  // 🭩 Top edge triangle (inverted)
      verts[0] = {cx, cy};
      verts[1] = {0.0, 0.0};
      verts[2] = {1.0, 0.0};
      break;
    case 0x1FB6A:  // 🭪 Right edge triangle (inverted)
      verts[0] = {cx, cy};
      verts[1] = {1.0, 0.0};
      verts[2] = {1.0, 1.0};
      break;
    case 0x1FB6B:  // 🭫 Bottom edge triangle (inverted)
      verts[0] = {cx, cy};
      verts[1] = {0.0, 1.0};
      verts[2] = {1.0, 1.0};
      break;
    case 0x1FB6C:  // 🭬 Left edge triangle
      verts[0] = {cx, cy};
      verts[1] = {0.0, 0.0};
      verts[2] = {0.0, 1.0};
      return fillPolygon(verts, 3);
    case 0x1FB6D:  // 🭭 Top edge triangle
      verts[0] = {cx, cy};
      verts[1] = {0.0, 0.0};
      verts[2] = {1.0, 0.0};
      return fillPolygon(verts, 3);
    case 0x1FB6E:  // 🭮 Right edge triangle
      verts[0] = {cx, cy};
      verts[1] = {1.0, 0.0};
      verts[2] = {1.0, 1.0};
      return fillPolygon(verts, 3);
    case 0x1FB6F:  // 🭯 Bottom edge triangle
      verts[0] = {cx, cy};
      verts[1] = {0.0, 1.0};
      verts[2] = {1.0, 1.0};
      return fillPolygon(verts, 3);
    default:
      return FillPattern{};
  }

  // For inverted (0x1FB68-0x1FB6B): fill everything EXCEPT the triangle
  FillPattern tri = fillPolygon(verts, 3);
  return ~tri;
}

// =============================================================================
// Eighth Blocks (U+1FB70 to U+1FB7B)
// =============================================================================
constexpr uint32_t kFirstEighthBlock = 0x1FB70;
constexpr uint32_t kLastEighthBlock = 0x1FB7B;

inline auto getEighthBlockPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstEighthBlock || codepoint > kLastEighthBlock) {
    return FillPattern{};
  }

  // Vertical eighth blocks: U+1FB70-U+1FB75 (left 1/8 to left 6/8)
  // Horizontal eighth blocks: U+1FB76-U+1FB7B (upper 1/8 to upper 6/8)
  int idx = codepoint - kFirstEighthBlock;

  if (idx <= 5) {
    // Vertical: left N/8
    double right = (idx + 1) / 8.0;
    return fillRect(0.0, 0.0, right, 1.0);
  } else {
    // Horizontal: upper N/8
    int n = idx - 5;  // 1 to 6
    double bottom = n / 8.0;
    return fillRect(0.0, 0.0, 1.0, bottom);
  }
}

// =============================================================================
// Combined Blocks (U+1FB7C to U+1FB97)
// =============================================================================
constexpr uint32_t kFirstCombinedBlock = 0x1FB7C;
constexpr uint32_t kLastCombinedBlock = 0x1FB97;

inline auto getCombinedBlockPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstCombinedBlock || codepoint > kLastCombinedBlock) {
    return FillPattern{};
  }

  switch (codepoint) {
    // Left and lower pieces
    case 0x1FB7C:
      return fillRect(0.0, 0.0, 0.125, 1.0) | fillRect(0.0, 0.875, 1.0, 1.0);
    case 0x1FB7D:
      return fillRect(0.0, 0.0, 0.125, 1.0) | fillRect(0.0, 0.75, 1.0, 1.0);
    case 0x1FB7E:
      return fillRect(0.875, 0.0, 1.0, 1.0) | fillRect(0.0, 0.875, 1.0, 1.0);
    case 0x1FB7F:
      return fillRect(0.875, 0.0, 1.0, 1.0) | fillRect(0.0, 0.75, 1.0, 1.0);

    // Upper pieces
    case 0x1FB80:
      return fillRect(0.0, 0.0, 1.0, 0.125) | fillRect(0.0, 0.875, 1.0, 1.0);
    case 0x1FB81:
      return fillRect(0.0, 0.0, 1.0, 0.125) | fillRect(0.0, 0.375, 1.0, 0.625) |
             fillRect(0.0, 0.875, 1.0, 1.0);

    // Horizontal multi-eighth
    case 0x1FB82:
      return fillRect(0.0, 0.0, 1.0, 0.25);  // Upper 1/4
    case 0x1FB83:
      return fillRect(0.0, 0.0, 1.0, 0.375);  // Upper 3/8
    case 0x1FB84:
      return fillRect(0.0, 0.0, 1.0, 0.625);  // Upper 5/8
    case 0x1FB85:
      return fillRect(0.0, 0.0, 1.0, 0.75);  // Upper 3/4
    case 0x1FB86:
      return fillRect(0.0, 0.0, 1.0, 0.875);  // Upper 7/8

    // Vertical multi-eighth
    case 0x1FB87:
      return fillRect(0.75, 0.0, 1.0, 1.0);  // Right 1/4
    case 0x1FB88:
      return fillRect(0.625, 0.0, 1.0, 1.0);  // Right 3/8
    case 0x1FB89:
      return fillRect(0.375, 0.0, 1.0, 1.0);  // Right 5/8
    case 0x1FB8A:
      return fillRect(0.25, 0.0, 1.0, 1.0);  // Right 3/4
    case 0x1FB8B:
      return fillRect(0.125, 0.0, 1.0, 1.0);  // Right 7/8

    // Shade/checkerboard patterns (0x1FB8C-0x1FB97) intentionally omitted -
    // these create dithering artifacts that don't represent actual density

    default:
      return FillPattern{};
  }
}

// =============================================================================
// Diagonal Fills (U+1FB98 to U+1FB99)
// =============================================================================
constexpr uint32_t kFirstDiagonalFill = 0x1FB98;
constexpr uint32_t kLastDiagonalFill = 0x1FB99;

inline auto getDiagonalFillPattern(uint32_t codepoint) -> FillPattern {
  switch (codepoint) {
    case 0x1FB98:
      return fillDiagonalStripes(true);   // Upper-left to lower-right
    case 0x1FB99:
      return fillDiagonalStripes(false);  // Upper-right to lower-left
    default:
      return FillPattern{};
  }
}

// =============================================================================
// Triangle & Shade Combinations (U+1FB9A to U+1FB9F)
// =============================================================================
constexpr uint32_t kFirstTriangleShade = 0x1FB9A;
constexpr uint32_t kLastTriangleShade = 0x1FB9F;

inline auto getTriangleShadePattern(uint32_t codepoint) -> FillPattern {
  Vertex top_tri[] = {{0.5, 0.5}, {0.0, 0.0}, {1.0, 0.0}};
  Vertex bot_tri[] = {{0.5, 0.5}, {0.0, 1.0}, {1.0, 1.0}};
  Vertex left_tri[] = {{0.5, 0.5}, {0.0, 0.0}, {0.0, 1.0}};
  Vertex right_tri[] = {{0.5, 0.5}, {1.0, 0.0}, {1.0, 1.0}};

  switch (codepoint) {
    case 0x1FB9A:  // Upper and lower triangles
      return fillPolygon(top_tri, 3) | fillPolygon(bot_tri, 3);
    case 0x1FB9B:  // Left and right triangles
      return fillPolygon(left_tri, 3) | fillPolygon(right_tri, 3);
    case 0x1FB9C:  // Upper-left triangle with shade
      {
        Vertex ul[] = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
        return fillPolygon(ul, 3) & fillCheckerboard(1);
      }
    case 0x1FB9D:  // Upper-right triangle with shade
      {
        Vertex ur[] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}};
        return fillPolygon(ur, 3) & fillCheckerboard(1);
      }
    case 0x1FB9E:  // Lower-right triangle with shade
      {
        Vertex lr[] = {{1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
        return fillPolygon(lr, 3) & fillCheckerboard(1);
      }
    case 0x1FB9F:  // Lower-left triangle with shade
      {
        Vertex ll[] = {{0.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
        return fillPolygon(ll, 3) & fillCheckerboard(1);
      }
    default:
      return FillPattern{};
  }
}

// =============================================================================
// Corner Diagonal Lines (U+1FBA0 to U+1FBAE)
// =============================================================================
constexpr uint32_t kFirstCornerDiagonal = 0x1FBA0;
constexpr uint32_t kLastCornerDiagonal = 0x1FBAE;

inline auto drawLine(double x0, double y0, double x1, double y1,
                     double thickness = 0.08) -> FillPattern {
  FillPattern pattern;
  double dx = x1 - x0, dy = y1 - y0;
  double len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.001) return pattern;

  for (int y = 0; y < kSubpixelHeight; ++y) {
    double py = (y + 0.5) / kSubpixelHeight;
    for (int x = 0; x < kSubpixelWidth; ++x) {
      double px = (x + 0.5) / kSubpixelWidth;

      // Distance from point to line segment
      double t = std::clamp(((px - x0) * dx + (py - y0) * dy) / (len * len), 0.0, 1.0);
      double proj_x = x0 + t * dx;
      double proj_y = y0 + t * dy;
      double dist = std::sqrt((px - proj_x) * (px - proj_x) + (py - proj_y) * (py - proj_y));

      if (dist <= thickness) {
        pattern.set(subpixelIndex(x, y));
      }
    }
  }
  return pattern;
}

inline auto getCornerDiagonalPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstCornerDiagonal || codepoint > kLastCornerDiagonal) {
    return FillPattern{};
  }

  // Center
  constexpr double cx = 0.5, cy = 0.5;

  // Corners: TL, TR, BL, BR
  constexpr double corners[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};

  // Each codepoint has different corner combinations
  // Bits: TL=1, TR=2, BL=4, BR=8
  static constexpr uint8_t corner_bits[] = {
      0x05,  // 0x1FBA0: TL + BL
      0x0A,  // 0x1FBA1: TR + BR
      0x03,  // 0x1FBA2: TL + TR
      0x0C,  // 0x1FBA3: BL + BR
      0x09,  // 0x1FBA4: TL + BR
      0x06,  // 0x1FBA5: TR + BL
      0x07,  // 0x1FBA6: TL + TR + BL
      0x0B,  // 0x1FBA7: TL + TR + BR
      0x0D,  // 0x1FBA8: TL + BL + BR
      0x0E,  // 0x1FBA9: TR + BL + BR
      0x01,  // 0x1FBAA: TL only
      0x02,  // 0x1FBAB: TR only
      0x04,  // 0x1FBAC: BL only
      0x08,  // 0x1FBAD: BR only
      0x0F,  // 0x1FBAE: All corners
  };

  int idx = codepoint - kFirstCornerDiagonal;
  uint8_t bits = corner_bits[idx];

  FillPattern pattern;
  for (int i = 0; i < 4; ++i) {
    if (bits & (1 << i)) {
      pattern |= drawLine(cx, cy, corners[i][0], corners[i][1]);
    }
  }
  return pattern;
}

// =============================================================================
// Cell Diagonals (U+1FBD0 to U+1FBDF)
// =============================================================================
constexpr uint32_t kFirstCellDiagonal = 0x1FBD0;
constexpr uint32_t kLastCellDiagonal = 0x1FBDF;

inline auto getCellDiagonalPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstCellDiagonal || codepoint > kLastCellDiagonal) {
    return FillPattern{};
  }

  // Cell positions: left/center/right × top/middle/bottom
  // Each diagonal connects two positions
  struct Pos {
    double x, y;
  };
  constexpr Pos left_top = {0.0, 0.0};
  constexpr Pos center_top = {0.5, 0.0};
  constexpr Pos right_top = {1.0, 0.0};
  constexpr Pos left_mid = {0.0, 0.5};
  constexpr Pos center_mid = {0.5, 0.5};
  constexpr Pos right_mid = {1.0, 0.5};
  constexpr Pos left_bot = {0.0, 1.0};
  constexpr Pos center_bot = {0.5, 1.0};
  constexpr Pos right_bot = {1.0, 1.0};

  // Define each diagonal line
  switch (codepoint) {
    case 0x1FBD0:
      return drawLine(left_top.x, left_top.y, center_mid.x, center_mid.y);
    case 0x1FBD1:
      return drawLine(center_top.x, center_top.y, left_mid.x, left_mid.y);
    case 0x1FBD2:
      return drawLine(center_top.x, center_top.y, right_mid.x, right_mid.y);
    case 0x1FBD3:
      return drawLine(right_top.x, right_top.y, center_mid.x, center_mid.y);
    case 0x1FBD4:
      return drawLine(left_mid.x, left_mid.y, center_bot.x, center_bot.y);
    case 0x1FBD5:
      return drawLine(center_mid.x, center_mid.y, left_bot.x, left_bot.y);
    case 0x1FBD6:
      return drawLine(center_mid.x, center_mid.y, right_bot.x, right_bot.y);
    case 0x1FBD7:
      return drawLine(right_mid.x, right_mid.y, center_bot.x, center_bot.y);
    case 0x1FBD8:
      return drawLine(left_top.x, left_top.y, center_mid.x, center_mid.y) |
             drawLine(center_mid.x, center_mid.y, right_top.x, right_top.y);
    case 0x1FBD9:
      return drawLine(left_bot.x, left_bot.y, center_mid.x, center_mid.y) |
             drawLine(center_mid.x, center_mid.y, right_bot.x, right_bot.y);
    case 0x1FBDA:
      return drawLine(left_top.x, left_top.y, center_mid.x, center_mid.y) |
             drawLine(center_mid.x, center_mid.y, left_bot.x, left_bot.y);
    case 0x1FBDB:
      return drawLine(right_top.x, right_top.y, center_mid.x, center_mid.y) |
             drawLine(center_mid.x, center_mid.y, right_bot.x, right_bot.y);
    case 0x1FBDC:
      return drawLine(center_top.x, center_top.y, left_mid.x, left_mid.y) |
             drawLine(left_mid.x, left_mid.y, center_bot.x, center_bot.y);
    case 0x1FBDD:
      return drawLine(center_top.x, center_top.y, right_mid.x, right_mid.y) |
             drawLine(right_mid.x, right_mid.y, center_bot.x, center_bot.y);
    case 0x1FBDE:
      return drawLine(left_top.x, left_top.y, right_mid.x, right_mid.y) |
             drawLine(right_mid.x, right_mid.y, left_bot.x, left_bot.y);
    case 0x1FBDF:
      return drawLine(right_top.x, right_top.y, left_mid.x, left_mid.y) |
             drawLine(left_mid.x, left_mid.y, right_bot.x, right_bot.y);
    default:
      return FillPattern{};
  }
}

// =============================================================================
// Circles & Circle Segments (U+1FBE0 to U+1FBEF)
// =============================================================================
constexpr uint32_t kFirstCircle = 0x1FBE0;
constexpr uint32_t kLastCircle = 0x1FBEF;

inline auto getCirclePattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstCircle || codepoint > kLastCircle) {
    return FillPattern{};
  }

  // Radius is half the minimum dimension (accounting for aspect ratio)
  constexpr double r = 0.4;

  switch (codepoint) {
    // Filled circles at corners/edges/center
    case 0x1FBE0:
      return fillCircle(0.25, 0.25, r * 0.5, true);  // Upper-left quadrant
    case 0x1FBE1:
      return fillCircle(0.75, 0.25, r * 0.5, true);  // Upper-right quadrant
    case 0x1FBE2:
      return fillCircle(0.25, 0.75, r * 0.5, true);  // Lower-left quadrant
    case 0x1FBE3:
      return fillCircle(0.75, 0.75, r * 0.5, true);  // Lower-right quadrant
    case 0x1FBE4:
      return fillCircle(0.5, 0.5, r, true);  // Center filled

    // Outlined circles
    case 0x1FBE5:
      return fillCircle(0.25, 0.25, r * 0.5, false);
    case 0x1FBE6:
      return fillCircle(0.75, 0.25, r * 0.5, false);
    case 0x1FBE7:
      return fillCircle(0.25, 0.75, r * 0.5, false);
    case 0x1FBE8:
      return fillCircle(0.75, 0.75, r * 0.5, false);
    case 0x1FBE9:
      return fillCircle(0.5, 0.5, r, false);

    // Half circles (filled)
    case 0x1FBEA:
      return fillCircle(0.5, 0.0, r, true);  // Top half
    case 0x1FBEB:
      return fillCircle(0.5, 1.0, r, true);  // Bottom half
    case 0x1FBEC:
      return fillCircle(0.0, 0.5, r, true);  // Left half
    case 0x1FBED:
      return fillCircle(1.0, 0.5, r, true);  // Right half

    // Combined circles
    case 0x1FBEE:
      return fillCircle(0.25, 0.25, r * 0.5, true) |
             fillCircle(0.75, 0.75, r * 0.5, true);
    case 0x1FBEF:
      return fillCircle(0.75, 0.25, r * 0.5, true) |
             fillCircle(0.25, 0.75, r * 0.5, true);

    default:
      return FillPattern{};
  }
}

// =============================================================================
// Block Elements (U+2580 to U+259F)
// =============================================================================
constexpr uint32_t kFirstBlockElement = 0x2580;
constexpr uint32_t kLastBlockElement = 0x259F;

inline auto getBlockElementPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstBlockElement || codepoint > kLastBlockElement) {
    return FillPattern{};
  }

  switch (codepoint) {
    case 0x2580:
      return fillRect(0.0, 0.0, 1.0, 0.5);  // Upper half
    case 0x2581:
      return fillRect(0.0, 0.875, 1.0, 1.0);  // Lower 1/8
    case 0x2582:
      return fillRect(0.0, 0.75, 1.0, 1.0);  // Lower 1/4
    case 0x2583:
      return fillRect(0.0, 0.625, 1.0, 1.0);  // Lower 3/8
    case 0x2584:
      return fillRect(0.0, 0.5, 1.0, 1.0);  // Lower half
    case 0x2585:
      return fillRect(0.0, 0.375, 1.0, 1.0);  // Lower 5/8
    case 0x2586:
      return fillRect(0.0, 0.25, 1.0, 1.0);  // Lower 3/4
    case 0x2587:
      return fillRect(0.0, 0.125, 1.0, 1.0);  // Lower 7/8
    case 0x2588:
      return fillRect(0.0, 0.0, 1.0, 1.0);  // Full block
    case 0x2589:
      return fillRect(0.0, 0.0, 0.875, 1.0);  // Left 7/8
    case 0x258A:
      return fillRect(0.0, 0.0, 0.75, 1.0);  // Left 3/4
    case 0x258B:
      return fillRect(0.0, 0.0, 0.625, 1.0);  // Left 5/8
    case 0x258C:
      return fillRect(0.0, 0.0, 0.5, 1.0);  // Left half
    case 0x258D:
      return fillRect(0.0, 0.0, 0.375, 1.0);  // Left 3/8
    case 0x258E:
      return fillRect(0.0, 0.0, 0.25, 1.0);  // Left 1/4
    case 0x258F:
      return fillRect(0.0, 0.0, 0.125, 1.0);  // Left 1/8
    case 0x2590:
      return fillRect(0.5, 0.0, 1.0, 1.0);  // Right half
    // Shades (0x2591-0x2593) intentionally omitted - checkerboard patterns
    // create visual noise and don't represent actual density gradients
    case 0x2594:
      return fillRect(0.0, 0.0, 1.0, 0.125);  // Upper 1/8
    case 0x2595:
      return fillRect(0.875, 0.0, 1.0, 1.0);  // Right 1/8

    // Quadrants
    case 0x2596:
      return fillRect(0.0, 0.5, 0.5, 1.0);  // Lower-left
    case 0x2597:
      return fillRect(0.5, 0.5, 1.0, 1.0);  // Lower-right
    case 0x2598:
      return fillRect(0.0, 0.0, 0.5, 0.5);  // Upper-left
    case 0x2599:
      return fillRect(0.0, 0.0, 0.5, 1.0) | fillRect(0.5, 0.5, 1.0, 1.0);  // All but upper-right
    case 0x259A:
      return fillRect(0.0, 0.0, 0.5, 0.5) | fillRect(0.5, 0.5, 1.0, 1.0);  // Diagonal
    case 0x259B:
      return fillRect(0.0, 0.0, 1.0, 0.5) | fillRect(0.0, 0.5, 0.5, 1.0);  // All but lower-right
    case 0x259C:
      return fillRect(0.0, 0.0, 1.0, 0.5) | fillRect(0.5, 0.5, 1.0, 1.0);  // All but lower-left
    case 0x259D:
      return fillRect(0.5, 0.0, 1.0, 0.5);  // Upper-right
    case 0x259E:
      return fillRect(0.5, 0.0, 1.0, 0.5) | fillRect(0.0, 0.5, 0.5, 1.0);  // Anti-diagonal
    case 0x259F:
      return fillRect(0.5, 0.0, 1.0, 1.0) | fillRect(0.0, 0.5, 0.5, 1.0);  // All but upper-left

    default:
      return FillPattern{};
  }
}

// =============================================================================
// SUPPLEMENT: Octants (U+1CD00 to U+1CDE5) - 2×4 grid = 8 cells
// =============================================================================
// Octant encoding requires a lookup table - codepoints don't follow a formula.
// Table sourced from wezterm: https://github.com/wez/wezterm/pull/6502
constexpr uint32_t kFirstOctant = 0x1CD00;
constexpr uint32_t kLastOctant = 0x1CDE5;

// Maps (codepoint - 0x1CD00) to 8-bit pattern where each bit represents one cell
// in a 2×4 grid: bits 0,1 = top row, bits 2,3 = second row, etc.
// clang-format off
constexpr uint8_t kOctantPatterns[230] = {
    0b00000100, 0b00000110, 0b00000111, 0b00001000, 0b00001001, 0b00001011, 0b00001100,
    0b00001101, 0b00001110, 0b00010000, 0b00010001, 0b00010010, 0b00010011, 0b00010101,
    0b00010110, 0b00010111, 0b00011000, 0b00011001, 0b00011010, 0b00011011, 0b00011100,
    0b00011101, 0b00011110, 0b00011111, 0b00100000, 0b00100001, 0b00100010, 0b00100011,
    0b00100100, 0b00100101, 0b00100110, 0b00100111, 0b00101001, 0b00101010, 0b00101011,
    0b00101100, 0b00101101, 0b00101110, 0b00101111, 0b00110000, 0b00110001, 0b00110010,
    0b00110011, 0b00110100, 0b00110101, 0b00110110, 0b00110111, 0b00111000, 0b00111001,
    0b00111010, 0b00111011, 0b00111100, 0b00111101, 0b00111110, 0b01000001, 0b01000010,
    0b01000011, 0b01000100, 0b01000101, 0b01000110, 0b01000111, 0b01001000, 0b01001001,
    0b01001010, 0b01001011, 0b01001100, 0b01001101, 0b01001110, 0b01001111, 0b01010001,
    0b01010010, 0b01010011, 0b01010100, 0b01010110, 0b01010111, 0b01011000, 0b01011001,
    0b01011011, 0b01011100, 0b01011101, 0b01011110, 0b01100000, 0b01100001, 0b01100010,
    0b01100011, 0b01100100, 0b01100101, 0b01100110, 0b01100111, 0b01101000, 0b01101001,
    0b01101010, 0b01101011, 0b01101100, 0b01101101, 0b01101110, 0b01101111, 0b01110000,
    0b01110001, 0b01110010, 0b01110011, 0b01110100, 0b01110101, 0b01110110, 0b01110111,
    0b01111000, 0b01111001, 0b01111010, 0b01111011, 0b01111100, 0b01111101, 0b01111110,
    0b01111111, 0b10000001, 0b10000010, 0b10000011, 0b10000100, 0b10000101, 0b10000110,
    0b10000111, 0b10001000, 0b10001001, 0b10001010, 0b10001011, 0b10001100, 0b10001101,
    0b10001110, 0b10001111, 0b10010000, 0b10010001, 0b10010010, 0b10010011, 0b10010100,
    0b10010101, 0b10010110, 0b10010111, 0b10011000, 0b10011001, 0b10011010, 0b10011011,
    0b10011100, 0b10011101, 0b10011110, 0b10011111, 0b10100001, 0b10100010, 0b10100011,
    0b10100100, 0b10100110, 0b10100111, 0b10101000, 0b10101001, 0b10101011, 0b10101100,
    0b10101101, 0b10101110, 0b10110000, 0b10110001, 0b10110010, 0b10110011, 0b10110100,
    0b10110101, 0b10110110, 0b10110111, 0b10111000, 0b10111001, 0b10111010, 0b10111011,
    0b10111100, 0b10111101, 0b10111110, 0b10111111, 0b11000001, 0b11000010, 0b11000011,
    0b11000100, 0b11000101, 0b11000110, 0b11000111, 0b11001000, 0b11001001, 0b11001010,
    0b11001011, 0b11001100, 0b11001101, 0b11001110, 0b11001111, 0b11010000, 0b11010001,
    0b11010010, 0b11010011, 0b11010100, 0b11010101, 0b11010110, 0b11010111, 0b11011000,
    0b11011001, 0b11011010, 0b11011011, 0b11011100, 0b11011101, 0b11011110, 0b11011111,
    0b11100000, 0b11100001, 0b11100010, 0b11100011, 0b11100100, 0b11100101, 0b11100110,
    0b11100111, 0b11101000, 0b11101001, 0b11101010, 0b11101011, 0b11101100, 0b11101101,
    0b11101110, 0b11101111, 0b11110001, 0b11110010, 0b11110011, 0b11110100, 0b11110110,
    0b11110111, 0b11111000, 0b11111001, 0b11111011, 0b11111101, 0b11111110,
};
// clang-format on

inline auto octantBitsToPattern(uint8_t bits) -> FillPattern {
  FillPattern pattern;
  // 2×4 grid: bits 0,1 = row 0; bits 2,3 = row 1; bits 4,5 = row 2; bits 6,7 = row 3
  for (int y = 0; y < kSubpixelHeight; ++y) {
    double py = static_cast<double>(y) / kSubpixelHeight;
    for (int x = 0; x < kSubpixelWidth; ++x) {
      double px = static_cast<double>(x) / kSubpixelWidth;

      bool left = px < 0.5;
      int row = static_cast<int>(py * 4.0);  // 0-3
      row = std::min(row, 3);

      // Bit index: row * 2 + (left ? 0 : 1)
      int bit_idx = row * 2 + (left ? 0 : 1);
      if (bits & (1 << bit_idx)) {
        pattern.set(subpixelIndex(x, y));
      }
    }
  }
  return pattern;
}

inline auto getOctantPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstOctant || codepoint > kLastOctant) {
    return FillPattern{};
  }
  uint32_t idx = codepoint - kFirstOctant;
  if (idx >= std::size(kOctantPatterns)) {
    return FillPattern{};
  }
  return octantBitsToPattern(kOctantPatterns[idx]);
}

// =============================================================================
// SUPPLEMENT: Separated Block Quadrants (U+1CC21 to U+1CC2F)
// =============================================================================
// NOTE: Unicode 16.0 encoding not verified - disabled until we have proper mapping.
// TODO: Add proper lookup table when encoding is confirmed.
constexpr uint32_t kFirstSepQuadrant = 0x1CC21;
constexpr uint32_t kLastSepQuadrant = 0x1CC2F;

inline auto getSeparatedQuadrantPattern(uint32_t /*codepoint*/) -> FillPattern {
  // Separated quadrants disabled - encoding not verified
  return FillPattern{};
}

// =============================================================================
// SUPPLEMENT: Separated Block Sextants (U+1CE51 to U+1CE8F)
// =============================================================================
// NOTE: Unicode 16.0 encoding not verified - disabled until we have proper mapping.
// TODO: Add proper lookup table when encoding is confirmed.
constexpr uint32_t kFirstSepSextant = 0x1CE51;
constexpr uint32_t kLastSepSextant = 0x1CE8F;

inline auto getSeparatedSextantPattern(uint32_t /*codepoint*/) -> FillPattern {
  // Separated sextants disabled - encoding not verified
  return FillPattern{};
}

// =============================================================================
// SUPPLEMENT: Sixteenth Blocks (U+1CE90 to U+1CEAF)
// =============================================================================
// 4×4 grid where each cell is one sixteenth of the character cell.
// U+1CE90-U+1CE9F: Individual cells (row by row, left to right)
// U+1CEA0-U+1CEAF: Composite blocks (horizontal/vertical strips)
// Layout based on Ghostty implementation.
constexpr uint32_t kFirstSixteenth = 0x1CE90;
constexpr uint32_t kLastSixteenth = 0x1CEAF;

inline auto getSixteenthBlockPattern(uint32_t codepoint) -> FillPattern {
  if (codepoint < kFirstSixteenth || codepoint > kLastSixteenth) {
    return FillPattern{};
  }

  // Helper to fill a rectangular region in normalized [0,1] coordinates
  auto fillRegion = [](double x0, double y0, double x1, double y1) -> FillPattern {
    FillPattern p;
    for (int y = 0; y < kSubpixelHeight; ++y) {
      double py = (y + 0.5) / kSubpixelHeight;
      for (int x = 0; x < kSubpixelWidth; ++x) {
        double px = (x + 0.5) / kSubpixelWidth;
        if (px >= x0 && px < x1 && py >= y0 && py < y1) {
          p.set(subpixelIndex(x, y));
        }
      }
    }
    return p;
  };

  // Quarter boundaries for the 4×4 grid
  constexpr double q0 = 0.0, q1 = 0.25, q2 = 0.5, q3 = 0.75, q4 = 1.0;

  switch (codepoint) {
    // Individual sixteenth cells (U+1CE90-U+1CE9F)
    // Row 0 (top)
    case 0x1CE90: return fillRegion(q0, q0, q1, q1);  // Upper-left
    case 0x1CE91: return fillRegion(q1, q0, q2, q1);  // Upper-center-left
    case 0x1CE92: return fillRegion(q2, q0, q3, q1);  // Upper-center-right
    case 0x1CE93: return fillRegion(q3, q0, q4, q1);  // Upper-right
    // Row 1
    case 0x1CE94: return fillRegion(q0, q1, q1, q2);
    case 0x1CE95: return fillRegion(q1, q1, q2, q2);
    case 0x1CE96: return fillRegion(q2, q1, q3, q2);
    case 0x1CE97: return fillRegion(q3, q1, q4, q2);
    // Row 2
    case 0x1CE98: return fillRegion(q0, q2, q1, q3);
    case 0x1CE99: return fillRegion(q1, q2, q2, q3);
    case 0x1CE9A: return fillRegion(q2, q2, q3, q3);
    case 0x1CE9B: return fillRegion(q3, q2, q4, q3);
    // Row 3 (bottom)
    case 0x1CE9C: return fillRegion(q0, q3, q1, q4);
    case 0x1CE9D: return fillRegion(q1, q3, q2, q4);
    case 0x1CE9E: return fillRegion(q2, q3, q3, q4);
    case 0x1CE9F: return fillRegion(q3, q3, q4, q4);

    // Composite blocks (U+1CEA0-U+1CEAF) - from Ghostty source
    // 𜺠 RIGHT HALF LOWER ONE QUARTER BLOCK
    case 0x1CEA0: return fillRegion(q2, q3, q4, q4);
    // 𜺡 RIGHT THREE QUARTERS LOWER ONE QUARTER BLOCK
    case 0x1CEA1: return fillRegion(q1, q3, q4, q4);
    // 𜺢 LEFT THREE QUARTERS LOWER ONE QUARTER BLOCK
    case 0x1CEA2: return fillRegion(q0, q3, q3, q4);
    // 𜺣 LEFT HALF LOWER ONE QUARTER BLOCK
    case 0x1CEA3: return fillRegion(q0, q3, q2, q4);
    // 𜺤 LOWER HALF LEFT ONE QUARTER BLOCK
    case 0x1CEA4: return fillRegion(q0, q2, q1, q4);
    // 𜺥 LOWER THREE QUARTERS LEFT ONE QUARTER BLOCK
    case 0x1CEA5: return fillRegion(q0, q1, q1, q4);
    // 𜺦 UPPER THREE QUARTERS LEFT ONE QUARTER BLOCK
    case 0x1CEA6: return fillRegion(q0, q0, q1, q3);
    // 𜺧 UPPER HALF LEFT ONE QUARTER BLOCK
    case 0x1CEA7: return fillRegion(q0, q0, q1, q2);
    // 𜺨 LEFT HALF UPPER ONE QUARTER BLOCK
    case 0x1CEA8: return fillRegion(q0, q0, q2, q1);
    // 𜺩 LEFT THREE QUARTERS UPPER ONE QUARTER BLOCK
    case 0x1CEA9: return fillRegion(q0, q0, q3, q1);
    // 𜺪 RIGHT THREE QUARTERS UPPER ONE QUARTER BLOCK
    case 0x1CEAA: return fillRegion(q1, q0, q4, q1);
    // 𜺫 RIGHT HALF UPPER ONE QUARTER BLOCK
    case 0x1CEAB: return fillRegion(q2, q0, q4, q1);
    // 𜺬 UPPER HALF RIGHT ONE QUARTER BLOCK
    case 0x1CEAC: return fillRegion(q3, q0, q4, q2);
    // 𜺭 UPPER THREE QUARTERS RIGHT ONE QUARTER BLOCK
    case 0x1CEAD: return fillRegion(q3, q0, q4, q3);
    // 𜺮 LOWER THREE QUARTERS RIGHT ONE QUARTER BLOCK
    case 0x1CEAE: return fillRegion(q3, q1, q4, q4);
    // 𜺯 LOWER HALF RIGHT ONE QUARTER BLOCK
    case 0x1CEAF: return fillRegion(q3, q2, q4, q4);

    default: return FillPattern{};
  }
}

// =============================================================================
// Master lookup function - returns pattern for any supported codepoint
// =============================================================================
inline auto getPattern(uint32_t codepoint) -> FillPattern {
  // Block Elements (standard)
  if (codepoint >= kFirstBlockElement && codepoint <= kLastBlockElement) {
    return getBlockElementPattern(codepoint);
  }

  // Sextants
  if (codepoint >= kFirstSextant && codepoint <= kLastSextant) {
    return getSextantPattern(codepoint);
  }

  // Smooth Mosaics
  if (codepoint >= kFirstSmoothMosaic && codepoint <= kLastSmoothMosaic) {
    return getSmoothMosaicPattern(codepoint);
  }

  // Edge Triangles
  if (codepoint >= kFirstEdgeTriangle && codepoint <= kLastEdgeTriangle) {
    return getEdgeTrianglePattern(codepoint);
  }

  // Eighth Blocks
  if (codepoint >= kFirstEighthBlock && codepoint <= kLastEighthBlock) {
    return getEighthBlockPattern(codepoint);
  }

  // Combined Blocks
  if (codepoint >= kFirstCombinedBlock && codepoint <= kLastCombinedBlock) {
    return getCombinedBlockPattern(codepoint);
  }

  // Diagonal Fills
  if (codepoint >= kFirstDiagonalFill && codepoint <= kLastDiagonalFill) {
    return getDiagonalFillPattern(codepoint);
  }

  // Triangle & Shade
  if (codepoint >= kFirstTriangleShade && codepoint <= kLastTriangleShade) {
    return getTriangleShadePattern(codepoint);
  }

  // Corner Diagonals
  if (codepoint >= kFirstCornerDiagonal && codepoint <= kLastCornerDiagonal) {
    return getCornerDiagonalPattern(codepoint);
  }

  // Cell Diagonals
  if (codepoint >= kFirstCellDiagonal && codepoint <= kLastCellDiagonal) {
    return getCellDiagonalPattern(codepoint);
  }

  // Circles
  if (codepoint >= kFirstCircle && codepoint <= kLastCircle) {
    return getCirclePattern(codepoint);
  }

  // SUPPLEMENT: Octants
  if (codepoint >= kFirstOctant && codepoint <= kLastOctant) {
    return getOctantPattern(codepoint);
  }

  // SUPPLEMENT: Separated Quadrants
  if (codepoint >= kFirstSepQuadrant && codepoint <= kLastSepQuadrant) {
    return getSeparatedQuadrantPattern(codepoint);
  }

  // SUPPLEMENT: Separated Sextants
  if (codepoint >= kFirstSepSextant && codepoint <= kLastSepSextant) {
    return getSeparatedSextantPattern(codepoint);
  }

  // SUPPLEMENT: Sixteenth Blocks
  if (codepoint >= kFirstSixteenth && codepoint <= kLastSixteenth) {
    return getSixteenthBlockPattern(codepoint);
  }

  return FillPattern{};
}

// =============================================================================
// Character info structure for library building
// =============================================================================
struct CharInfo {
  uint32_t codepoint;
  const char* utf8;
  FillPattern pattern;
};

// Helper to encode codepoint to UTF-8
inline auto codepointToUtf8(uint32_t cp, char* buf) -> int {
  if (cp < 0x80) {
    buf[0] = static_cast<char>(cp);
    return 1;
  } else if (cp < 0x800) {
    buf[0] = static_cast<char>(0xC0 | (cp >> 6));
    buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
    return 2;
  } else if (cp < 0x10000) {
    buf[0] = static_cast<char>(0xE0 | (cp >> 12));
    buf[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
  } else {
    buf[0] = static_cast<char>(0xF0 | (cp >> 18));
    buf[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
  }
}

}  // namespace tempura
