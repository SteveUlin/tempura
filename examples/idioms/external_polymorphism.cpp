#include <memory>
#include <print>
#include <utility>
#include <vector>

// --- Problem Context: Existing Unrelated Classes ---
//
// Imagine these classes come from different libraries or legacy code.
// They perform similar conceptual actions (e.g., representing a shape that can
// be displayed) but do not share a common interface, and we cannot (or choose
// not to) modify them to introduce one directly (e.g., by making them inherit
// from a common 'Shape' base class).

class Circle {
 public:
  explicit Circle(double radius) : radius_(radius) {}

  // Original method for displaying a Circle. Note the name is specific to
  // Circle.
  void display() const { std::println("Circle with radius: {}", radius_); }

  auto getRadius() const -> double { return radius_; }

 private:
  double radius_;
};

class Square {
 public:
  explicit Square(double side) : side_(side) {}

  // Original method for displaying a Square. Note the name is specific to
  // Square.
  void render() const { std::println("Square with side length: {}", side_); }

  auto getSide() const -> double { return side_; }

 private:
  double side_;
};

// --- External Polymorphism Implementation ---
//
// The goal is to treat Circle and Square objects polymorphically (e.g., call a
// 'draw' method on them) without modifying their original definitions.

class Drawable {
 public:
  virtual ~Drawable() = default;

  virtual void draw() const = 0;
};

template <typename T>
class DrawableAdapter : public Drawable {
 public:
  explicit DrawableAdapter(T shape) : shape_(std::move(shape)) {}

  void draw() const override;

 private:
  T shape_;
};

template <>
void DrawableAdapter<Circle>::draw() const {
  shape_.display();
}

template <>
void DrawableAdapter<Square>::draw() const {
  shape_.render();
}

auto main() -> int {
  std::vector<std::unique_ptr<Drawable>> drawables;

  drawables.push_back(std::make_unique<DrawableAdapter<Circle>>(Circle(5.0)));
  drawables.push_back(std::make_unique<DrawableAdapter<Square>>(Square(3.0)));
  drawables.push_back(std::make_unique<DrawableAdapter<Circle>>(Circle(10.2)));

  for (const auto& drawable_ptr : drawables) {
    drawable_ptr->draw();
  }

  return EXIT_SUCCESS;
}
