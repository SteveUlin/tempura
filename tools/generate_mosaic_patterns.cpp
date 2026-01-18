// Generate correct smooth mosaic patterns from Ghostty's 3x4 grid format
// This tool outputs the 12-position perimeter encoding for each character

#include <array>
#include <cstdint>
#include <print>
#include <string>
#include <vector>

// Ghostty patterns: 4 rows of 3 characters each
// '.' = empty, '#' = filled, '/' = diagonal UL-LR, '\' = diagonal UR-LL
struct GhosttyPattern {
  uint32_t codepoint;
  const char* rows[4];  // 4 rows, each 3 chars
};

// All smooth mosaic patterns from Ghostty source
// Note: diagonals are implicit in the smooth rendering, we just track filled cells
constexpr GhosttyPattern kGhosttyPatterns[] = {
    {0x1FB3C, {"...", "...", "#..", "##."}},
    {0x1FB3D, {"...", "...", "#..", "###"}},
    {0x1FB3E, {"...", "#..", "#..", "##."}},
    {0x1FB3F, {"...", "#..", "##.", "###"}},
    {0x1FB40, {"#..", "#..", "##.", "##."}},
    {0x1FB41, {".##", "###", "###", "###"}},
    {0x1FB42, {"..#", "###", "###", "###"}},
    {0x1FB43, {".##", ".##", "###", "###"}},
    {0x1FB44, {"..#", ".##", "###", "###"}},
    {0x1FB45, {".##", ".##", ".##", "###"}},
    {0x1FB46, {"...", "..#", "###", "###"}},
    {0x1FB47, {"...", "...", "..#", ".##"}},
    {0x1FB48, {"...", "...", "..#", "###"}},
    {0x1FB49, {"...", "..#", ".##", ".##"}},
    {0x1FB4A, {"...", "..#", ".##", "###"}},
    {0x1FB4B, {"..#", "..#", ".##", ".##"}},
    {0x1FB4C, {"##.", "###", "###", "###"}},
    {0x1FB4D, {"#..", "###", "###", "###"}},
    {0x1FB4E, {"##.", "##.", "###", "###"}},
    {0x1FB4F, {"#..", "##.", "###", "###"}},
    {0x1FB50, {"##.", "##.", "##.", "###"}},
    {0x1FB51, {"...", "#..", "###", "###"}},
    {0x1FB52, {"###", "###", "###", ".##"}},
    {0x1FB53, {"###", "###", "###", "..#"}},
    {0x1FB54, {"###", "###", ".##", ".##"}},
    {0x1FB55, {"###", "###", ".##", "..#"}},
    {0x1FB56, {"###", ".##", ".##", ".##"}},
    {0x1FB57, {"##.", "#..", "...", "..."}},
    {0x1FB58, {"###", "#..", "...", "..."}},
    {0x1FB59, {"##.", "#..", "#..", "..."}},
    {0x1FB5A, {"###", "##.", "#..", "..."}},
    {0x1FB5B, {"##.", "##.", "#..", "#.."}},
    {0x1FB5C, {"###", "###", "#..", "..."}},
    {0x1FB5D, {"###", "###", "###", "##."}},
    {0x1FB5E, {"###", "###", "###", "#.."}},
    {0x1FB5F, {"###", "###", "##.", "##."}},
    {0x1FB60, {"###", "###", "##.", "#.."}},
    {0x1FB61, {"###", "##.", "##.", "##."}},
    {0x1FB62, {".##", "..#", "...", "..."}},
    {0x1FB63, {"###", "..#", "...", "..."}},
    {0x1FB64, {".##", "..#", "..#", "..."}},
    {0x1FB65, {"###", ".##", "..#", "..."}},
    {0x1FB66, {".##", ".##", "..#", "..#"}},
    {0x1FB67, {"###", "###", "..#", "..."}},
};

// Convert Ghostty 3x4 grid to 12-position perimeter encoding
// Perimeter positions:
//   [0]   [1]   [2]     <- top (y=0)
//   [3]         [5]     <- y=1/3
//   [6]         [8]     <- y=2/3
//   [9]  [10]  [11]     <- bottom (y=1)
auto convertToPerimeter(const GhosttyPattern& gp) -> std::string {
  std::string result(12, '.');

  // Map 3x4 grid positions to perimeter
  // Grid: rows 0-3, cols 0-2
  // Row 0 (top):    col0->p[0], col1->p[1], col2->p[2]
  // Row 1 (y=1/4):  col0->between p[0]/p[3], col2->between p[2]/p[5]
  // Row 2 (y=1/2):  col0->between p[3]/p[6], col2->between p[5]/p[8]
  // Row 3 (bottom): col0->p[9], col1->p[10], col2->p[11]

  // Top row
  if (gp.rows[0][0] == '#') result[0] = '#';
  if (gp.rows[0][1] == '#') result[1] = '#';
  if (gp.rows[0][2] == '#') result[2] = '#';

  // Bottom row
  if (gp.rows[3][0] == '#') result[9] = '#';
  if (gp.rows[3][1] == '#') result[10] = '#';
  if (gp.rows[3][2] == '#') result[11] = '#';

  // Left edge - check if left column is filled at various heights
  // p[3] at y=1/3: filled if row 1 col 0 is filled
  if (gp.rows[1][0] == '#') result[3] = '#';
  // p[6] at y=2/3: filled if row 2 col 0 is filled
  if (gp.rows[2][0] == '#') result[6] = '#';

  // Right edge - check if right column is filled at various heights
  // p[5] at y=1/3: filled if row 1 col 2 is filled
  if (gp.rows[1][2] == '#') result[5] = '#';
  // p[8] at y=2/3: filled if row 2 col 2 is filled
  if (gp.rows[2][2] == '#') result[8] = '#';

  return result;
}

int main() {
  std::println("// Smooth Mosaic Patterns - Generated from Ghostty source");
  std::println("// Format: 12-char perimeter encoding");
  std::println("inline constexpr const char* kMosaicPatterns[] = {{");

  for (const auto& gp : kGhosttyPatterns) {
    std::string perimeter = convertToPerimeter(gp);
    std::println("    \"{}\",  // 0x{:04X}", perimeter, gp.codepoint);
  }

  std::println("}};");

  // Also print visual comparison
  std::println("\n// Visual comparison:");
  for (const auto& gp : kGhosttyPatterns) {
    std::string perimeter = convertToPerimeter(gp);
    std::println("\n// 0x{:04X}:", gp.codepoint);
    std::println("// Ghostty: {} {} {} {}", gp.rows[0], gp.rows[1], gp.rows[2], gp.rows[3]);
    std::println("// Perimeter: {}", perimeter);
  }

  return 0;
}
