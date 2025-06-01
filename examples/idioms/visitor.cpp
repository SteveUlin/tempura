#include <print>
#include <vector>
#include <variant>

// Visitor
//
// The visitor pattern lets you define new functions to operate over a fixed
// set of classes.

// By defining a variant type, we can avoid the need for inheritance and
// the cyclic visitor pattern. The variant type can hold any of the
// shapes, and we can define visitor functions that operate on the variant.

class Circle {};
class Square {};

using Shape = std::variant<Circle, Square>;

class PrintVisitor {
 public:
  void operator()(const Circle&) const {
    std::println("Visiting Circle");
  }

  void operator()(const Square&) const {
    std::println("Visiting Square");
  }
};

auto main() -> int {
  std::vector<Shape> shapes = {Circle{}, Square{}, Circle{}, Square{}};

  for (const auto& shape : shapes) {
    std::visit(PrintVisitor{}, shape);
  }
}

