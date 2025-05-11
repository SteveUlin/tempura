#include "meta/expression.h"

#include <cassert>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Unary Plus"_test = [] {
    auto expr = +makeSymbol();
    expectEq(expr.getNode()->op, ExpressionOperator::kUnaryPlus);
    expectEq(expr.getNode()->unary_data.operand->op,
             ExpressionOperator::kVariable);
  };

  "Unary Minus"_test = [] {
    auto expr = -makeSymbol();
    expectEq(expr.getNode()->op, ExpressionOperator::kUnaryMinus);
    expectEq(expr.getNode()->unary_data.operand->op,
             ExpressionOperator::kVariable);
  };

  "Addition"_test = [] {
    auto expr = makeSymbol() + 1.0;
    expectEq(expr.getNode()->op, ExpressionOperator::kPlus);
    expectEq(expr.getNode()->binary_data.left->op,
             ExpressionOperator::kVariable);
    expectEq(expr.getNode()->binary_data.right->double_value, 1.0);
  };

  "Subtraction"_test = [] {
    auto expr = makeSymbol() - 1.0;
    expectEq(expr.getNode()->op, ExpressionOperator::kMinus);
    expectEq(expr.getNode()->binary_data.left->op,
             ExpressionOperator::kVariable);
    expectEq(expr.getNode()->binary_data.right->double_value, 1.0);
  };

  "Multiplication"_test = [] {
    auto expr = makeSymbol() * 1.0;
    expectEq(expr.getNode()->op, ExpressionOperator::kMultiply);
    expectEq(expr.getNode()->binary_data.left->op,
             ExpressionOperator::kVariable);
    expectEq(expr.getNode()->binary_data.right->double_value, 1.0);
  };

  "Division"_test = [] {
    auto expr = makeSymbol() / 1.0;
    expectEq(expr.getNode()->op, ExpressionOperator::kDivide);
    expectEq(expr.getNode()->binary_data.left->op,
             ExpressionOperator::kVariable);
    expectEq(expr.getNode()->binary_data.right->double_value, 1.0);
  };

  "Power"_test = [] {
    auto expr = pow(makeSymbol(), 2.0);
    expectEq(expr.getNode()->op, ExpressionOperator::kPower);
    expectEq(expr.getNode()->binary_data.left->op,
             ExpressionOperator::kVariable);
    expectEq(expr.getNode()->binary_data.right->double_value, 2.0);
  };


  "Sqrt"_test = [] {
    auto expr = sqrt(makeSymbol());
    expectEq(expr.getNode()->op, ExpressionOperator::kSqrt);
    expectEq(expr.getNode()->unary_data.operand->op,
             ExpressionOperator::kVariable);
  };

  "evaluation"_test = []{
    auto one = Expression(1);
    expectEq(eval(one), 1.0);
    expectEq(eval(one + one + one), 3.0);
  };

}
