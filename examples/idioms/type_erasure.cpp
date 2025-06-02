#include <concepts>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>

// Type Erasure
//
// Type erasure is a technique where you "erase" the specific type of an
// object at compile time an manage it though a common interface.
//
// For example std::function and std::any
//
// This allows you to work with loosely coupled types as if they were the
// same type, while still retaining the ability to call their underlying
// methods.

// We start with a couple of classes that know nothing about the type erasure
// interface.

class Circle {
 public:
  explicit Circle(double radius) : radius_(radius) {}

  auto getRadius() const -> double { return radius_; }

 private:
  double radius_;
};

class Square {
 public:
  explicit Square(double side) : side_(side) {}

  auto getSide() const -> double { return side_; }

 private:
  double side_;
};

// The "Concept" interface that defines the operations we want to
// perform on our types.

class ShapeConcept {
 public:
  virtual ~ShapeConcept() = default;

  virtual auto draw() const -> void = 0;

  // Clone enables us to implement value semantics for our type-erased
  // objects.
  virtual auto clone() const -> std::unique_ptr<ShapeConcept> = 0;
};

// The "Model" defines how to implement the draw method
template <typename ShapeT, typename DrawStrategy>
class ShapeModel : public ShapeConcept {
 public:
  explicit ShapeModel(ShapeT shape, DrawStrategy drawStrategy)
      : shape_(std::move(shape)), drawStrategy_(std::move(drawStrategy)) {}

  auto draw() const -> void override { drawStrategy_(shape_); }

  auto clone() const -> std::unique_ptr<ShapeConcept> override {
    return std::make_unique<ShapeModel<ShapeT, DrawStrategy>>(*this);
  }

 private:
  ShapeT shape_;
  DrawStrategy drawStrategy_;
};

// The Type Erasure class that holds a pointer to a generic ShapeConcept.
class Shape {
 public:
  template <typename ShapeT, typename DrawStrategy>
  explicit Shape(ShapeT shape, DrawStrategy drawStrategy)
      : Shape{std::make_unique<ShapeModel<ShapeT, DrawStrategy>>(
            std::move(shape), std::move(drawStrategy))} {}

  // Delegates the draw call to the underlying concept.
  auto draw() const -> void { impl_->draw(); }

  // Enables value semantics for the type-erased object.
  auto clone() const -> Shape { return Shape(impl_->clone()); }

 private:
  explicit Shape(std::unique_ptr<ShapeConcept> impl) : impl_(std::move(impl)) {}

  std::unique_ptr<ShapeConcept> impl_;
};

auto main() -> int {
  // Create a Circle and a Square with their respective draw strategies.
  Circle circle(5.0);
  auto circleDrawStrategy = [](const Circle& c) {
    std::print("Drawing Circle with radius: {}\n", c.getRadius());
  };

  Square square(4.0);
  auto squareDrawStrategy = [](const Square& s) {
    std::print("Drawing Square with side length: {}\n", s.getSide());
  };

  // Create type-erased shapes.
  Shape shapeCircle(circle, std::move(circleDrawStrategy));
  Shape shapeSquare(square, std::move(squareDrawStrategy));

  // Draw the shapes.
  shapeCircle.draw();
  shapeSquare.draw();

  return EXIT_SUCCESS;
}
