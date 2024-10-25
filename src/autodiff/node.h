#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace tempura::autodiff {

template <typename T>
class Node {
 public:
  virtual ~Node() = default;

  virtual auto value() -> const T& = 0;

  virtual void clearValue() = 0;

  virtual void propagate(const T& derivative) = 0;

  virtual void propagateNode(std::shared_ptr<Node> node) = 0;
};
template <typename T>
class IndependentVariable : public Node<T> {
 public:
  static auto make(auto&& value) {
    return std::shared_ptr<IndependentVariable>(
        new IndependentVariable(std::forward<decltype(value)>(value)));
  }

  auto value() -> const T& override { return value_.value(); }

  // Do nothing as IndependentVariables represent constants in the expression
  // and we are not going to rebind data to them
  void clearValue() override {}

  void propagate(const T& /*unused*/) override {}

  void propagateNode(std::shared_ptr<Node<T>> /*unused*/) override {}

 private:
  IndependentVariable() = default;
  IndependentVariable(T forward) : value_{std::move(forward)} {}

  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class PlusNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> left,
                   std::shared_ptr<Node<T>> right) {
    return std::shared_ptr<PlusNode>(
        new PlusNode(std::move(left), std::move(right)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      value_.emplace(left_->value() + right_->value());
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    left_->clearValue();
    right_->clearValue();
  }

  void propagate(const T& derivative) override {
    left_->propagate(derivative);
    right_->propagate(derivative);
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    left_->propagateNode(node);
    right_->propagateNode(node);
  }

 private:
  PlusNode(std::shared_ptr<Node<T>> left, std::shared_ptr<Node<T>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<T>> left_;
  std::shared_ptr<Node<T>> right_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class PosNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr{new PlusNode(std::move(node))};
  }

 private:
  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class NegateNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr<NegateNode>(new NegateNode(std::move(node)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      value_.emplace(-node_->value());
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    node_->clearValue();
  }

  void propagate(const T& derivative) override {
    node_->propagate(-derivative);
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    node_->propagateNode(NegateNode<T>::make(node));
  }

 private:
  NegateNode(std::shared_ptr<Node<T>> node) : node_{std::move(node)} {}

  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class MinusNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> left,
                   std::shared_ptr<Node<T>> right) {
    return std::shared_ptr<Node<T>>(
        new MinusNode<T>(std::move(left), std::move(right)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      value_.emplace(left_->value() - right_->value());
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    left_->clearValue();
    right_->clearValue();
  }

  void propagate(const T& derivative) override {
    left_->propagate(derivative);
    right_->propagate(-derivative);
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    left_->propagateNode(node);
    right_->propagateNode(NegateNode<T>::make(node));
  }

 private:
  MinusNode(std::shared_ptr<Node<T>> left, std::shared_ptr<Node<T>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<T>> left_;
  std::shared_ptr<Node<T>> right_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class MultipliesNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> left,
                   std::shared_ptr<Node<T>> right) {
    return std::shared_ptr<MultipliesNode>(
        new MultipliesNode(std::move(left), std::move(right)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      value_.emplace(left_->value() * right_->value());
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    left_->clearValue();
    right_->clearValue();
  }

  void propagate(const T& derivative) override {
    left_->propagate(derivative * right_->value());
    right_->propagate(derivative * left_->value());
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    left_->propagateNode(MultipliesNode<T>::make(node, right_));
    right_->propagateNode(MultipliesNode<T>::make(node, left_));
  }

 private:
  MultipliesNode(std::shared_ptr<Node<T>> left, std::shared_ptr<Node<T>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<T>> left_;
  std::shared_ptr<Node<T>> right_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class DividesNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> left,
                   std::shared_ptr<Node<T>> right) {
    return std::shared_ptr<DividesNode>(
        new DividesNode(std::move(left), std::move(right)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      value_.emplace(left_->value() / right_->value());
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    left_->clearValue();
    right_->clearValue();
  }

  void propagate(const T& derivative) override {
    left_->propagate(derivative / right_->value());
    right_->propagate(-derivative * left_->value() /
                      (right_->value() * right_->value()));
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    left_->propagateNode(DividesNode<T>::make(node, right_));
    right_->propagateNode(NegateNode<T>::make(
        DividesNode<T>::make(MultipliesNode<T>::make(node, left_),
                             MultipliesNode<T>::make(right_, right_))));
  }

 private:
  DividesNode(std::shared_ptr<Node<T>> left, std::shared_ptr<Node<T>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<T>> left_;
  std::shared_ptr<Node<T>> right_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class SqrtNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr<SqrtNode>(new SqrtNode(std::move(node)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      using std::sqrt;
      value_.emplace(sqrt(node_->value()));
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    node_->clearValue();
  }

  void propagate(const T& derivative) override {
    using std::sqrt;
    node_->propagate(derivative / (T{2.} * sqrt(node_->value())));
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    node_->propagateNode(MultipliesNode<T>::make(
        DividesNode<T>::make(node, IndependentVariable<T>::make(T{2.})),
        SqrtNode<T>::make(node_)));
  }

 private:
  SqrtNode(std::shared_ptr<Node<T>> node) : node_{std::move(node)} {}

  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class LogNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr<LogNode>(new LogNode(std::move(node)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      using std::log;
      value_.emplace(log(node_->value()));
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    node_->clearValue();
  }

  void propagate(const T& derivative) override {
    node_->propagate(derivative / node_->value());
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    node_->propagateNode(DividesNode<T>::make(node, node_));
  }

 private:
  LogNode(std::shared_ptr<Node<T>> node) : node_{std::move(node)} {}
  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class PowNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> base,
                   std::shared_ptr<Node<T>> exponent) {
    return std::shared_ptr<PowNode>(
        new PowNode(std::move(base), std::move(exponent)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      using std::pow;
      value_.emplace(pow(base_->value(), exponent_->value()));
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    base_->clearValue();
    exponent_->clearValue();
  }

  void propagate(const T& derivative) override {
    using std::pow;
    base_->propagate(derivative * exponent_->value() *
                     pow(base_->value(), exponent_->value() - 1));
    if (base_->value() == T{0}) {
      exponent_->propagate(T{0});
      return;
    }
    exponent_->propagate(derivative * pow(base_->value(), exponent_->value()) *
                         std::log(base_->value()));
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    base_->propagateNode(MultipliesNode<T>::make(
        MultipliesNode<T>::make(
            exponent_,
            PowNode<T>::make(
                base_, MinusNode<T>::make(
                           exponent_, IndependentVariable<T>::make(T{1.})))),
        node));
    if (base_->value() == T{0}) {
      exponent_->propagateNode(IndependentVariable<T>::make(T{0.}));
      return;
    }
    exponent_->propagateNode(MultipliesNode<T>::make(
        MultipliesNode<T>::make(node, PowNode<T>::make(base_, exponent_)),
        LogNode<T>::make(base_)));
  }

 private:
  PowNode(std::shared_ptr<Node<T>> base, std::shared_ptr<Node<T>> exponent)
      : base_{std::move(base)}, exponent_{std::move(exponent)} {}
  std::shared_ptr<Node<T>> base_;
  std::shared_ptr<Node<T>> exponent_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class ExpNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr<ExpNode>(new ExpNode(std::move(node)));
  }

  auto value() -> const T& {
    if (!value_.has_value()) {
      using std::exp;
      value_.emplace(exp(node_->value()));
    }
    return *value_;
  }

  void clearValue() {
    value_ = std::nullopt;
    node_->clearValue();
  }

  void propagate(const T& derivative) {
    using std::exp;
    node_->propagate(derivative * exp(node_->value()));
  }

  void propagateNode(std::shared_ptr<Node<T>> node) {
    node_->propagateNode(
        MultipliesNode<T>::make(ExpNode<T>::make(node_), node));
  }

 private:
  ExpNode(std::shared_ptr<Node<T>> node) : node_{std::move(node)} {}

  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class CosNode;

template <typename T>
class SinNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr<SinNode>(new SinNode(std::move(node)));
  }

  auto value() -> const T& override {
    if (!value_.has_value()) {
      using std::sin;
      value_.emplace(sin(node_->value()));
    }
    return *value_;
  }

  void clearValue() override {
    value_ = std::nullopt;
    node_->clearValue();
  }

  void propagate(const T& derivative) override {
    using std::cos;
    node_->propagate(derivative * cos(node_->value()));
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    node_->propagateNode(
        MultipliesNode<T>::make(CosNode<T>::make(node_), node));
  }

 private:
  SinNode(std::shared_ptr<Node<T>> node) : node_{std::move(node)} {}
  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class CosNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr<CosNode>(new CosNode(std::move(node)));
  }

  auto value() -> const T& {
    if (!value_.has_value()) {
      using std::cos;
      value_.emplace(cos(node_->value()));
    }
    return *value_;
  }

  void clearValue() {
    value_ = std::nullopt;
    node_->clearValue();
  }

  void propagate(const T& derivative) {
    using std::sin;
    node_->propagate(-derivative * sin(node_->value()));
  }

  void propagateNode(std::shared_ptr<Node<T>> node) {
    node_->propagateNode(MultipliesNode<T>::make(
        NegateNode<T>::make(SinNode<T>::make(node_)), node));
  }

 private:
  CosNode(std::shared_ptr<Node<T>> node) : node_{std::move(node)} {}
  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class TanNode : public Node<T> {
 public:
  static auto make(std::shared_ptr<Node<T>> node) {
    return std::shared_ptr<TanNode>(new TanNode(std::move(node)));
  }

  auto value() -> const T& {
    if (!value_.has_value()) {
      using std::tan;
      value_.emplace(tan(node_->value()));
    }
    return *value_;
  }

  void clearValue() {
    value_ = std::nullopt;
    node_->clearValue();
  }

  void propagate(const T& derivative) {
    using std::cos;
    node_->propagate(derivative / (cos(node_->value()) * cos(node_->value())));
  }

  void propagateNode(std::shared_ptr<Node<T>> node) {
    auto cos_node = CosNode<T>::make(node_);
    node_->propagateNode(DividesNode<T>::make(
        node, MultipliesNode<T>::make(cos_node, cos_node)));
  }

 private:
  TanNode(std::shared_ptr<Node<T>> node) : node_{std::move(node)} {}
  std::shared_ptr<Node<T>> node_;
  std::optional<T> value_ = std::nullopt;
};

template <typename T>
class DependantVariable : public Node<T> {
 public:
  static auto make() {
    return std::shared_ptr<DependantVariable>(new DependantVariable());
  }

  static auto make(auto&& value) {
    return std::shared_ptr<DependantVariable>(
        new DependantVariable(std::forward<decltype(value)>(value)));
  }

  void bind(T value) { value_ = std::move(value); }

  auto value() -> const T& override {
    if (!value_.has_value()) [[unlikely]] {
      throw std::runtime_error("No value bound to DependantVariable");
    }
    return *value_;
  }

  auto derivative() const -> const T& { return reverse_.value(); }

  auto derivativeNode() { return reverse_node_; }

  void clearValue() override {
    value_ = std::nullopt;
  }

  void clearDerivative() {
    reverse_ = std::nullopt;
    reverse_node_ = nullptr;
  }

  void propagate(const T& derivative) override {
    if (!reverse_.has_value()) {
      reverse_ = derivative;
      return;
    }
    *reverse_ += derivative;
  }

  void propagateNode(std::shared_ptr<Node<T>> node) override {
    if (reverse_node_ == nullptr) {
      reverse_node_ = node;
      return;
    }
    reverse_node_ = PlusNode<T>::make(reverse_node_, node);
  }

 private:
  DependantVariable() = default;
  DependantVariable(T value) : value_{std::move(value)} {}

  std::optional<T> value_ = std::nullopt;
  std::optional<T> reverse_ = std::nullopt;
  std::shared_ptr<Node<T>> reverse_node_ = nullptr;
};

template <typename T>
struct NodeExpr {
  explicit NodeExpr(std::shared_ptr<Node<T>> arg) : node{std::move(arg)} {}

  virtual ~NodeExpr() = default;

  auto value() const -> const T& { return node->value(); }

  std::shared_ptr<Node<T>> node;
};

template <template <typename> typename NodeT, typename T>
NodeExpr(std::shared_ptr<NodeT<T>>) -> NodeExpr<T>;

template <typename T = double>
struct Variable : public NodeExpr<T> {
  Variable() : NodeExpr<T>{DependantVariable<T>::make()} {}

  auto bind(T value) -> void {
    std::static_pointer_cast<DependantVariable<T>>(this->node)->bind(value);
  }

  auto derivative() const -> const T& {
    return std::static_pointer_cast<DependantVariable<T>>(this->node)
        ->derivative();
  }

  auto derivativeNode() {
    return std::static_pointer_cast<DependantVariable<T>>(this->node)
        ->derivativeNode();
  }

  void clearDerivative() {
    std::static_pointer_cast<DependantVariable<T>>(this->node)
        ->clearDerivative();
  }
};

template <typename T>
auto operator+(NodeExpr<T> left, NodeExpr<T> right) {
  return NodeExpr(
      PlusNode<T>::make(std::move(left.node), std::move(right.node)));
}

template <typename T>
auto operator+(NodeExpr<T> left, T right) {
  return std::move(left) +
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
}

template <typename T>
auto operator+(T left, NodeExpr<T> right) {
  return NodeExpr{IndependentVariable<T>::make(std::move(left))} +
         std::move(right);
}

template <typename T>
auto operator+=(NodeExpr<T>& left, NodeExpr<T> right) {
  left = std::move(left) + std::move(right);
  return left;
}

template <typename T>
auto operator+=(NodeExpr<T>& left, T right) {
  left = std::move(left) +
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
  return left;
}

template <typename T>
auto operator-(NodeExpr<T> left, NodeExpr<T> right) {
  return NodeExpr{
      MinusNode<T>::make(std::move(left.node), std::move(right.node))};
}

template <typename T>
auto operator-(NodeExpr<T> left, T right) {
  return std::move(left) -
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
}

template <typename T>
auto operator-(T left, NodeExpr<T> right) {
  return NodeExpr{IndependentVariable<T>::make(std::move(left))} -
         std::move(right);
}

template <typename T>
auto operator-=(NodeExpr<T>& left, NodeExpr<T> right) {
  left = std::move(left) - std::move(right);
  return left;
}

template <typename T>
auto operator-=(NodeExpr<T>& left, T right) {
  left = std::move(left) -
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
  return left;
}

template <typename T>
auto operator*(NodeExpr<T> left, NodeExpr<T> right) {
  return NodeExpr{
      MultipliesNode<T>::make(std::move(left.node), std::move(right.node))};
}

template <typename T>
auto operator*(NodeExpr<T> left, T right) {
  return std::move(left) *
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
}

template <typename T>
auto operator*(T left, NodeExpr<T> right) {
  return NodeExpr{IndependentVariable<T>::make(std::move(left))} *
         std::move(right);
}

template <typename T>
auto operator*=(NodeExpr<T>& left, NodeExpr<T> right) {
  left = std::move(left) * std::move(right);
  return left;
}

template <typename T>
auto operator*=(NodeExpr<T>& left, T right) {
  left = std::move(left) *
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
  return left;
}

template <typename T>
auto operator/(NodeExpr<T> left, NodeExpr<T> right) {
  return NodeExpr{
      DividesNode<T>::make(std::move(left.node), std::move(right.node))};
}

template <typename T>
auto operator/(NodeExpr<T> left, T right) {
  return std::move(left) /
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
}

template <typename T>
auto operator/(T left, NodeExpr<T> right) {
  return NodeExpr{IndependentVariable<T>::make(std::move(left))} /
         std::move(right);
}

template <typename T>
auto operator/=(NodeExpr<T>& left, NodeExpr<T> right) {
  left = std::move(left) / std::move(right);
  return left;
}

template <typename T>
auto operator/=(NodeExpr<T>& left, T right) {
  left = std::move(left) /
         NodeExpr{IndependentVariable<T>::make(std::move(right))};
  return left;
}

template <typename T>
auto sqrt(NodeExpr<T> expr) {
  return NodeExpr{SqrtNode<T>::make(std::move(expr.node))};
}

template <typename T>
auto pow(NodeExpr<T> base, NodeExpr<T> exponent) {
  return NodeExpr{
      PowNode<T>::make(std::move(base.node), std::move(exponent.node))};
}

template <typename T>
auto pow(NodeExpr<T> base, T exponent) {
  return pow(std::move(base),
             NodeExpr{IndependentVariable<T>::make(std::move(exponent))});
}

template <typename T>
auto pow(T base, NodeExpr<T> exponent) {
  return pow(NodeExpr{IndependentVariable<T>::make(std::move(base))},
             std::move(exponent));
}

template <typename T>
auto exp(NodeExpr<T> expr) {
  return NodeExpr{ExpNode<T>::make(std::move(expr.node))};
}

template <typename T>
auto log(NodeExpr<T> expr) {
  return NodeExpr{LogNode<T>::make(std::move(expr.node))};
}

template <typename T>
auto sin(NodeExpr<T> expr) {
  return NodeExpr{SinNode<T>::make(std::move(expr.node))};
}

template <typename T>
auto cos(NodeExpr<T> expr) {
  return NodeExpr{CosNode<T>::make(std::move(expr.node))};
}

template <typename T>
auto tan(NodeExpr<T> expr) {
  return NodeExpr{TanNode<T>::make(std::move(expr.node))};
}

template <typename... Args>
struct Wrt {
  Wrt(Args... args) : symbols{args...} {}
  std::tuple<Args...> symbols;
};

template <typename... Args>
struct At {
  At(Args&&... args) : values(std::forward<Args>(args)...) {}
  std::tuple<Args...> values;
};
template <typename... Args>
At(Args&&... args) -> At<Args...>;

template <typename T, typename... Vars, typename... Args>
  requires(sizeof...(Vars) == sizeof...(Args))
auto differentiate(NodeExpr<T> expr, Wrt<Vars...> wrt, const At<Args...>& at) {
  // clang-format off
  std::apply([&](auto&... symbol) {
    std::apply([&](auto&... value) {
      (symbol.bind(value),
       ...);
    }, at.values);
  }, wrt.symbols);
  expr.node->propagate(T{1.});
  auto out = std::apply([&](auto&... symbol) {
    return std::tuple{expr.value(), symbol.derivative()...};
  }, wrt.symbols);
  // clang-format on
  expr.node->clearValue();
  return out;
}

}  // namespace tempura::autodiff
