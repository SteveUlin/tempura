#include <array>
#include <cstddef>
#include <memory>
#include <print>

// Manual Dispatch Type Erasure

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

class Shape {
 public:
  template <typename ShapeT, typename DrawStrategy>
  Shape(ShapeT shape, DrawStrategy drawStrategy)
      : impl_(new Model<ShapeT, DrawStrategy>(std::move(shape),
                                              std::move(drawStrategy)),
              [](void* ptr) {
                delete static_cast<Model<ShapeT, DrawStrategy>*>(ptr);
              }),
        cloneFunc_([](void* ptr) -> void* {
          return new Model<ShapeT, DrawStrategy>(
              *static_cast<Model<ShapeT, DrawStrategy>*>(ptr));
        }),
        drawFunc_([](void* ptr) {
          auto model = static_cast<Model<ShapeT, DrawStrategy>*>(ptr);
          model->drawStrategy_(model->shape_);
        }) {}

  Shape(const Shape& other)
      : impl_(other.impl_->clone()),
        cloneFunc_(other.cloneFunc_),
        drawFunc_(other.drawFunc_) {}

  void draw() const {
    drawFunc_(impl_.get());
  }

 private:
  template <typename ShapeT, typename DrawStrategy>
  struct Model {
    explicit Model(ShapeT shape, DrawStrategy drawStrategy)
        : shape_(std::move(shape)), drawStrategy_(std::move(drawStrategy)) {}

    auto clone() -> std::unique_ptr<Model> {
      return std::make_unique<Model<ShapeT, DrawStrategy>>(*this);
    }

    ShapeT shape_;
    DrawStrategy drawStrategy_;
  };

  using DestroyFunc = void(void*);
  using CloneFunc = void*(void*);
  using DrawFunc = void(void*);

  std::unique_ptr<void, DestroyFunc*> impl_;
  CloneFunc* cloneFunc_;
  DrawFunc* drawFunc_;
};
