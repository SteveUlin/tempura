// Tests for operators.h - symbolic operator overloads
#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  Symbol<struct X> x;
  Symbol<struct Y> y;

  "binary arithmetic operators"_test = [&] {
    auto add = x + y;
    static_assert(is_expression_v<decltype(add)>);
    static_assert(std::is_same_v<get_op_t<decltype(add)>, AddOp>);
    static_assert(decltype(add)::arity == 2);

    auto sub = x - y;
    static_assert(std::is_same_v<get_op_t<decltype(sub)>, SubOp>);

    auto mul = x * y;
    static_assert(std::is_same_v<get_op_t<decltype(mul)>, MulOp>);

    auto div = x / y;
    static_assert(std::is_same_v<get_op_t<decltype(div)>, DivOp>);
  };

  "unary negation"_test = [&] {
    auto neg = -x;
    static_assert(is_expression_v<decltype(neg)>);
    static_assert(std::is_same_v<get_op_t<decltype(neg)>, NegOp>);
    static_assert(decltype(neg)::arity == 1);
  };

  "trigonometric functions"_test = [&] {
    auto s = sin(x);
    static_assert(is_expression_v<decltype(s)>);
    static_assert(std::is_same_v<get_op_t<decltype(s)>, SinOp>);

    auto c = cos(x);
    static_assert(std::is_same_v<get_op_t<decltype(c)>, CosOp>);

    auto t = tan(x);
    static_assert(std::is_same_v<get_op_t<decltype(t)>, TanOp>);
  };

  "hyperbolic functions"_test = [&] {
    auto sh = sinh(x);
    static_assert(std::is_same_v<get_op_t<decltype(sh)>, SinhOp>);

    auto ch = cosh(x);
    static_assert(std::is_same_v<get_op_t<decltype(ch)>, CoshOp>);

    auto th = tanh(x);
    static_assert(std::is_same_v<get_op_t<decltype(th)>, TanhOp>);
  };

  "exponential and logarithmic"_test = [&] {
    auto ex = exp(x);
    static_assert(std::is_same_v<get_op_t<decltype(ex)>, ExpOp>);

    auto lg = log(x);
    static_assert(std::is_same_v<get_op_t<decltype(lg)>, LogOp>);

    auto sq = sqrt(x);
    static_assert(std::is_same_v<get_op_t<decltype(sq)>, SqrtOp>);
  };

  "power function"_test = [&] {
    auto p = pow(x, y);
    static_assert(is_expression_v<decltype(p)>);
    static_assert(std::is_same_v<get_op_t<decltype(p)>, PowOp>);
    static_assert(decltype(p)::arity == 2);
  };

  "nested expressions"_test = [&] {
    auto expr = (x + y) * x;
    static_assert(is_expression_v<decltype(expr)>);
    static_assert(std::is_same_v<get_op_t<decltype(expr)>, MulOp>);

    auto lhs = expr.arg<0>();
    static_assert(is_expression_v<decltype(lhs)>);
    static_assert(std::is_same_v<get_op_t<decltype(lhs)>, AddOp>);
  };

  "mixed symbol and constant operations"_test = [&] {
    auto expr = x + Constant<5>{};
    static_assert(is_expression_v<decltype(expr)>);
    static_assert(std::is_same_v<get_op_t<decltype(expr)>, AddOp>);

    auto lhs = expr.arg<0>();
    auto rhs = expr.arg<1>();
    static_assert(is_atom_v<decltype(lhs)>);
    static_assert(is_constant_v<decltype(rhs)>);
  };

  "mathematical constants pi and e"_test = [] {
    static_assert(is_expression_v<decltype(pi)>);
    static_assert(std::is_same_v<get_op_t<decltype(pi)>, PiOp>);
    static_assert(decltype(pi)::arity == 0);

    static_assert(is_expression_v<decltype(e)>);
    static_assert(std::is_same_v<get_op_t<decltype(e)>, EOp>);
    static_assert(decltype(e)::arity == 0);
  };

  return TestRegistry::result();
}
