// Demo: toString output for various symbolic expressions
#include <iostream>

#include "symbolic4/core.h"
#include "symbolic4/interpreter/to_string.h"
#include "symbolic4/operators.h"

using namespace tempura::symbolic4;

auto main() -> int {
  Symbol<struct X> x;
  Symbol<struct Y> y;

  std::cout << "=== Constants ===\n";
  std::cout << "42:        " << toString(Constant<42>{}) << "\n";
  std::cout << "-7:        " << toString(Constant<-7>{}) << "\n";
  std::cout << "1/2:       " << toString(Fraction<1, 2>{}) << "\n";
  std::cout << "3/4:       " << toString(Fraction<3, 4>{}) << "\n";
  std::cout << "pi:        " << toString(pi) << "\n";
  std::cout << "e:         " << toString(e) << "\n";

  std::cout << "\n=== Symbols ===\n";
  std::cout << "x:         " << toString(x, name(x, "x")) << "\n";
  std::cout << "y:         " << toString(y, name(y, "y")) << "\n";

  std::cout << "\n=== Binary Operations ===\n";
  std::cout << "x + y:     " << toString(x + y, name(x, "x"), name(y, "y")) << "\n";
  std::cout << "x - y:     " << toString(x - y, name(x, "x"), name(y, "y")) << "\n";
  std::cout << "x * y:     " << toString(x * y, name(x, "x"), name(y, "y")) << "\n";
  std::cout << "x / y:     " << toString(x / y, name(x, "x"), name(y, "y")) << "\n";
  std::cout << "x ^ 3:     " << toString(pow(x, Constant<3>{}), name(x, "x")) << "\n";

  std::cout << "\n=== Unary Operations ===\n";
  std::cout << "-x:        " << toString(-x, name(x, "x")) << "\n";
  std::cout << "sin(x):    " << toString(sin(x), name(x, "x")) << "\n";
  std::cout << "cos(x):    " << toString(cos(x), name(x, "x")) << "\n";
  std::cout << "tan(x):    " << toString(tan(x), name(x, "x")) << "\n";
  std::cout << "exp(x):    " << toString(exp(x), name(x, "x")) << "\n";
  std::cout << "log(x):    " << toString(log(x), name(x, "x")) << "\n";
  std::cout << "sqrt(x):   " << toString(sqrt(x), name(x, "x")) << "\n";

  std::cout << "\n=== Precedence (parentheses) ===\n";
  std::cout << "x + y * x:       " << toString(x + y * x, name(x, "x"), name(y, "y")) << "\n";
  std::cout << "(x + y) * x:     " << toString((x + y) * x, name(x, "x"), name(y, "y")) << "\n";
  std::cout << "x * y + x:       " << toString(x * y + x, name(x, "x"), name(y, "y")) << "\n";
  std::cout << "x - (y - x):     " << toString(x - (y - x), name(x, "x"), name(y, "y")) << "\n";
  std::cout << "x / (y / x):     " << toString(x / (y / x), name(x, "x"), name(y, "y")) << "\n";

  std::cout << "\n=== Nested Expressions ===\n";
  std::cout << "sin(x^2):        " << toString(sin(x * x), name(x, "x")) << "\n";
  std::cout << "sin(x^2 + 1):    " << toString(sin(x * x + Constant<1>{}), name(x, "x")) << "\n";
  std::cout << "exp(sin(x)):     " << toString(exp(sin(x)), name(x, "x")) << "\n";
  std::cout << "log(1 + x^2):    " << toString(log(Constant<1>{} + x * x), name(x, "x")) << "\n";

  std::cout << "\n=== Polynomials ===\n";
  // x^2 + 2x + 1
  auto poly = x * x + Constant<2>{} * x + Constant<1>{};
  std::cout << "x^2 + 2x + 1:    " << toString(poly, name(x, "x")) << "\n";

  // x^3 - 3x^2 + 3x - 1
  auto cubic = x * x * x - Constant<3>{} * x * x + Constant<3>{} * x - Constant<1>{};
  std::cout << "x^3 - 3x^2 + 3x - 1:  " << toString(cubic, name(x, "x")) << "\n";

  std::cout << "\n=== Complex Expressions ===\n";
  // sin(x) * cos(y) + cos(x) * sin(y) = sin(x + y)
  auto sum_formula = sin(x) * cos(y) + cos(x) * sin(y);
  std::cout << "sin(x)cos(y) + cos(x)sin(y):  " << toString(sum_formula, name(x, "x"), name(y, "y")) << "\n";

  // e^x / (1 + e^x) = sigmoid
  auto sigmoid = exp(x) / (Constant<1>{} + exp(x));
  std::cout << "e^x / (1 + e^x):  " << toString(sigmoid, name(x, "x")) << "\n";

  // sqrt(x^2 + y^2) = distance
  auto distance = sqrt(x * x + y * y);
  std::cout << "sqrt(x^2 + y^2):  " << toString(distance, name(x, "x"), name(y, "y")) << "\n";

  std::cout << "\n=== Greek Letters ===\n";
  Symbol<struct Alpha> alpha;
  Symbol<struct Beta> beta;
  Symbol<struct Theta> theta;
  auto expr = alpha * sin(theta) + beta * cos(theta);
  std::cout << "α*sin(θ) + β*cos(θ):  " << toString(expr, name(alpha, "α"), name(beta, "β"), name(theta, "θ")) << "\n";

  return 0;
}
