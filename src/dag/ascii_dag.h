#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "dag/dag.h"

namespace tempura::dag {

// =============================================================================
// Character Sets for DAG Rendering
// =============================================================================

struct CharSet {
  std::string_view horiz;
  std::string_view vert;
  std::string_view corner_tl;
  std::string_view corner_tr;
  std::string_view corner_bl;
  std::string_view corner_br;
  std::string_view tee_down;
  std::string_view tee_up;
  std::string_view tee_left;
  std::string_view tee_right;
  std::string_view cross;
  std::string_view arrow_down;
  std::string_view arrow_up;
  std::string_view arrow_left;
  std::string_view arrow_right;
  std::string_view node_marker;
};

// Pure ASCII - maximum compatibility
inline constexpr CharSet kAscii = {
    .horiz = "-",
    .vert = "|",
    .corner_tl = "+",
    .corner_tr = "+",
    .corner_bl = "+",
    .corner_br = "+",
    .tee_down = "+",
    .tee_up = "+",
    .tee_left = "+",
    .tee_right = "+",
    .cross = "+",
    .arrow_down = "v",
    .arrow_up = "^",
    .arrow_left = "<",
    .arrow_right = ">",
    .node_marker = "*",
};

// Unicode box drawing - sharp corners
inline constexpr CharSet kBoxSharp = {
    .horiz = "─",
    .vert = "│",
    .corner_tl = "┌",
    .corner_tr = "┐",
    .corner_bl = "└",
    .corner_br = "┘",
    .tee_down = "┬",
    .tee_up = "┴",
    .tee_left = "├",
    .tee_right = "┤",
    .cross = "┼",
    .arrow_down = "▼",
    .arrow_up = "▲",
    .arrow_left = "◀",
    .arrow_right = "▶",
    .node_marker = "●",
};

// Unicode box drawing - rounded corners
inline constexpr CharSet kBoxRounded = {
    .horiz = "─",
    .vert = "│",
    .corner_tl = "╭",
    .corner_tr = "╮",
    .corner_bl = "╰",
    .corner_br = "╯",
    .tee_down = "┬",
    .tee_up = "┴",
    .tee_left = "├",
    .tee_right = "┤",
    .cross = "┼",
    .arrow_down = "▼",
    .arrow_up = "▲",
    .arrow_left = "◀",
    .arrow_right = "▶",
    .node_marker = "●",
};

// =============================================================================
// Render Configuration
// =============================================================================

struct AsciiDagConfig {
  size_t box_padding = 1;      // Padding inside box (each side)
  size_t min_box_width = 5;    // Minimum box width (excluding border)
  size_t h_gap = 3;            // Horizontal gap between boxes
  size_t v_gap = 2;            // Vertical gap between layers (for edge routing)
  bool show_arrows = true;     // Show arrow heads on edges
  CharSet chars = kBoxSharp;   // Character set to use
};

// =============================================================================
// Canvas with proper cell tracking
// =============================================================================

class AsciiCanvas {
 public:
  AsciiCanvas(size_t width, size_t height)
      : width_{width}, height_{height}, cells_(width * height, " ") {}

  void put(size_t x, size_t y, std::string_view ch) {
    if (x < width_ && y < height_) {
      cells_[y * width_ + x] = ch;
    }
  }

  void putString(size_t x, size_t y, std::string_view s) {
    for (size_t i = 0; i < s.size() && x + i < width_; ++i) {
      cells_[y * width_ + x + i] = std::string(1, s[i]);
    }
  }

  void hline(size_t x1, size_t x2, size_t y, std::string_view ch) {
    if (x1 > x2) std::swap(x1, x2);
    for (size_t x = x1; x <= x2; ++x) {
      put(x, y, ch);
    }
  }

  void vline(size_t x, size_t y1, size_t y2, std::string_view ch) {
    if (y1 > y2) std::swap(y1, y2);
    for (size_t y = y1; y <= y2; ++y) {
      put(x, y, ch);
    }
  }

  auto get(size_t x, size_t y) const -> const std::string& {
    static const std::string space = " ";
    if (x >= width_ || y >= height_) return space;
    return cells_[y * width_ + x];
  }

  auto render() const -> std::string {
    std::string result;
    for (size_t y = 0; y < height_; ++y) {
      std::string row;
      for (size_t x = 0; x < width_; ++x) {
        row += cells_[y * width_ + x];
      }
      while (!row.empty() && row.back() == ' ') {
        row.pop_back();
      }
      result += row;
      result += '\n';
    }
    while (result.size() >= 2 && result[result.size() - 1] == '\n' &&
           result[result.size() - 2] == '\n') {
      result.pop_back();
    }
    return result;
  }

  auto width() const -> size_t { return width_; }
  auto height() const -> size_t { return height_; }

 private:
  size_t width_;
  size_t height_;
  std::vector<std::string> cells_;
};

// =============================================================================
// Layout Data Structures
// =============================================================================

struct LayoutNode {
  size_t original_idx;
  std::string label;
  size_t layer;
  size_t position;       // Position within layer (0, 1, 2, ...)
  double x_coord;        // Actual x coordinate for rendering
  bool is_dummy = false;
};

struct LayoutEdge {
  size_t from_node;
  size_t to_node;
};

struct Layout {
  std::vector<LayoutNode> nodes;
  std::vector<LayoutEdge> edges;
  std::vector<std::vector<size_t>> layers;
  size_t num_layers = 0;
};

// =============================================================================
// Step 1: Layer Assignment (longest path from roots)
// =============================================================================

inline auto assignLayers(const Dag& dag) -> std::vector<size_t> {
  const auto& nodes = dag.nodes();
  std::vector<size_t> layers(nodes.size(), 0);

  auto topo = dag.topologicalSort();
  if (topo.empty() && !nodes.empty()) {
    for (size_t i = 0; i < nodes.size(); ++i) {
      layers[i] = i;
    }
    return layers;
  }

  auto findIdx = [&](std::string_view name) -> size_t {
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (nodes[i].name == name) return i;
    }
    return SIZE_MAX;
  };

  for (const auto& name : topo) {
    size_t idx = findIdx(name);
    if (idx == SIZE_MAX) continue;

    size_t max_parent_layer = 0;
    bool has_parent = false;
    for (const auto& parent_name : dag.parents(name)) {
      size_t parent_idx = findIdx(parent_name);
      if (parent_idx != SIZE_MAX) {
        max_parent_layer = std::max(max_parent_layer, layers[parent_idx] + 1);
        has_parent = true;
      }
    }
    layers[idx] = has_parent ? max_parent_layer : 0;
  }

  return layers;
}

// =============================================================================
// Step 2: Build Layout (no dummy nodes - edges route directly)
// =============================================================================

inline auto buildLayout(const Dag& dag) -> Layout {
  Layout layout;
  const auto& nodes = dag.nodes();
  const auto& edges = dag.edges();

  auto node_layers = assignLayers(dag);

  // Only add real nodes to layout (no dummies)
  for (size_t i = 0; i < nodes.size(); ++i) {
    layout.nodes.push_back({
        .original_idx = i,
        .label = nodes[i].label,
        .layer = node_layers[i],
        .position = 0,
        .x_coord = 0.0,
        .is_dummy = false,
    });
  }

  auto findNodeIdx = [&](std::string_view name) -> size_t {
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (nodes[i].name == name) return i;
    }
    return SIZE_MAX;
  };

  // Add edges directly (no dummy nodes for skip-layer edges)
  for (const auto& edge : edges) {
    size_t from_idx = findNodeIdx(edge.from);
    size_t to_idx = findNodeIdx(edge.to);
    if (from_idx == SIZE_MAX || to_idx == SIZE_MAX) continue;
    layout.edges.push_back({from_idx, to_idx});
  }

  size_t max_layer = 0;
  for (const auto& node : layout.nodes) {
    max_layer = std::max(max_layer, node.layer);
  }
  layout.num_layers = max_layer + 1;
  layout.layers.resize(layout.num_layers);

  for (size_t i = 0; i < layout.nodes.size(); ++i) {
    layout.layers[layout.nodes[i].layer].push_back(i);
  }

  for (auto& layer : layout.layers) {
    for (size_t pos = 0; pos < layer.size(); ++pos) {
      layout.nodes[layer[pos]].position = pos;
    }
  }

  return layout;
}

// =============================================================================
// Step 3: Crossing Minimization (Barycenter with local optimization)
// =============================================================================

inline auto countCrossings(const Layout& layout,
                           const std::vector<std::vector<size_t>>& children) -> size_t {
  size_t crossings = 0;
  for (size_t layer_idx = 0; layer_idx + 1 < layout.num_layers; ++layer_idx) {
    const auto& layer = layout.layers[layer_idx];
    // Count crossings between adjacent layers
    for (size_t i = 0; i < layer.size(); ++i) {
      for (size_t j = i + 1; j < layer.size(); ++j) {
        size_t ni = layer[i];
        size_t nj = layer[j];
        // For each pair of nodes in this layer, count crossing edges
        for (size_t ci : children[ni]) {
          for (size_t cj : children[nj]) {
            // Crossing if ni is left of nj but ci is right of cj
            if (layout.nodes[ci].position > layout.nodes[cj].position) {
              ++crossings;
            }
          }
        }
      }
    }
  }
  return crossings;
}

inline void minimizeCrossings(Layout& layout, int iterations = 24) {
  if (layout.num_layers < 2) return;

  std::vector<std::vector<size_t>> children(layout.nodes.size());
  std::vector<std::vector<size_t>> parents(layout.nodes.size());
  for (const auto& edge : layout.edges) {
    children[edge.from_node].push_back(edge.to_node);
    parents[edge.to_node].push_back(edge.from_node);
  }

  // Barycenter: average position of neighbors
  auto barycenter = [&](const std::vector<size_t>& neighbors) -> double {
    if (neighbors.empty()) return -1.0;
    double sum = 0.0;
    for (size_t n : neighbors) {
      sum += static_cast<double>(layout.nodes[n].position);
    }
    return sum / static_cast<double>(neighbors.size());
  };

  for (int iter = 0; iter < iterations; ++iter) {
    // Down sweep
    for (size_t layer_idx = 1; layer_idx < layout.num_layers; ++layer_idx) {
      auto& layer = layout.layers[layer_idx];
      std::vector<std::pair<double, size_t>> scored;
      scored.reserve(layer.size());

      for (size_t node_idx : layer) {
        double bc = barycenter(parents[node_idx]);
        if (bc < 0) bc = static_cast<double>(layout.nodes[node_idx].position);
        scored.emplace_back(bc, node_idx);
      }

      std::ranges::stable_sort(scored, [](auto& a, auto& b) {
        return a.first < b.first;
      });

      for (size_t pos = 0; pos < scored.size(); ++pos) {
        layer[pos] = scored[pos].second;
        layout.nodes[scored[pos].second].position = pos;
      }
    }

    // Up sweep
    for (size_t layer_idx = layout.num_layers - 1; layer_idx > 0; --layer_idx) {
      auto& layer = layout.layers[layer_idx - 1];
      std::vector<std::pair<double, size_t>> scored;
      scored.reserve(layer.size());

      for (size_t node_idx : layer) {
        double bc = barycenter(children[node_idx]);
        if (bc < 0) bc = static_cast<double>(layout.nodes[node_idx].position);
        scored.emplace_back(bc, node_idx);
      }

      std::ranges::stable_sort(scored, [](auto& a, auto& b) {
        return a.first < b.first;
      });

      for (size_t pos = 0; pos < scored.size(); ++pos) {
        layer[pos] = scored[pos].second;
        layout.nodes[scored[pos].second].position = pos;
      }
    }

    // Local swap optimization: try swapping adjacent nodes
    for (size_t layer_idx = 0; layer_idx < layout.num_layers; ++layer_idx) {
      auto& layer = layout.layers[layer_idx];
      bool improved = true;
      while (improved) {
        improved = false;
        for (size_t i = 0; i + 1 < layer.size(); ++i) {
          size_t before = countCrossings(layout, children);
          // Swap
          std::swap(layer[i], layer[i + 1]);
          layout.nodes[layer[i]].position = i;
          layout.nodes[layer[i + 1]].position = i + 1;
          size_t after = countCrossings(layout, children);
          if (after >= before) {
            // Swap back
            std::swap(layer[i], layer[i + 1]);
            layout.nodes[layer[i]].position = i;
            layout.nodes[layer[i + 1]].position = i + 1;
          } else {
            improved = true;
          }
        }
      }
    }
  }
}

// =============================================================================
// Step 4: Coordinate Assignment (Barycenter centering)
// =============================================================================

inline void assignCoordinates(Layout& layout, size_t node_width, size_t h_gap) {
  if (layout.num_layers == 0) return;

  // Assign x coordinates based on position, with spacing
  size_t cell_width = node_width + h_gap;

  for (size_t layer_idx = 0; layer_idx < layout.num_layers; ++layer_idx) {
    const auto& layer = layout.layers[layer_idx];
    for (size_t i = 0; i < layer.size(); ++i) {
      layout.nodes[layer[i]].x_coord = static_cast<double>(i * cell_width);
    }
  }

  // Center layers based on widest layer
  size_t max_width = 0;
  for (const auto& layer : layout.layers) {
    if (!layer.empty()) {
      size_t width = static_cast<size_t>(
          layout.nodes[layer.back()].x_coord + node_width);
      max_width = std::max(max_width, width);
    }
  }

  for (size_t layer_idx = 0; layer_idx < layout.num_layers; ++layer_idx) {
    const auto& layer = layout.layers[layer_idx];
    if (layer.empty()) continue;
    size_t layer_width = static_cast<size_t>(
        layout.nodes[layer.back()].x_coord + node_width);
    double offset = static_cast<double>(max_width - layer_width) / 2.0;
    for (size_t node_idx : layer) {
      layout.nodes[node_idx].x_coord += offset;
    }
  }
}

// =============================================================================
// Step 5: Render to ASCII with proper edge routing
// =============================================================================

// Edge routing data for one edge
struct EdgeRoute {
  size_t from_node;
  size_t to_node;
  size_t from_x;
  size_t to_x;
  size_t from_layer;
  size_t to_layer;
  size_t channel;  // Which routing channel to use (0, 1, 2, ...)
};

inline auto renderAsciiDag(const Dag& dag, const AsciiDagConfig& cfg = {})
    -> std::string {
  if (dag.nodes().empty()) return "";

  auto layout = buildLayout(dag);
  minimizeCrossings(layout);

  const auto& ch = cfg.chars;

  // Calculate box widths - use uniform width
  size_t max_label_len = 0;
  for (const auto& node : layout.nodes) {
    if (!node.is_dummy) {
      max_label_len = std::max(max_label_len, node.label.size());
    }
  }
  size_t box_width = std::max(cfg.min_box_width, max_label_len + cfg.box_padding * 2);

  assignCoordinates(layout, box_width, cfg.h_gap);

  // Calculate dimensions
  size_t box_height = 3;
  size_t channels_per_gap = cfg.v_gap + 2;  // Routing channels between layers
  size_t layer_height = box_height + channels_per_gap;

  double max_x = 0;
  for (const auto& node : layout.nodes) {
    if (!node.is_dummy) {
      max_x = std::max(max_x, node.x_coord + box_width);
    }
  }
  size_t canvas_width = static_cast<size_t>(max_x) + 4;
  size_t canvas_height = layout.num_layers * layer_height + 2;

  AsciiCanvas canvas(canvas_width, canvas_height);

  // Helper functions
  auto nodeX = [&](size_t idx) -> size_t {
    return static_cast<size_t>(layout.nodes[idx].x_coord);
  };
  auto nodeCenterX = [&](size_t idx) -> size_t {
    if (layout.nodes[idx].is_dummy) {
      return nodeX(idx);
    }
    return nodeX(idx) + box_width / 2;
  };
  auto nodeTopY = [&](size_t idx) -> size_t {
    return layout.nodes[idx].layer * layer_height;
  };
  auto nodeBottomY = [&](size_t idx) -> size_t {
    return nodeTopY(idx) + box_height - 1;
  };

  // Track box occupied areas (for edge routing to avoid)
  std::vector<std::tuple<size_t, size_t, size_t, size_t>> box_rects; // (x1, y1, x2, y2)

  // Draw all boxes first
  for (size_t i = 0; i < layout.nodes.size(); ++i) {
    const auto& node = layout.nodes[i];
    if (node.is_dummy) continue;

    size_t x = nodeX(i);
    size_t y = nodeTopY(i);

    // Track this box's area
    box_rects.emplace_back(x, y, x + box_width - 1, y + 2);

    canvas.put(x, y, ch.corner_tl);
    canvas.hline(x + 1, x + box_width - 2, y, ch.horiz);
    canvas.put(x + box_width - 1, y, ch.corner_tr);

    canvas.put(x, y + 1, ch.vert);
    size_t label_start = x + 1 + (box_width - 2 - node.label.size()) / 2;
    canvas.putString(label_start, y + 1, node.label);
    canvas.put(x + box_width - 1, y + 1, ch.vert);

    canvas.put(x, y + 2, ch.corner_bl);
    canvas.hline(x + 1, x + box_width - 2, y + 2, ch.horiz);
    canvas.put(x + box_width - 1, y + 2, ch.corner_br);
  }

  // Helper to check if a position is inside a box
  auto isInsideBox = [&](size_t x, size_t y) -> bool {
    for (const auto& [bx1, by1, bx2, by2] : box_rects) {
      if (x >= bx1 && x <= bx2 && y >= by1 && y <= by2) {
        return true;
      }
    }
    return false;
  };

  // Build edge routes with channel assignment
  std::vector<EdgeRoute> routes;
  routes.reserve(layout.edges.size());

  for (const auto& edge : layout.edges) {
    size_t from = edge.from_node;
    size_t to = edge.to_node;
    routes.push_back({
        .from_node = from,
        .to_node = to,
        .from_x = nodeCenterX(from),
        .to_x = nodeCenterX(to),
        .from_layer = layout.nodes[from].layer,
        .to_layer = layout.nodes[to].layer,
        .channel = 0,
    });
  }

  // Assign channels to avoid collisions
  // Sort routes by source layer, then by horizontal span
  std::ranges::sort(routes, [](const EdgeRoute& a, const EdgeRoute& b) {
    if (a.from_layer != b.from_layer) return a.from_layer < b.from_layer;
    size_t span_a = a.from_x < a.to_x ? a.to_x - a.from_x : a.from_x - a.to_x;
    size_t span_b = b.from_x < b.to_x ? b.to_x - b.from_x : b.from_x - b.to_x;
    return span_a > span_b;  // Longer spans first
  });

  // Use SIZE_MAX as sentinel for unassigned channels
  for (auto& route : routes) {
    route.channel = SIZE_MAX;
  }

  for (auto& route : routes) {
    if (route.from_layer >= route.to_layer) continue;  // Skip back edges

    size_t min_x = std::min(route.from_x, route.to_x);
    size_t max_x = std::max(route.from_x, route.to_x);

    // Find first available channel for this route
    size_t channel = 0;
    bool found = false;

    while (!found) {
      // Check if this channel is available for our x range
      bool available = true;
      // Only check routes that have already been assigned (channel != SIZE_MAX)
      for (const auto& existing : routes) {
        if (existing.channel == SIZE_MAX) continue;  // Not yet assigned
        if (existing.from_layer != route.from_layer) continue;
        if (existing.channel != channel) continue;

        size_t ex_min = std::min(existing.from_x, existing.to_x);
        size_t ex_max = std::max(existing.from_x, existing.to_x);

        // Check overlap with some margin
        if (max_x + 1 >= ex_min && min_x <= ex_max + 1) {
          available = false;
          break;
        }
      }

      if (available) {
        found = true;
      } else {
        ++channel;
        if (channel > 10) {
          // Safety limit
          found = true;
        }
      }
    }

    route.channel = channel;
  }

  // Helper to merge junction characters properly
  auto mergeJunction = [&](size_t x, size_t y, std::string_view wanted) {
    auto existing = canvas.get(x, y);
    if (existing == " ") {
      canvas.put(x, y, wanted);
      return;
    }
    // Merge logic: corners + opposite direction = T-junction or cross
    std::string e = existing;
    std::string w(wanted);

    // If same, keep it
    if (e == w) return;

    // corner_bl (└) + corner_br (┘) = tee_up (┴)
    if ((e == std::string(ch.corner_bl) && w == std::string(ch.corner_br)) ||
        (e == std::string(ch.corner_br) && w == std::string(ch.corner_bl))) {
      canvas.put(x, y, ch.tee_up);
      return;
    }
    // corner_tl (┌) + corner_tr (┐) = tee_down (┬)
    if ((e == std::string(ch.corner_tl) && w == std::string(ch.corner_tr)) ||
        (e == std::string(ch.corner_tr) && w == std::string(ch.corner_tl))) {
      canvas.put(x, y, ch.tee_down);
      return;
    }
    // horiz + vert = cross
    if ((e == std::string(ch.horiz) && w == std::string(ch.vert)) ||
        (e == std::string(ch.vert) && w == std::string(ch.horiz))) {
      canvas.put(x, y, ch.cross);
      return;
    }
    // vert + corner = tee (line continues through corner)
    // └ (up,right) + │ (up,down) = ├ (up,down,right) = tee_left
    // ┘ (up,left) + │ (up,down) = ┤ (up,down,left) = tee_right
    if (e == std::string(ch.vert) && w == std::string(ch.corner_bl)) {
      canvas.put(x, y, ch.tee_left);
      return;
    }
    if (e == std::string(ch.vert) && w == std::string(ch.corner_br)) {
      canvas.put(x, y, ch.tee_right);
      return;
    }
    // corner + vert = tee (corner already there, vert added)
    if (e == std::string(ch.corner_bl) && w == std::string(ch.vert)) {
      canvas.put(x, y, ch.tee_left);
      return;
    }
    if (e == std::string(ch.corner_br) && w == std::string(ch.vert)) {
      canvas.put(x, y, ch.tee_right);
      return;
    }
    // Top corners + vert = tee
    // ┌ (right,down) + │ (up,down) = ├ (up,down,right) = tee_left
    // ┐ (left,down) + │ (up,down) = ┤ (up,down,left) = tee_right
    if (e == std::string(ch.vert) && w == std::string(ch.corner_tl)) {
      canvas.put(x, y, ch.tee_left);
      return;
    }
    if (e == std::string(ch.vert) && w == std::string(ch.corner_tr)) {
      canvas.put(x, y, ch.tee_right);
      return;
    }
    if (e == std::string(ch.corner_tl) && w == std::string(ch.vert)) {
      canvas.put(x, y, ch.tee_left);
      return;
    }
    if (e == std::string(ch.corner_tr) && w == std::string(ch.vert)) {
      canvas.put(x, y, ch.tee_right);
      return;
    }
    // tee_up + vert from above = cross
    if (e == std::string(ch.tee_up) && w == std::string(ch.vert)) {
      canvas.put(x, y, ch.cross);
      return;
    }
    // tee + vert = cross
    if ((e == std::string(ch.tee_down) || e == std::string(ch.tee_left) ||
         e == std::string(ch.tee_right)) &&
        w == std::string(ch.vert)) {
      canvas.put(x, y, ch.cross);
      return;
    }
    // horiz + corner = appropriate tee (and corner + horiz for commutativity)
    // └ (up,right) + horiz (left,right) = ┴ (left,right,up) = tee_up
    // ┘ (up,left) + horiz (left,right) = ┴ (left,right,up) = tee_up
    // ┌ (right,down) + horiz (left,right) = ┬ (left,right,down) = tee_down
    // ┐ (left,down) + horiz (left,right) = ┬ (left,right,down) = tee_down
    if ((e == std::string(ch.horiz) && w == std::string(ch.corner_bl)) ||
        (e == std::string(ch.corner_bl) && w == std::string(ch.horiz))) {
      canvas.put(x, y, ch.tee_up);
      return;
    }
    if ((e == std::string(ch.horiz) && w == std::string(ch.corner_br)) ||
        (e == std::string(ch.corner_br) && w == std::string(ch.horiz))) {
      canvas.put(x, y, ch.tee_up);
      return;
    }
    if ((e == std::string(ch.horiz) && w == std::string(ch.corner_tl)) ||
        (e == std::string(ch.corner_tl) && w == std::string(ch.horiz))) {
      canvas.put(x, y, ch.tee_down);
      return;
    }
    if ((e == std::string(ch.horiz) && w == std::string(ch.corner_tr)) ||
        (e == std::string(ch.corner_tr) && w == std::string(ch.horiz))) {
      canvas.put(x, y, ch.tee_down);
      return;
    }
    // T-junction + corner where corner's arms are subset of T's arms → keep T
    // ┬ (left/right/down) + ┌ (right/down) or ┐ (left/down) = ┬
    // ┴ (left/right/up) + └ (right/up) or ┘ (left/up) = ┴
    // ├ (up/down/right) + └ (up/right) or ┌ (down/right) = ├
    // ┤ (up/down/left) + ┘ (up/left) or ┐ (down/left) = ┤
    if (e == std::string(ch.tee_down) &&
        (w == std::string(ch.corner_tl) || w == std::string(ch.corner_tr))) {
      return;  // Keep existing ┬
    }
    if (e == std::string(ch.tee_up) &&
        (w == std::string(ch.corner_bl) || w == std::string(ch.corner_br))) {
      return;  // Keep existing ┴
    }
    if (e == std::string(ch.tee_left) &&
        (w == std::string(ch.corner_bl) || w == std::string(ch.corner_tl))) {
      return;  // Keep existing ├
    }
    if (e == std::string(ch.tee_right) &&
        (w == std::string(ch.corner_br) || w == std::string(ch.corner_tr))) {
      return;  // Keep existing ┤
    }
    // Commutative: corner + T-junction (corner drawn first)
    if (w == std::string(ch.tee_down) &&
        (e == std::string(ch.corner_tl) || e == std::string(ch.corner_tr))) {
      canvas.put(x, y, ch.tee_down);
      return;
    }
    if (w == std::string(ch.tee_up) &&
        (e == std::string(ch.corner_bl) || e == std::string(ch.corner_br))) {
      canvas.put(x, y, ch.tee_up);
      return;
    }
    if (w == std::string(ch.tee_left) &&
        (e == std::string(ch.corner_bl) || e == std::string(ch.corner_tl))) {
      canvas.put(x, y, ch.tee_left);
      return;
    }
    if (w == std::string(ch.tee_right) &&
        (e == std::string(ch.corner_br) || e == std::string(ch.corner_tr))) {
      canvas.put(x, y, ch.tee_right);
      return;
    }
    // Default: overwrite (shouldn't happen often)
    canvas.put(x, y, wanted);
  };

  // Find a clear vertical path (x position) that avoids boxes between y1 and y2
  auto findClearVerticalPath = [&](size_t preferred_x, size_t y1, size_t y2) -> size_t {
    // Check if preferred path is clear
    bool clear = true;
    for (size_t y = y1; y <= y2; ++y) {
      if (isInsideBox(preferred_x, y)) {
        clear = false;
        break;
      }
    }
    if (clear) return preferred_x;

    // Search left and right for a clear path
    for (size_t offset = 1; offset < canvas_width; ++offset) {
      // Try left
      if (preferred_x >= offset) {
        size_t try_x = preferred_x - offset;
        clear = true;
        for (size_t y = y1; y <= y2; ++y) {
          if (isInsideBox(try_x, y)) {
            clear = false;
            break;
          }
        }
        if (clear) return try_x;
      }
      // Try right
      if (preferred_x + offset < canvas_width) {
        size_t try_x = preferred_x + offset;
        clear = true;
        for (size_t y = y1; y <= y2; ++y) {
          if (isInsideBox(try_x, y)) {
            clear = false;
            break;
          }
        }
        if (clear) return try_x;
      }
    }
    return preferred_x; // Fallback
  };

  // Draw edges
  for (const auto& route : routes) {
    if (route.from_layer >= route.to_layer) continue;

    size_t src_x = route.from_x;
    size_t tgt_x = route.to_x;
    size_t src_y = nodeBottomY(route.from_node);
    size_t tgt_y = nodeTopY(route.to_node);

    // Route row is offset by channel
    size_t route_y = src_y + 1 + route.channel;

    // For skip-layer edges, check if vertical path is blocked
    bool skips_layers = (route.to_layer - route.from_layer) > 1;
    bool path_blocked = false;

    if (skips_layers) {
      // Check if target's vertical path goes through any boxes
      for (size_t y = route_y + 1; y < tgt_y; ++y) {
        if (isInsideBox(tgt_x, y)) {
          path_blocked = true;
          break;
        }
      }
    }

    if (src_x == tgt_x && !path_blocked) {
      // Simple straight vertical edge
      for (size_t y = src_y + 1; y < tgt_y; ++y) {
        if (!isInsideBox(src_x, y)) {
          mergeJunction(src_x, y, ch.vert);
        }
      }

      // Arrow
      if (cfg.show_arrows && tgt_y > 0) {
        canvas.put(tgt_x, tgt_y - 1, ch.arrow_down);
      }
    } else if (path_blocked) {
      // Need to route around boxes
      // Find clear vertical path for the long run
      size_t clear_x = findClearVerticalPath(std::min(src_x, tgt_x), route_y, tgt_y - 1);

      // Vertical from source to route row
      for (size_t y = src_y + 1; y < route_y; ++y) {
        mergeJunction(src_x, y, ch.vert);
      }

      // Route to clear_x - vertical from source box turns horizontal
      // Going right (src_x < clear_x): arms UP and RIGHT = └ (corner_bl)
      // Going left (src_x > clear_x): arms UP and LEFT = ┘ (corner_br)
      if (src_x != clear_x) {
        if (src_x < clear_x) {
          mergeJunction(src_x, route_y, ch.corner_bl);
        } else {
          mergeJunction(src_x, route_y, ch.corner_br);
        }

        size_t x1 = std::min(src_x, clear_x) + 1;
        size_t x2 = std::max(src_x, clear_x) - 1;
        for (size_t x = x1; x <= x2; ++x) {
          mergeJunction(x, route_y, ch.horiz);
        }

        // At clear_x, horizontal turns to vertical going DOWN
        // Coming from left (src_x < clear_x): arms LEFT and DOWN = ┐ (corner_tr)
        // Coming from right (src_x > clear_x): arms RIGHT and DOWN = ┌ (corner_tl)
        if (src_x < clear_x) {
          mergeJunction(clear_x, route_y, ch.corner_tr);
        } else {
          mergeJunction(clear_x, route_y, ch.corner_tl);
        }
      } else {
        mergeJunction(src_x, route_y, ch.vert);
      }

      // Vertical along clear path
      for (size_t y = route_y + 1; y < tgt_y - 1; ++y) {
        if (!isInsideBox(clear_x, y)) {
          mergeJunction(clear_x, y, ch.vert);
        }
      }

      // Final horizontal to target
      size_t final_y = tgt_y - 1;
      if (clear_x != tgt_x) {
        // At clear_x, edge turns from vertical (coming from above) to horizontal
        // Going right (clear_x < tgt_x): arms UP and RIGHT = └ (corner_bl)
        // Going left (clear_x > tgt_x): arms UP and LEFT = ┘ (corner_br)
        if (clear_x < tgt_x) {
          mergeJunction(clear_x, final_y, ch.corner_bl);
        } else {
          mergeJunction(clear_x, final_y, ch.corner_br);
        }

        size_t x1 = std::min(clear_x, tgt_x) + 1;
        size_t x2 = std::max(clear_x, tgt_x) - 1;
        for (size_t x = x1; x <= x2; ++x) {
          mergeJunction(x, final_y, ch.horiz);
        }

        // Edge turns DOWN to target
        // Edge comes from left (clear_x < tgt_x): horizontal from LEFT, vertical DOWN = ┐ (corner_tr)
        // Edge comes from right (clear_x > tgt_x): horizontal from RIGHT, vertical DOWN = ┌ (corner_tl)
        if (clear_x < tgt_x) {
          mergeJunction(tgt_x, final_y, ch.corner_tr);
        } else {
          mergeJunction(tgt_x, final_y, ch.corner_tl);
        }
      } else {
        mergeJunction(clear_x, final_y, ch.vert);
      }

      // Arrow
      if (cfg.show_arrows && tgt_y > 0) {
        canvas.put(tgt_x, tgt_y - 1, ch.arrow_down);
      }
    } else {
      // Regular edge with horizontal routing (src_x != tgt_x, path not blocked)
      // Vertical from source to route row
      for (size_t y = src_y + 1; y < route_y; ++y) {
        mergeJunction(src_x, y, ch.vert);
      }

      // Corner at route row (source side) - vertical from source box turns horizontal
      // Going right (src_x < tgt_x): arms UP (from source) and RIGHT = └ (corner_bl)
      // Going left (src_x > tgt_x): arms UP (from source) and LEFT = ┘ (corner_br)
      if (src_x < tgt_x) {
        mergeJunction(src_x, route_y, ch.corner_bl);
      } else {
        mergeJunction(src_x, route_y, ch.corner_br);
      }

      // Horizontal segment
      size_t x1 = std::min(src_x, tgt_x) + 1;
      size_t x2 = std::max(src_x, tgt_x) - 1;
      for (size_t x = x1; x <= x2; ++x) {
        mergeJunction(x, route_y, ch.horiz);
      }

      // Corner at target column - edge turns DOWN to target
      // Edge comes from left (src_x < tgt_x): horizontal from LEFT, vertical DOWN = ┐ (corner_tr)
      // Edge comes from right (src_x > tgt_x): horizontal from RIGHT, vertical DOWN = ┌ (corner_tl)
      if (src_x < tgt_x) {
        mergeJunction(tgt_x, route_y, ch.corner_tr);
      } else {
        mergeJunction(tgt_x, route_y, ch.corner_tl);
      }

      // Vertical to target
      for (size_t y = route_y + 1; y < tgt_y; ++y) {
        if (!isInsideBox(tgt_x, y)) {
          mergeJunction(tgt_x, y, ch.vert);
        }
      }

      // Arrow
      if (cfg.show_arrows && tgt_y > 0) {
        canvas.put(tgt_x, tgt_y - 1, ch.arrow_down);
      }
    }
  }

  // Post-process: fix box connection points where edges exit
  for (size_t i = 0; i < layout.nodes.size(); ++i) {
    const auto& node = layout.nodes[i];
    if (node.is_dummy) continue;

    size_t x = nodeCenterX(i);
    size_t bottom_y = nodeBottomY(i);
    size_t top_y = nodeTopY(i);

    // Check if there's an edge exiting downward
    bool has_outgoing = false;
    for (const auto& route : routes) {
      if (route.from_node == i && route.from_layer < route.to_layer) {
        has_outgoing = true;
        break;
      }
    }

    if (has_outgoing) {
      auto existing = canvas.get(x, bottom_y);
      if (existing == std::string(ch.horiz)) {
        canvas.put(x, bottom_y, ch.tee_down);
      }
    }

    // Check if there's an edge entering from above
    bool has_incoming = false;
    for (const auto& route : routes) {
      if (route.to_node == i && route.from_layer < route.to_layer) {
        has_incoming = true;
        break;
      }
    }

    if (has_incoming) {
      auto existing = canvas.get(x, top_y);
      if (existing == std::string(ch.horiz)) {
        canvas.put(x, top_y, ch.tee_up);
      }
    }
  }

  // Post-process: fix adjacent junctions that should be connected
  // If a junction with a RIGHT arm is followed by a corner, convert corner to T
  // Cache string conversions for efficiency
  const std::string str_tee_left(ch.tee_left);
  const std::string str_tee_up(ch.tee_up);
  const std::string str_tee_down(ch.tee_down);
  const std::string str_corner_bl(ch.corner_bl);
  const std::string str_corner_tl(ch.corner_tl);
  const std::string str_horiz(ch.horiz);
  const std::string str_cross(ch.cross);

  for (size_t y = 0; y < canvas.height(); ++y) {
    for (size_t x = 0; x + 1 < canvas.width(); ++x) {
      const auto& curr = canvas.get(x, y);
      const auto& next = canvas.get(x + 1, y);

      // Check if current has a RIGHT arm
      bool curr_has_right =
          curr == str_tee_left ||   // ├ (up/down/right)
          curr == str_tee_up ||     // ┴ (left/right/up)
          curr == str_tee_down ||   // ┬ (left/right/down)
          curr == str_corner_bl ||  // └ (up/right)
          curr == str_corner_tl ||  // ┌ (right/down)
          curr == str_horiz ||      // ─ (left/right)
          curr == str_cross;        // ┼ (all four)

      if (!curr_has_right) continue;

      // If next is a corner that should become a T-junction
      // ┌ (right/down) + horizontal from left → ┬ (left/right/down)
      if (next == str_corner_tl) {
        canvas.put(x + 1, y, ch.tee_down);
      }
    }
  }

  return canvas.render();
}

// =============================================================================
// Simple Text Rendering (for quick display)
// =============================================================================

inline auto renderSimple(const Dag& dag) -> std::string {
  std::string result;

  result += "DAG Structure:\n";
  result += std::string(40, '-') + "\n";

  result += "Nodes:\n";
  for (const auto& node : dag.nodes()) {
    result += "  ";
    result += node.observed ? "●" : "○";
    result += " " + node.label + "\n";
  }
  result += "\n";

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
    result += "  " + edge.from + " " + arrow + " " + edge.to;
    if (!edge.annotation.empty()) {
      result += " (" + edge.annotation + ")";
    }
    result += "\n";
  }

  return result;
}

// =============================================================================
// Compact Text Rendering
// =============================================================================

inline auto renderCompact(const Dag& dag) -> std::string {
  std::string result;

  for (const auto& node : dag.nodes()) {
    auto kids = dag.children(node.name);
    if (kids.empty()) continue;

    result += node.label;
    if (kids.size() == 1) {
      auto edge_it = std::ranges::find_if(dag.edges(), [&](const Edge& e) {
        return e.from == node.name && e.to == kids[0];
      });
      EdgeType type =
          (edge_it != dag.edges().end()) ? edge_it->type : EdgeType::kCausal;
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

// Alias for backwards compatibility
inline auto renderBox(const Dag& dag, const AsciiDagConfig& cfg = {})
    -> std::string {
  return renderAsciiDag(dag, cfg);
}

}  // namespace tempura::dag
