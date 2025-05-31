#include <print>
#include <vector>
#include <memory>

// Cyclic Visitor
//
// The visitor pattern is a design pattern that lets you separate an algorithm
// from the objects on which it operates. It allows you to add new operations to
// existing object structures without modifying those structures.

// The Cyclic Visitor relies on inheritance to implement the visitor pattern.
// It is cyclic in that the Visitor abstract class needs to know about
// all of the possible shapes. The shapes, in turn, need to know about
// the Visitor class to accept it.

class Circle;
class Square;

class Visitor {
 public:
  virtual ~Visitor() = default;

  virtual void visit(const Circle& circle) const = 0;
  virtual void visit(const Square& square) const = 0;
};

class Shape {
 public:
  virtual ~Shape() = default;

  virtual void accept(const Visitor& visitor) = 0;
};

class Circle : public Shape {
 public:
  void accept(const Visitor& visitor) override {
    // By calling visit on the visitor, we allow it to perform the operation
    // for the current shape type.
    visitor.visit(*this);
  }
};

class Square : public Shape {
 public:
  void accept(const Visitor& visitor) override {
    visitor.visit(*this);
  }
};

class PrintVisitor : public Visitor {
 public:
  void visit(const Circle&) const override {
    std::println("Visiting Circle");
  }

  void visit(const Square&) const override {
    std::println("Visiting Square");
  }
};

auto main() -> int {
  std::vector<std::unique_ptr<Shape>> shapes;
  shapes.emplace_back(std::make_unique<Circle>());
  shapes.emplace_back(std::make_unique<Square>());
  shapes.emplace_back(std::make_unique<Circle>());
  shapes.emplace_back(std::make_unique<Square>());

  for (const auto& shape : shapes) {
    shape->accept(PrintVisitor{});
  }
}
