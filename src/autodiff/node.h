#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace tempura::autodiff {

template <typename Forward, typename Reverse>
class Node {
 public:
  virtual ~Node() = default;

  virtual auto value() -> const Forward& = 0;

  virtual void clear() = 0;

  virtual void propagate(const Reverse& derivative) = 0;
};

template <typename Forward, typename Reverse>
class DependantVariable : public Node<Forward, Reverse> {
 public:
  static auto make() {
    return std::shared_ptr<DependantVariable>(new DependantVariable());
  }

  static auto make(auto&& value) {
    return std::shared_ptr<DependantVariable>(
        new DependantVariable(std::forward<decltype(value)>(value)));
  }

  void bind(const Forward* value) { forward_ = value; }

  auto value() -> const Forward& override {
    if (forward_ == nullptr) [[unlikely]] {
      throw std::runtime_error("No value bound to DependantVariable");
    }
    return *forward_;
  }

  auto derivative() const -> const Reverse& { return reverse_.value(); }

  void clear() override {
    forward_ = nullptr;
    reverse_ = std::nullopt;
  }

  void propagate(const Reverse& derivative) override {
    if (!reverse_.has_value()) {
      reverse_ = derivative;
      return;
    }
    *reverse_ += derivative;
  }

 private:
  DependantVariable() = default;
  DependantVariable(Forward forward) : forward_{std::move(forward)} {}

  const Forward* forward_ = nullptr;
  std::optional<Reverse> reverse_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class IndependentVariable : public Node<Forward, Reverse> {
 public:
  static auto make(auto&& value) {
    return std::shared_ptr<IndependentVariable>(
        new IndependentVariable(std::forward<decltype(value)>(value)));
  }

  auto value() -> const Forward& override { return forward_.value(); }

  // Do nothing as IndependentVariables represent constants in the expression
  // and we are not going to rebind data to them
  void clear() override {}

  void propagate(const Reverse& /*unused*/) override {}

 private:
  IndependentVariable() = default;
  IndependentVariable(Forward forward) : forward_{std::move(forward)} {}

  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class PlusNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> left,
                   std::shared_ptr<Node<Forward, Reverse>> right) {
    return std::shared_ptr<PlusNode>(
        new PlusNode(std::move(left), std::move(right)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      forward_.emplace(left_->value() + right_->value());
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    left_->clear();
    right_->clear();
  }

  void propagate(const Reverse& derivative) override {
    left_->propagate(derivative);
    right_->propagate(derivative);
  }

 private:
  PlusNode(std::shared_ptr<Node<Forward, Reverse>> left,
           std::shared_ptr<Node<Forward, Reverse>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<Forward, Reverse>> left_;
  std::shared_ptr<Node<Forward, Reverse>> right_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class PosNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> node) {
    return std::shared_ptr{new PlusNode(std::move(node))};
  }

 private:
  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class MinusNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> left,
                   std::shared_ptr<Node<Forward, Reverse>> right) {
    return std::shared_ptr<Node<Forward, Reverse>>(
        new MinusNode<Forward, Reverse>(std::move(left), std::move(right)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      forward_.emplace(left_->value() - right_->value());
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    left_->clear();
    right_->clear();
  }

  void propagate(const Reverse& derivative) override {
    left_->propagate(derivative);
    right_->propagate(-derivative);
  }

 private:
  MinusNode(std::shared_ptr<Node<Forward, Reverse>> left,
            std::shared_ptr<Node<Forward, Reverse>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<Forward, Reverse>> left_;
  std::shared_ptr<Node<Forward, Reverse>> right_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class NegateNode : public Node<Forward, Reverse> {
 public:
  static auto make(Node<Forward, Reverse> node) {
    return std::shared_ptr<NegateNode>(new NegateNode(std::move(node.node)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      forward_.emplace(-node_->value());
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    node_->clear();
  }

  void propagate(const Reverse& derivative) override {
    forward_->propagate(-derivative);
  }

 private:
  NegateNode(std::shared_ptr<Node<Forward, Reverse>> node)
      : node_{std::move(node)} {}

  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class MultipliesNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> left,
                   std::shared_ptr<Node<Forward, Reverse>> right) {
    return std::shared_ptr<MultipliesNode>(
        new MultipliesNode(std::move(left), std::move(right)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      forward_.emplace(left_->value() * right_->value());
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    left_->clear();
    right_->clear();
  }

  void propagate(const Reverse& derivative) override {
    left_->propagate(derivative * right_->value());
    right_->propagate(derivative * left_->value());
  }

 private:
  MultipliesNode(std::shared_ptr<Node<Forward, Reverse>> left,
                 std::shared_ptr<Node<Forward, Reverse>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<Forward, Reverse>> left_;
  std::shared_ptr<Node<Forward, Reverse>> right_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class DividesNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> left,
                   std::shared_ptr<Node<Forward, Reverse>> right) {
    return std::shared_ptr<DividesNode>(
        new DividesNode(std::move(left), std::move(right)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      forward_.emplace(left_->value() / right_->value());
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    left_->clear();
    right_->clear();
  }

  void propagate(const Reverse& derivative) override {
    left_->propagate(derivative / right_->value());
    right_->propagate(-derivative * left_->value() /
                      (right_->value() * right_->value()));
  }

 private:
  DividesNode(std::shared_ptr<Node<Forward, Reverse>> left,
              std::shared_ptr<Node<Forward, Reverse>> right)
      : left_{std::move(left)}, right_{std::move(right)} {}

  std::shared_ptr<Node<Forward, Reverse>> left_;
  std::shared_ptr<Node<Forward, Reverse>> right_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class SqrtNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> node) {
    return std::shared_ptr<SqrtNode>(new SqrtNode(std::move(node)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      using std::sqrt;
      forward_.emplace(sqrt(node_->value()));
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    node_->clear();
  }

  void propagate(const Reverse& derivative) override {
    using std::sqrt;
    node_->propagate(derivative / (2. * sqrt(node_->value())));
  }

 private:
  SqrtNode(std::shared_ptr<Node<Forward, Reverse>> node)
      : node_{std::move(node)} {}

  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class PowNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> base,
                   std::shared_ptr<Node<Forward, Reverse>> exponent) {
    return std::shared_ptr<PowNode>(
        new PowNode(std::move(base), std::move(exponent)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      using std::pow;
      forward_.emplace(pow(base_->value(), exponent_->value()));
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    base_->clear();
    exponent_->clear();
  }

  void propagate(const Reverse& derivative) override {
    using std::pow;
    base_->propagate(derivative * exponent_->value() *
                     pow(base_->value(), exponent_->value() - 1));
    if (base_->value() == Forward{0}) {
      exponent_->propagate(Reverse{0});
      return;
    }
    exponent_->propagate(derivative * pow(base_->value(), exponent_->value()) *
                         std::log(base_->value()));
  }

 private:
  PowNode(std::shared_ptr<Node<Forward, Reverse>> base,
          std::shared_ptr<Node<Forward, Reverse>> exponent)
      : base_{std::move(base)}, exponent_{std::move(exponent)} {}
  std::shared_ptr<Node<Forward, Reverse>> base_;
  std::shared_ptr<Node<Forward, Reverse>> exponent_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class LogNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> node) {
    return std::shared_ptr<LogNode>(new LogNode(std::move(node)));
  }

  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      using std::log;
      forward_.emplace(log(node_->value()));
    }
    return *forward_;
  }

  void clear() override {
    forward_ = std::nullopt;
    node_->clear();
  }

  void propagate(const Reverse& derivative) override {
    node_->propagate(derivative / node_->value());
  }

 private:
  LogNode(std::shared_ptr<Node<Forward, Reverse>> node)
      : node_{std::move(node)} {}
  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class SinNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> node) {
    return std::shared_ptr<SinNode>(new SinNode(std::move(node)));
  }
  auto value() -> const Forward& override {
    if (!forward_.has_value()) {
      using std::sin;
      forward_.emplace(sin(node_->value()));
    }
    return *forward_;
  }
  void clear() override {
    forward_ = std::nullopt;
    node_->clear();
  }
  void propagate(const Reverse& derivative) override {
    using std::cos;
    node_->propagate(derivative * cos(node_->value()));
  }
 private:
  SinNode(std::shared_ptr<Node<Forward, Reverse>> node)
      : node_{std::move(node)} {}
  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class ExpNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> node) {
    return std::shared_ptr<ExpNode>(new ExpNode(std::move(node)));
  }

  auto value() -> const Forward& {
    if (!forward_.has_value()) {
      using std::exp;
      forward_.emplace(exp(node_->value()));
    }
    return *forward_;
  }

  void clear() {
    forward_ = std::nullopt;
    node_->clear();
  }

  void propagate(const Reverse& derivative) {
    using std::exp;
    node_->propagate(derivative * exp(node_->value()));
  }

 private:
  ExpNode(std::shared_ptr<Node<Forward, Reverse>> node)
      : node_{std::move(node)} {}

  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class CosNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> node) {
    return std::shared_ptr<CosNode>(new CosNode(std::move(node)));
  }
  auto value() -> const Forward& {
    if (!forward_.has_value()) {
      using std::cos;
      forward_.emplace(cos(node_->value()));
    }
    return *forward_;
  }
  void clear() {
    forward_ = std::nullopt;
    node_->clear();
  }
  void propagate(const Reverse& derivative) {
    using std::sin;
    node_->propagate(-derivative * sin(node_->value()));
  }
 private:
  CosNode(std::shared_ptr<Node<Forward, Reverse>> node)
      : node_{std::move(node)} {}
  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
class TanNode : public Node<Forward, Reverse> {
 public:
  static auto make(std::shared_ptr<Node<Forward, Reverse>> node) {
    return std::shared_ptr<TanNode>(new TanNode(std::move(node)));
  }
  auto value() -> const Forward& {
    if (!forward_.has_value()) {
      using std::tan;
      forward_.emplace(tan(node_->value()));
    }
    return *forward_;
  }
  void clear() {
    forward_ = std::nullopt;
    node_->clear();
  }
  void propagate(const Reverse& derivative) {
    using std::cos;
    node_->propagate(derivative / (cos(node_->value()) * cos(node_->value())));
  }
 private:
  TanNode(std::shared_ptr<Node<Forward, Reverse>> node)
      : node_{std::move(node)} {}
  std::shared_ptr<Node<Forward, Reverse>> node_;
  std::optional<Forward> forward_ = std::nullopt;
};

template <typename Forward, typename Reverse>
struct NodeExpr {
  explicit NodeExpr(std::shared_ptr<Node<Forward, Reverse>> arg)
      : node{std::move(arg)} {}

  virtual ~NodeExpr() = default;

  auto value() const -> const Forward& { return node->value(); }

  std::shared_ptr<Node<Forward, Reverse>> node;
};

template <template <typename, typename> typename NodeT, typename Forward,
          typename Reverse>
NodeExpr(std::shared_ptr<NodeT<Forward, Reverse>>)
    -> NodeExpr<Forward, Reverse>;

template <typename Forward = double, typename Reverse = Forward>
struct Variable : public NodeExpr<Forward, Reverse> {
  Variable()
      : NodeExpr<Forward, Reverse>{
            DependantVariable<Forward, Reverse>::make()} {}

  auto bind(const Forward* value) -> void {
    std::static_pointer_cast<DependantVariable<Forward, Reverse>>(this->node)
        ->bind(value);
  }

  auto derivative() -> const Reverse& {
    return std::static_pointer_cast<DependantVariable<Forward, Reverse>>(
               this->node)
        ->derivative();
  }
};

template <typename Forward, typename Reverse>
auto operator+(NodeExpr<Forward, Reverse> left,
               NodeExpr<Forward, Reverse> right) {
  return NodeExpr(PlusNode<Forward, Reverse>::make(std::move(left.node),
                                                   std::move(right.node)));
}

template <typename Forward, typename Reverse>
auto operator+(NodeExpr<Forward, Reverse> left, Forward right) {
  return std::move(left) + NodeExpr{IndependentVariable<Forward, Reverse>::make(
                               std::move(right))};
}

template <typename Forward, typename Reverse>
auto operator+(Forward left, NodeExpr<Forward, Reverse> right) {
  return NodeExpr{
             IndependentVariable<Forward, Reverse>::make(std::move(left))} +
         std::move(right);
}

template <typename Forward, typename Reverse>
auto operator+=(NodeExpr<Forward, Reverse>& left,
                NodeExpr<Forward, Reverse> right) {
  left = std::move(left) + std::move(right);
  return left;
}

template <typename Forward, typename Reverse>
auto operator+=(NodeExpr<Forward, Reverse>& left, Forward right) {
  left =
      std::move(left) +
      NodeExpr{IndependentVariable<Forward, Reverse>::make(std::move(right))};
  return left;
}

template <typename Forward, typename Reverse>
auto operator-(NodeExpr<Forward, Reverse> left,
               NodeExpr<Forward, Reverse> right) {
  return NodeExpr{MinusNode<Forward, Reverse>::make(std::move(left.node),
                                                    std::move(right.node))};
}

template <typename Forward, typename Reverse>
auto operator-(NodeExpr<Forward, Reverse> left, Forward right) {
  return std::move(left) - NodeExpr{IndependentVariable<Forward, Reverse>::make(
                               std::move(right))};
}

template <typename Forward, typename Reverse>
auto operator-(Forward left, NodeExpr<Forward, Reverse> right) {
  return NodeExpr{
             IndependentVariable<Forward, Reverse>::make(std::move(left))} -
         std::move(right);
}

template <typename Forward, typename Reverse = Forward>
auto operator-=(NodeExpr<Forward, Reverse>& left,
                NodeExpr<Forward, Reverse> right) {
  left = std::move(left) - std::move(right);
  return left;
}

template <typename Forward, typename Reverse = Forward>
auto operator-=(NodeExpr<Forward, Reverse>& left, Forward right) {
  left =
      std::move(left) -
      NodeExpr{IndependentVariable<Forward, Reverse>::make(std::move(right))};
  return left;
}

template <typename Forward, typename Reverse>
auto operator*(NodeExpr<Forward, Reverse> left,
               NodeExpr<Forward, Reverse> right) {
  return NodeExpr{MultipliesNode<Forward, Reverse>::make(
      std::move(left.node), std::move(right.node))};
}

template <typename Forward, typename Reverse>
auto operator*(NodeExpr<Forward, Reverse> left, Forward right) {
  return std::move(left) * NodeExpr{IndependentVariable<Forward, Reverse>::make(
                               std::move(right))};
}

template <typename Forward, typename Reverse>
auto operator*(Forward left, NodeExpr<Forward, Reverse> right) {
  return NodeExpr{
             IndependentVariable<Forward, Reverse>::make(std::move(left))} *
         std::move(right);
}

template <typename Forward, typename Reverse = Forward>
auto operator*=(NodeExpr<Forward, Reverse>& left,
                NodeExpr<Forward, Reverse> right) {
  left = std::move(left) * std::move(right);
  return left;
}

template <typename Forward, typename Reverse = Forward>
auto operator*=(NodeExpr<Forward, Reverse>& left, Forward right) {
  left =
      std::move(left) *
      NodeExpr{IndependentVariable<Forward, Reverse>::make(std::move(right))};
  return left;
}

template <typename Forward, typename Reverse>
auto operator/(NodeExpr<Forward, Reverse> left,
               NodeExpr<Forward, Reverse> right) {
  return NodeExpr{DividesNode<Forward, Reverse>::make(std::move(left.node),
                                                      std::move(right.node))};
}

template <typename Forward, typename Reverse>
auto operator/(NodeExpr<Forward, Reverse> left, Forward right) {
  return std::move(left) / NodeExpr{IndependentVariable<Forward, Reverse>::make(
                               std::move(right))};
}

template <typename Forward, typename Reverse>
auto operator/(Forward left, NodeExpr<Forward, Reverse> right) {
  return NodeExpr{
             IndependentVariable<Forward, Reverse>::make(std::move(left))} /
         std::move(right);
}

template <typename Forward, typename Reverse = Forward>
auto operator/=(NodeExpr<Forward, Reverse>& left,
                NodeExpr<Forward, Reverse> right) {
  left = std::move(left) / std::move(right);
  return left;
}

template <typename Forward, typename Reverse = Forward>
auto operator/=(NodeExpr<Forward, Reverse>& left, Forward right) {
  left =
      std::move(left) /
      NodeExpr{IndependentVariable<Forward, Reverse>::make(std::move(right))};
  return left;
}

template <typename Forward, typename Reverse>
auto sqrt(NodeExpr<Forward, Reverse> expr) {
  return NodeExpr{SqrtNode<Forward, Reverse>::make(std::move(expr.node))};
}

template <typename Forward, typename Reverse>
auto pow(NodeExpr<Forward, Reverse> base, NodeExpr<Forward, Reverse> exponent) {
  return NodeExpr{PowNode<Forward, Reverse>::make(std::move(base.node),
                                                  std::move(exponent.node))};
}

template <typename Forward, typename Reverse>
auto pow(NodeExpr<Forward, Reverse> base, Forward exponent) {
  return pow(std::move(base),
             NodeExpr{IndependentVariable<Forward, Reverse>::make(
                 std::move(exponent))});
}

template <typename Forward, typename Reverse>
auto pow(Forward base, NodeExpr<Forward, Reverse> exponent) {
  return pow(
      NodeExpr{IndependentVariable<Forward, Reverse>::make(std::move(base))},
      std::move(exponent));
}

template <typename Forward, typename Reverse>
auto exp(NodeExpr<Forward, Reverse> expr) {
  return NodeExpr{ExpNode<Forward, Reverse>::make(std::move(expr.node))};
}

template <typename Forward, typename Reverse>
auto log(NodeExpr<Forward, Reverse> expr) {
  return NodeExpr{LogNode<Forward, Reverse>::make(std::move(expr.node))};
}

template <typename Forward, typename Reverse>
auto sin(NodeExpr<Forward, Reverse> expr) {
  return NodeExpr{SinNode<Forward, Reverse>::make(std::move(expr.node))};
}

template <typename Forward, typename Reverse>
auto cos(NodeExpr<Forward, Reverse> expr) {
  return NodeExpr{CosNode<Forward, Reverse>::make(std::move(expr.node))};
}

template <typename Forward, typename Reverse>
auto tan(NodeExpr<Forward, Reverse> expr) {
  return NodeExpr{TanNode<Forward, Reverse>::make(std::move(expr.node))};
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

template <typename Forward, typename Reverse, typename... Vars,
          typename... Args>
  requires(sizeof...(Vars) == sizeof...(Args))
auto differentiate(NodeExpr<Forward, Reverse> expr, Wrt<Vars...> wrt,
                   const At<Args...>& at) {
  // clang-format off
  std::apply([&](auto&... symbol) {
    std::apply([&](auto&... value) {
      (symbol.bind(&value),
       ...);
    }, at.values);
  }, wrt.symbols);
  expr.node->propagate(Reverse{1.});
  auto out = std::apply([&](auto&... symbol) {
    return std::tuple{expr.value(), symbol.derivative()...};
  }, wrt.symbols);
  // clang-format on
  expr.node->clear();
  return out;
}

}  // namespace tempura::autodiff
