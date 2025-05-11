#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <vector>
#include <cmath>

namespace tempura {

// An Expression is an abstract representation of some computation. There are
// operator and function overloads for creating expressions, but there is
// no additional meaning assigned to the expression. You must use visitor
// pattern to evaluate the expression or convert it to a string.
//
// ExpressionNodes are always immutable const shared pointers. This is to
// allow for easy sharing of subexpressions and to avoid copying large
// expressions. The Expression class is a thin wrapper around the
// ExpressionNode.

enum class ExpressionOperator : uint8_t {
  // Some placeholder to be substituted with a value on evaluation
  // x in `x + 1`
  kVariable,

  // Constants that store some data in the node
  kDouble,
  kInt,
  // A storage type for higher order functions that take other functions
  // as input. For example kFoldLeft {kPlus, 1, x, 3}
  kExpressionOperator,

  // Unary operators
  kUnaryPlus,  // Must be the first unary operator
  kUnaryMinus,

  // Unary functions
  kSqrt,

  // Binary operators
  kPlus,  // Must be the first binary operator
  kMinus,
  kMultiply,
  kDivide,
  kPower,

  // Higher order functions
  kFoldLeft,  // Must be the first higher order function
  kFoldRight,
};

enum class ExpressionStorageType : uint8_t {
  kTrival,
  kUnary,
  kBinary,
  kVector,
};

constexpr auto getStorageType(ExpressionOperator op) -> ExpressionStorageType {
  if (op >= ExpressionOperator::kFoldLeft) {
    return ExpressionStorageType::kVector;
  }
  if (op >= ExpressionOperator::kPlus) {
    return ExpressionStorageType::kBinary;
  }
  if (op >= ExpressionOperator::kUnaryPlus) {
    return ExpressionStorageType::kUnary;
  }
  return ExpressionStorageType::kTrival;
}

struct ExpressionNode {
  struct UnaryData {
    std::shared_ptr<const ExpressionNode> operand;
  };
  struct BinaryData {
    std::shared_ptr<const ExpressionNode> left;
    std::shared_ptr<const ExpressionNode> right;
  };
  using VectorData = std::vector<std::shared_ptr<const ExpressionNode>>;

  ExpressionOperator op;

  union {
    double double_value;
    int64_t int_value;
    ExpressionOperator operator_value;
    std::monostate monostate_value;
    UnaryData unary_data;
    BinaryData binary_data;
    VectorData vector_data;
  };

  ExpressionNode() = delete;
  ExpressionNode(const ExpressionNode&) = delete;
  ExpressionNode(ExpressionNode&&) = delete;
  auto operator=(const ExpressionNode&) -> ExpressionNode& = delete;
  auto operator=(ExpressionNode&&) -> ExpressionNode& = delete;

  constexpr explicit ExpressionNode(double value)
      : op{ExpressionOperator::kDouble}, double_value{value} {}

  constexpr explicit ExpressionNode(int64_t value)
      : op{ExpressionOperator::kInt}, int_value{value} {}

  constexpr explicit ExpressionNode(ExpressionOperator value)
      : op{ExpressionOperator::kExpressionOperator}, operator_value{value} {}

  constexpr explicit ExpressionNode(std::monostate value)
      : op{ExpressionOperator::kVariable}, monostate_value{value} {}

  // Precondition:
  //   The ExpressionType must have the correct type for the data

  constexpr ExpressionNode(ExpressionOperator expr_op, UnaryData value)
      : op{expr_op}, unary_data{std::move(value)} {}

  constexpr ExpressionNode(ExpressionOperator expr_op, BinaryData value)
      : op{expr_op}, binary_data{std::move(value)} {}

  constexpr ExpressionNode(ExpressionOperator expr_op, VectorData value)
      : op{expr_op}, vector_data{std::move(value)} {}

  constexpr ~ExpressionNode() {
    switch (ExpressionStorageType(op)) {
      case ExpressionStorageType::kTrival:
        break;
      case ExpressionStorageType::kUnary:
        unary_data.~UnaryData();
        break;
      case ExpressionStorageType::kBinary:
        binary_data.~BinaryData();
        break;
      case ExpressionStorageType::kVector:
        vector_data.~VectorData();
        break;
    }
  }
};

class Expression {
 public:
  using NodePtr = std::shared_ptr<const ExpressionNode>;

  constexpr explicit Expression(NodePtr node) : node_(std::move(node)) {}

  // Constants can auto convert to expressions
  constexpr Expression(double value)
      : node_(std::make_shared<const ExpressionNode>(value)) {}
  constexpr Expression(int value)
      : node_(std::make_shared<const ExpressionNode>(
            static_cast<int64_t>(value))) {}
  constexpr Expression(ExpressionOperator value)
      : node_(std::make_shared<const ExpressionNode>(value)) {}

  constexpr auto getOperator() const -> ExpressionOperator { return node_->op; }

  constexpr auto getNode() const -> NodePtr { return node_; }

 private:
  NodePtr node_;
};

constexpr auto makeSymbol() -> Expression {
  return Expression(
      std::make_shared<const ExpressionNode>(std::monostate{}));
}

constexpr auto operator+(const Expression& expr) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kUnaryPlus,
      ExpressionNode::UnaryData{.operand = expr.getNode()}));
}

constexpr auto operator-(const Expression& expr) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kUnaryMinus,
      ExpressionNode::UnaryData{.operand = expr.getNode()}));
}

constexpr auto sqrt(const Expression& expr) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kSqrt,
      ExpressionNode::UnaryData{.operand = expr.getNode()}));
}

constexpr auto operator+(const Expression& lhs, const Expression& rhs) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kPlus,
      ExpressionNode::BinaryData{.left = lhs.getNode(),
                                 .right = rhs.getNode()}));
}

constexpr auto operator-(const Expression& lhs, const Expression& rhs) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kMinus,
      ExpressionNode::BinaryData{.left = lhs.getNode(),
                                 .right = rhs.getNode()}));
}

constexpr auto operator*(const Expression& lhs, const Expression& rhs) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kMultiply,
      ExpressionNode::BinaryData{.left = lhs.getNode(),
                                 .right = rhs.getNode()}));
}

constexpr auto operator/(const Expression& lhs, const Expression& rhs) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kDivide,
      ExpressionNode::BinaryData{.left = lhs.getNode(),
                                 .right = rhs.getNode()}));
}

constexpr auto pow(const Expression& lhs, const Expression& rhs) -> Expression {
  return Expression(std::make_shared<const ExpressionNode>(
      ExpressionOperator::kPower,
      ExpressionNode::BinaryData{.left = lhs.getNode(),
                                 .right = rhs.getNode()}));
}

// Eval assuming every leaf node is a constant
constexpr auto eval(const Expression& expr) -> double {
  switch (expr.getOperator()) {
    case ExpressionOperator::kDouble:
      return expr.getNode()->double_value;
    case ExpressionOperator::kInt:
      return static_cast<double>(expr.getNode()->int_value);
    case ExpressionOperator::kUnaryPlus:
      return +eval(Expression(expr.getNode()->unary_data.operand));
    case ExpressionOperator::kUnaryMinus:
      return -eval(Expression(expr.getNode()->unary_data.operand));
    case ExpressionOperator::kSqrt:
      using std::sqrt;
      return sqrt(eval(Expression(expr.getNode()->unary_data.operand)));
    case ExpressionOperator::kPlus:
      return eval(Expression(expr.getNode()->binary_data.left)) +
             eval(Expression(expr.getNode()->binary_data.right));
    case ExpressionOperator::kMinus:
      return eval(Expression(expr.getNode()->binary_data.left)) -
             eval(Expression(expr.getNode()->binary_data.right));
    case ExpressionOperator::kMultiply:
      return eval(Expression(expr.getNode()->binary_data.left)) *
             eval(Expression(expr.getNode()->binary_data.right));
    case ExpressionOperator::kDivide:
      return eval(Expression(expr.getNode()->binary_data.left)) /
             eval(Expression(expr.getNode()->binary_data.right));
    case ExpressionOperator::kPower:
      using std::pow;
      return pow(eval(Expression(expr.getNode()->binary_data.left)),
                 eval(Expression(expr.getNode()->binary_data.right)));
    default:
      assert(false && "Unsupported expression type");
  }
  return 0.0;  // Unreachable
}


}  // namespace tempura
