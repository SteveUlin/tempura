#include <print>
#include <string_view>

// CRTP
//
// C++ idiom: Curiously Recurring Template Pattern
//
// CRTP is a technique in C++ where a class derives from a template
// instantiation of itself. It is often used to achieve static polymorphism,
// code reuse, and to implement interfaces without the overhead of virtual
// functions.

// CRTP has been mostly supplanted by Concepts and `Deducing This` Mixins

template <typename Derived>
class Shape {
 public:
  auto asDerived() -> Derived& { return static_cast<Derived&>(*this); }

  void printName() { std::print("Shape name: {}\n", asDerived().name()); }

 private:
  // Disallow the direct construction of Shape<Derived>
  Shape() = default;

  // Only allow the Derived class to access the constructor of Shape<Derived>
  friend Derived;
};

class Circle : public Shape<Circle> {
 public:
  static auto name() -> std::string_view { return "Circle"; }
};

class Square : public Shape<Square> {
 public:
  static auto name() -> std::string_view { return "Square"; }
};

auto main() -> int {
  Circle circle;
  circle.printName();  // Output: Shape name: Circle

  Square square;
  square.printName();  // Output: Shape name: Square

  return EXIT_SUCCESS;
}
