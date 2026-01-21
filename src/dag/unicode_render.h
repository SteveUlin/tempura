#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dag/dag.h"

namespace tempura::dag {

// =============================================================================
// Unicode Characters for DAG Rendering
// =============================================================================
//
// Box-drawing characters for clean graph visualization:
//
// Lines:   ─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼
// Arrows:  → ← ↑ ↓ ↔ ↕
// Dashed:  ╌ ╎ ┄ ┆ ┈ ┊
// Double:  ═ ║ ╔ ╗ ╚ ╝
// Rounded: ╭ ╮ ╰ ╯
//
// Special arrows for causal diagrams:
//   ──→  Standard causal arrow
//   ╌╌→  Competing cause (dashed)
//   ┈┈→  Unobserved variable (dotted)
//   ←→   Bidirectional
//   ══→  Selection/conditioning (double)

namespace unicode {

// Horizontal lines
constexpr std::string_view kHorizontal = "─";
constexpr std::string_view kHorizontalDash = "╌";
constexpr std::string_view kHorizontalDot = "┈";
constexpr std::string_view kHorizontalDouble = "═";
constexpr std::string_view kHorizontalBold = "━";

// Vertical lines
constexpr std::string_view kVertical = "│";
constexpr std::string_view kVerticalDash = "╎";
constexpr std::string_view kVerticalDot = "┊";
constexpr std::string_view kVerticalDouble = "║";
constexpr std::string_view kVerticalBold = "┃";

// Corners (standard)
constexpr std::string_view kCornerTL = "┌";
constexpr std::string_view kCornerTR = "┐";
constexpr std::string_view kCornerBL = "└";
constexpr std::string_view kCornerBR = "┘";

// Corners (rounded)
constexpr std::string_view kRoundTL = "╭";
constexpr std::string_view kRoundTR = "╮";
constexpr std::string_view kRoundBL = "╰";
constexpr std::string_view kRoundBR = "╯";

// T-junctions
constexpr std::string_view kTeeLeft = "├";
constexpr std::string_view kTeeRight = "┤";
constexpr std::string_view kTeeUp = "┴";
constexpr std::string_view kTeeDown = "┬";
constexpr std::string_view kCross = "┼";

// Arrows (triangles for better alignment with box-drawing lines)
constexpr std::string_view kArrowRight = "▶";
constexpr std::string_view kArrowLeft = "◀";
constexpr std::string_view kArrowUp = "▲";
constexpr std::string_view kArrowDown = "▼";
constexpr std::string_view kArrowBiH = "◀▶";
constexpr std::string_view kArrowBiV = "▲▼";
constexpr std::string_view kTriUp = "▲";
constexpr std::string_view kTriDown = "▼";

// Node decorators
constexpr std::string_view kCircle = "○";
constexpr std::string_view kCircleFilled = "●";
constexpr std::string_view kSquare = "□";
constexpr std::string_view kSquareFilled = "■";
constexpr std::string_view kDiamond = "◇";
constexpr std::string_view kDiamondFilled = "◆";

}  // namespace unicode

// =============================================================================
// Grid-based Canvas for Unicode Rendering
// =============================================================================

class UnicodeCanvas {
 public:
  UnicodeCanvas(size_t width, size_t height)
      : width_{width}, height_{height}, cells_(width * height, ' ') {}

  void set(size_t x, size_t y, char c) {
    if (x < width_ && y < height_) {
      cells_[y * width_ + x] = c;
    }
  }

  void set(size_t x, size_t y, std::string_view s) {
    for (size_t i = 0; i < s.size() && x + i < width_; ++i) {
      cells_[y * width_ + x + i] = s[i];
    }
  }

  // Set a UTF-8 string (handles multi-byte characters)
  void setUtf8(size_t x, size_t y, std::string_view s) {
    if (x >= width_ || y >= height_) return;
    size_t pos = y * width_ + x;
    for (char c : s) {
      if (pos >= cells_.size()) break;
      cells_[pos++] = c;
    }
  }

  auto get(size_t x, size_t y) const -> char {
    if (x < width_ && y < height_) {
      return cells_[y * width_ + x];
    }
    return ' ';
  }

  auto render() const -> std::string {
    std::string result;
    result.reserve(height_ * (width_ + 1));
    for (size_t y = 0; y < height_; ++y) {
      for (size_t x = 0; x < width_; ++x) {
        result += cells_[y * width_ + x];
      }
      // Trim trailing spaces
      while (!result.empty() && result.back() == ' ') {
        result.pop_back();
      }
      result += '\n';
    }
    return result;
  }

  auto width() const -> size_t { return width_; }
  auto height() const -> size_t { return height_; }

 private:
  size_t width_;
  size_t height_;
  std::vector<char> cells_;
};

// =============================================================================
// Cell-based Canvas (proper Unicode support)
// =============================================================================
//
// Each cell holds a string (can be multi-byte UTF-8 for box drawing chars).
// Positions are in visual columns, not bytes.

class StringCanvas {
 public:
  StringCanvas(size_t width, size_t height)
      : width_{width}, height_{height}, cells_(width * height, " ") {}

  // Write a single-column character/string at position (x, y)
  void write(size_t x, size_t y, std::string_view text) {
    if (x >= width_ || y >= height_) return;
    cells_[y * width_ + x] = std::string{text};
  }

  // Write a multi-column string starting at (x, y)
  // Each character in ASCII takes one column
  void writeString(size_t x, size_t y, std::string_view text) {
    if (y >= height_) return;
    for (size_t i = 0; i < text.size() && x + i < width_; ++i) {
      // Only handle ASCII for labels - each byte = one column
      cells_[y * width_ + x + i] = std::string(1, text[i]);
    }
  }

  // Draw horizontal line from (x1, y) to (x2, y)
  void hline(size_t x1, size_t x2, size_t y, std::string_view ch) {
    if (y >= height_) return;
    if (x1 > x2) std::swap(x1, x2);
    for (size_t x = x1; x <= x2 && x < width_; ++x) {
      write(x, y, ch);
    }
  }

  // Draw vertical line from (x, y1) to (x, y2)
  void vline(size_t x, size_t y1, size_t y2, std::string_view ch) {
    if (x >= width_) return;
    if (y1 > y2) std::swap(y1, y2);
    for (size_t y = y1; y <= y2 && y < height_; ++y) {
      write(x, y, ch);
    }
  }

  auto get(size_t x, size_t y) const -> const std::string& {
    static const std::string empty = " ";
    if (x >= width_ || y >= height_) return empty;
    return cells_[y * width_ + x];
  }

  auto render() const -> std::string {
    std::string result;
    for (size_t y = 0; y < height_; ++y) {
      std::string row;
      for (size_t x = 0; x < width_; ++x) {
        row += cells_[y * width_ + x];
      }
      // Trim trailing spaces
      while (!row.empty() && row.back() == ' ') {
        row.pop_back();
      }
      result += row;
      result += '\n';
    }
    return result;
  }

  auto width() const -> size_t { return width_; }
  auto height() const -> size_t { return height_; }

 private:
  size_t width_;
  size_t height_;
  std::vector<std::string> cells_;  // Each cell is one visual column
};

// =============================================================================
// DAG Renderer - Converts DAG to Unicode text
// =============================================================================

struct RenderOptions {
  size_t node_width = 12;    // Width per node cell
  size_t node_height = 3;    // Height per node cell
  size_t h_spacing = 4;      // Horizontal spacing between nodes
  size_t v_spacing = 2;      // Vertical spacing between levels
  bool use_rounded = true;   // Use rounded corners for boxes
  bool show_labels = true;   // Show node labels
};

// Get arrow character based on edge type
inline auto getArrowChar(EdgeType type) -> std::string_view {
  switch (type) {
    case EdgeType::kCausal:
      return unicode::kArrowRight;
    case EdgeType::kCompeting:
      return unicode::kArrowRight;
    case EdgeType::kUnobserved:
      return unicode::kArrowRight;
    case EdgeType::kBidirectional:
      return unicode::kArrowBiH;
    case EdgeType::kSelection:
      return unicode::kArrowRight;
    default:
      return unicode::kArrowRight;
  }
}

// Get line character based on edge type
inline auto getLineChar(EdgeType type) -> std::string_view {
  switch (type) {
    case EdgeType::kCausal:
      return unicode::kHorizontal;
    case EdgeType::kCompeting:
      return unicode::kHorizontalDash;
    case EdgeType::kUnobserved:
      return unicode::kHorizontalDot;
    case EdgeType::kBidirectional:
      return unicode::kHorizontal;
    case EdgeType::kSelection:
      return unicode::kHorizontalDouble;
    default:
      return unicode::kHorizontal;
  }
}

// Simple text-based DAG rendering (for quick display)
inline auto renderSimple(const Dag& dag) -> std::string {
  std::string result;

  // Header
  result += "DAG Structure:\n";
  result += std::string(40, '-') + "\n";

  // List nodes
  result += "Nodes:\n";
  for (const auto& node : dag.nodes()) {
    result += std::format("  {} {}\n",
                          node.observed ? unicode::kCircleFilled
                                        : unicode::kCircle,
                          node.label);
  }
  result += "\n";

  // List edges with appropriate arrows
  result += "Edges:\n";
  for (const auto& edge : dag.edges()) {
    std::string arrow;
    switch (edge.type) {
      case EdgeType::kCausal:
        arrow = "───▶";
        break;
      case EdgeType::kCompeting:
        arrow = "╌╌╌▶";
        break;
      case EdgeType::kUnobserved:
        arrow = "┈┈┈▶";
        break;
      case EdgeType::kBidirectional:
        arrow = "◀──▶";
        break;
      case EdgeType::kSelection:
        arrow = "═══▶";
        break;
    }
    result += std::format("  {} {} {}", edge.from, arrow, edge.to);
    if (!edge.annotation.empty()) {
      result += std::format(" ({})", edge.annotation);
    }
    result += "\n";
  }

  return result;
}

// =============================================================================
// Layout Algorithm - Assign positions to nodes
// =============================================================================

struct LayoutResult {
  std::vector<std::pair<std::string, std::pair<int, int>>> positions;
  int width = 0;
  int height = 0;
};

// Simple layered layout based on topological order
inline auto computeLayout(const Dag& dag) -> LayoutResult {
  LayoutResult result;

  auto topo = dag.topologicalSort();
  if (topo.empty() && !dag.nodes().empty()) {
    // Cycle detected or disconnected - fall back to linear layout
    for (size_t i = 0; i < dag.nodes().size(); ++i) {
      result.positions.emplace_back(
          dag.nodes()[i].name,
          std::make_pair(static_cast<int>(i), 0));
    }
    result.width = static_cast<int>(dag.nodes().size());
    result.height = 1;
    return result;
  }

  // Assign layers based on longest path from roots
  std::vector<int> layers(dag.nodes().size(), 0);
  for (const auto& name : topo) {
    auto idx = std::distance(
        dag.nodes().begin(),
        std::ranges::find_if(dag.nodes(),
                             [&](const Node& n) { return n.name == name; }));

    int max_parent_layer = -1;
    for (const auto& parent_name : dag.parents(name)) {
      auto parent_idx = std::distance(
          dag.nodes().begin(),
          std::ranges::find_if(
              dag.nodes(),
              [&](const Node& n) { return n.name == parent_name; }));
      max_parent_layer =
          std::max(max_parent_layer, layers[static_cast<size_t>(parent_idx)]);
    }
    layers[static_cast<size_t>(idx)] = max_parent_layer + 1;
  }

  // Count nodes per layer
  int max_layer = *std::ranges::max_element(layers);
  std::vector<int> layer_counts(static_cast<size_t>(max_layer + 1), 0);
  std::vector<int> layer_positions(layers.size());

  for (size_t i = 0; i < layers.size(); ++i) {
    layer_positions[i] = layer_counts[static_cast<size_t>(layers[i])]++;
  }

  // Assign positions
  int max_width = *std::ranges::max_element(layer_counts);
  for (size_t i = 0; i < dag.nodes().size(); ++i) {
    result.positions.emplace_back(
        dag.nodes()[i].name,
        std::make_pair(layer_positions[i], layers[i]));
  }

  result.width = max_width;
  result.height = max_layer + 1;
  return result;
}

// =============================================================================
// Box Rendering - Draw nodes as boxes
// =============================================================================

inline auto renderBox(const Dag& dag, const RenderOptions& opts = {})
    -> std::string {
  auto layout = computeLayout(dag);

  // Calculate canvas size
  size_t canvas_width =
      static_cast<size_t>(layout.width) * (opts.node_width + opts.h_spacing) +
      opts.h_spacing;
  size_t canvas_height =
      static_cast<size_t>(layout.height) * (opts.node_height + opts.v_spacing) +
      opts.v_spacing;

  StringCanvas canvas(canvas_width, canvas_height);

  // Track node box positions for edge drawing
  struct BoxPos {
    size_t x, y, w, h;
    std::string name;
  };
  std::vector<BoxPos> boxes;

  // Draw nodes as boxes
  for (const auto& [name, pos] : layout.positions) {
    const auto* node = dag.findNode(name);
    if (!node) continue;

    size_t box_x =
        static_cast<size_t>(pos.first) * (opts.node_width + opts.h_spacing) +
        opts.h_spacing;
    size_t box_y =
        static_cast<size_t>(pos.second) * (opts.node_height + opts.v_spacing) +
        opts.v_spacing;

    boxes.push_back({box_x, box_y, opts.node_width, opts.node_height, name});

    // Choose corners
    std::string_view tl = opts.use_rounded ? unicode::kRoundTL : unicode::kCornerTL;
    std::string_view tr = opts.use_rounded ? unicode::kRoundTR : unicode::kCornerTR;
    std::string_view bl = opts.use_rounded ? unicode::kRoundBL : unicode::kCornerBL;
    std::string_view br = opts.use_rounded ? unicode::kRoundBR : unicode::kCornerBR;

    // Different box style for unobserved nodes
    if (!node->observed) {
      tl = unicode::kCornerTL;
      tr = unicode::kCornerTR;
      bl = unicode::kCornerBL;
      br = unicode::kCornerBR;
    }

    // Top border
    canvas.write(box_x, box_y, tl);
    for (size_t i = 1; i < opts.node_width - 1; ++i) {
      canvas.write(box_x + i, box_y,
                   node->observed ? unicode::kHorizontal
                                  : unicode::kHorizontalDash);
    }
    canvas.write(box_x + opts.node_width - 1, box_y, tr);

    // Sides
    for (size_t row = 1; row < opts.node_height - 1; ++row) {
      canvas.write(box_x, box_y + row,
                   node->observed ? unicode::kVertical : unicode::kVerticalDash);
      canvas.write(box_x + opts.node_width - 1, box_y + row,
                   node->observed ? unicode::kVertical : unicode::kVerticalDash);
    }

    // Bottom border
    canvas.write(box_x, box_y + opts.node_height - 1, bl);
    for (size_t i = 1; i < opts.node_width - 1; ++i) {
      canvas.write(box_x + i, box_y + opts.node_height - 1,
                   node->observed ? unicode::kHorizontal
                                  : unicode::kHorizontalDash);
    }
    canvas.write(box_x + opts.node_width - 1, box_y + opts.node_height - 1, br);

    // Label (centered)
    if (opts.show_labels) {
      std::string_view label = node->label;
      size_t label_len = std::min(label.size(), opts.node_width - 2);
      size_t label_x = box_x + 1 + (opts.node_width - 2 - label_len) / 2;
      size_t label_y = box_y + opts.node_height / 2;
      canvas.writeString(label_x, label_y, label.substr(0, label_len));
    }
  }

  // Draw edges
  for (const auto& edge : dag.edges()) {
    // Find source and target boxes
    auto src_it = std::ranges::find_if(
        boxes, [&](const BoxPos& b) { return b.name == edge.from; });
    auto dst_it = std::ranges::find_if(
        boxes, [&](const BoxPos& b) { return b.name == edge.to; });

    if (src_it == boxes.end() || dst_it == boxes.end()) continue;

    auto line_char = getLineChar(edge.type);
    auto arrow_char = getArrowChar(edge.type);

    // Determine if vertical or horizontal layout
    bool same_column = (src_it->x == dst_it->x);
    bool target_below = (dst_it->y > src_it->y);

    if (same_column && target_below) {
      // Vertical edge: connect bottom of source to top of target
      size_t x = src_it->x + src_it->w / 2;
      size_t src_y = src_it->y + src_it->h;
      size_t dst_y = dst_it->y;

      // Draw vertical line in the gap between boxes
      for (size_t y = src_y; y < dst_y; ++y) {
        canvas.write(x, y, unicode::kVertical);
      }
      // Draw arrow at the top of destination
      if (dst_y > 0) {
        canvas.write(x, dst_y - 1, unicode::kArrowDown);
      }
    } else if (same_column && !target_below) {
      // Vertical edge going up
      size_t x = src_it->x + src_it->w / 2;
      size_t src_y = src_it->y;
      size_t dst_y = dst_it->y + dst_it->h;

      for (size_t y = dst_y; y < src_y; ++y) {
        canvas.write(x, y, unicode::kVertical);
      }
      if (dst_y < src_y) {
        canvas.write(x, dst_y, unicode::kArrowUp);
      }
    } else {
      // Horizontal or diagonal: route right of source, then down/up, then to target
      size_t src_x = src_it->x + src_it->w;
      size_t src_y = src_it->y + src_it->h / 2;
      size_t dst_x = dst_it->x;
      size_t dst_y = dst_it->y + dst_it->h / 2;

      if (src_y == dst_y) {
        // Simple horizontal edge
        for (size_t x = src_x; x < dst_x; ++x) {
          canvas.write(x, src_y, line_char);
        }
        if (dst_x > 0) {
          canvas.write(dst_x - 1, dst_y, arrow_char);
        }
      } else {
        // Edge with bend - route outside the boxes
        size_t mid_x = src_x + 2;  // Route just outside the source box

        // Horizontal from source
        for (size_t x = src_x; x <= mid_x; ++x) {
          canvas.write(x, src_y, line_char);
        }

        // Vertical segment (outside the boxes)
        size_t y1 = std::min(src_y, dst_y);
        size_t y2 = std::max(src_y, dst_y);
        for (size_t y = y1; y <= y2; ++y) {
          canvas.write(mid_x, y, unicode::kVertical);
        }

        // Horizontal to target
        for (size_t x = mid_x; x < dst_x; ++x) {
          canvas.write(x, dst_y, line_char);
        }
        if (dst_x > 0) {
          canvas.write(dst_x - 1, dst_y, arrow_char);
        }

        // Corners
        if (src_y < dst_y) {
          canvas.write(mid_x, src_y, unicode::kCornerTR);
          canvas.write(mid_x, dst_y, unicode::kCornerBL);
        } else {
          canvas.write(mid_x, src_y, unicode::kCornerBR);
          canvas.write(mid_x, dst_y, unicode::kCornerTL);
        }
      }
    }
  }

  return canvas.render();
}

// =============================================================================
// Compact Text Rendering
// =============================================================================

inline auto renderCompact(const Dag& dag) -> std::string {
  std::string result;

  // Build adjacency representation
  for (const auto& node : dag.nodes()) {
    auto kids = dag.children(node.name);
    if (kids.empty()) continue;

    result += node.label;
    if (kids.size() == 1) {
      // Find edge type
      auto edge_it = std::ranges::find_if(
          dag.edges(), [&](const Edge& e) {
            return e.from == node.name && e.to == kids[0];
          });
      EdgeType type = (edge_it != dag.edges().end()) ? edge_it->type
                                                     : EdgeType::kCausal;
      switch (type) {
        case EdgeType::kCausal:
          result += " ──▶ ";
          break;
        case EdgeType::kCompeting:
          result += " ╌╌▶ ";
          break;
        case EdgeType::kUnobserved:
          result += " ┈┈▶ ";
          break;
        case EdgeType::kBidirectional:
          result += " ◀─▶ ";
          break;
        case EdgeType::kSelection:
          result += " ══▶ ";
          break;
      }

      const auto* child = dag.findNode(kids[0]);
      result += child != nullptr ? child->label : kids[0];
      result += "\n";
    } else {
      // Multiple children - show as fork
      result += " ─┬─▶ ";
      for (size_t i = 0; i < kids.size(); ++i) {
        const auto* child = dag.findNode(kids[i]);
        result += child != nullptr ? child->label : kids[i];
        if (i < kids.size() - 1) result += ", ";
      }
      result += "\n";
    }
  }

  return result;
}

}  // namespace tempura::dag
