#pragma once

#include <algorithm>
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
struct NodeExpr {
  explicit NodeExpr(std::shared_ptr<Node<Forward, Reverse>> arg)
      : node{std::move(arg)} {}

  virtual ~NodeExpr() = default;

  auto value() const -> const Forward& { return node->value(); }

  std::shared_ptr<Node<Forward, Reverse>> node;
};

template <typename Forward, typename Reverse>
class DependantVariable;

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
class DependantVariable : public Node<Forward, Reverse> {
 public:
  static auto make() {
    return std::shared_ptr<Node<Forward, Reverse>>(
        new DependantVariable<Forward, Reverse>());
  }

  static auto make(auto&& value) {
    return std::shared_ptr<Node<Forward, Reverse>>(
        new DependantVariable<Forward, Reverse>(
            std::forward<decltype(value)>(value)));
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
    return std::shared_ptr<Node<Forward, Reverse>>(
        new IndependentVariable<Forward, Reverse>(
            std::forward<decltype(value)>(value)));
  }

  auto value() -> const Forward& override { return forward_.value(); }

  void clear() override { forward_ = std::nullopt; }

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
    return std::shared_ptr<Node<Forward, Reverse>>(
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
auto operator+(NodeExpr<Forward, Reverse> left,
               NodeExpr<Forward, Reverse> right) {
  return NodeExpr{PlusNode<Forward, Reverse>::make(std::move(left.node),
                                                   std::move(right.node))};
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
class NegateNode : public Node<Forward, Reverse> {
 public:
  static auto make(Node<Forward, Reverse> node) {
    return std::shared_ptr<Node<Forward, Reverse>>(
        new NegateNode(std::move(node.node)));
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
  static auto make(NodeExpr<Forward, Reverse> left,
                   NodeExpr<Forward, Reverse> right) {
    return std::shared_ptr<Node<Forward, Reverse>>(
        new MultipliesNode(std::move(left.node), std::move(right.node)));
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
auto operator*(NodeExpr<Forward, Reverse> left,
               NodeExpr<Forward, Reverse> right) {
  return NodeExpr{MultipliesNode<Forward, Reverse>::make(std::move(left),
                                                         std::move(right))};
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
class DividesNode : public Node<Forward, Reverse> {
 public:
  static auto make(NodeExpr<Forward, Reverse> left,
                   NodeExpr<Forward, Reverse> right) {
    return std::shared_ptr<Node<Forward, Reverse>>(
        new DividesNode(std::move(left.node), std::move(right.node)));
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
auto operator/(NodeExpr<Forward, Reverse> left,
               NodeExpr<Forward, Reverse> right) {
  return NodeExpr{
      DividesNode<Forward, Reverse>::make(std::move(left), std::move(right))};
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
