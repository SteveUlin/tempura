#include <array>
#include <cstddef>
#include <memory>
#include <print>

// SBO Type Erasure Example
//
// This example demonstrates how to implement type erasure in a way that allows
// you to directly construct the underlying type.

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

template <std::size_t Capacity = 32U,
          std::size_t Alignment = alignof(std::max_align_t)>
class Shape {
 public:
  template <typename ShapeT, typename DrawStrategy>
  Shape(ShapeT shape, DrawStrategy drawStrategy) {
    using Model = Model<ShapeT, DrawStrategy>;

    static_assert(
        sizeof(Model) <= Capacity,
        "Model size exceeds the capacity of the Shape type erasure buffer.");

    static_assert(alignof(Model) <= Alignment,
                  "Model alignment exceeds the capacity of the Shape type "
                  "erasure buffer.");

    std::construct_at(static_cast<Model*>(impl()), std::move(shape),
                      std::move(drawStrategy));
  }

  Shape(const Shape& other) { other.impl()->clone(impl()); }

  Shape(Shape&& other) noexcept { other.impl()->move(impl()); }

  auto operator=(const Shape& other) -> Shape& {
    Shape temp(other);
    std::swap(*this, temp);
    return *this;
  }

  auto operator=(Shape&& other) noexcept -> Shape& {
    Shape temp(std::move(other));
    std::swap(*this, temp);
    return *this;
  }

  ~Shape() { std::destroy_at(impl()); }

 private:
  friend void draw(Shape& shape) { shape.impl()->draw(); }

  struct Concept {
    virtual ~Concept() = default;

    virtual void draw() const = 0;
    virtual void clone(Concept* memory) const = 0;
    virtual void move(Concept* memory) = 0;
  };

  template <typename ShapeT, typename DrawStrategy>
  struct Model : public Concept {
    explicit Model(ShapeT shape, DrawStrategy drawStrategy)
        : shape_(std::move(shape)), drawStrategy_(std::move(drawStrategy)) {}

    void draw() const override { drawStrategy_(shape_); }

    void clone(Concept* memory) const override {
      std::construct_at(static_cast<Model*>(memory), *this);
    }

    void move(Concept* memory) override {
      std::construct_at(static_cast<Model*>(memory), std::move(*this));
    }

   private:
    ShapeT shape_;
    DrawStrategy drawStrategy_;
  };

  auto impl() -> Concept* { return reinterpret_cast<Concept*>(&buffer_); }

  auto impl() const -> const Concept* {
    return reinterpret_cast<const Concept*>(&buffer_);
  }

  alignas(Alignment) std::array<std::byte, Capacity> buffer_;
};

auto main() -> int {
  // Example usage of Shape type erasure
  Shape shape(Circle{5.0}, [](const Circle& circle) {
    std::print("Drawing: Circle with radius {}\n", circle.getRadius());
  });

  draw(shape);  // Output: Drawing: Circle

  return EXIT_SUCCESS;
}
