#include <functional>
#include <print>
#include <utility>

// Strategy
//
// The strategy pattern lets you define new implementations of a specific
// algorithm without changing the code that uses it.

class Shape {
 public:
  virtual ~Shape() = default;
  virtual void print() const = 0;
};

class Circle : public Shape {
 public:
  using PritStrategy = std::function<void(const Circle&)>;

  Circle(PritStrategy print_strategy)
      : print_strategy_(std::move(print_strategy)) {}

  void print() const override {
    print_strategy_(*this);
  }

 private:
  PritStrategy print_strategy_;
};

class Square : public Shape {
 public:
  using PrintStrategy = std::function<void(const Square&)>;

  Square(PrintStrategy print_strategy)
      : print_strategy_(std::move(print_strategy)) {}

  void print() const override {
    print_strategy_(*this);
  }

 private:
  PrintStrategy print_strategy_;
};

class CirclePrinter {
 public:
  void operator()(const Circle&) const {
    std::println("Visiting Circle");
  }
};

class SquarePrinter {
 public:
  void operator()(const Square&) const {
    std::println("Visiting Square");
  }
};

auto main() -> int {
  Circle circle{CirclePrinter{}};
  Square square{SquarePrinter{}};

  circle.print();
  square.print();

  // We can change the print strategy at runtime
  circle = Circle{[](const Circle&) { std::println("Custom Circle Print"); }};
  square = Square{[](const Square&) { std::println("Custom Square Print"); }};

  circle.print();
  square.print();
}

