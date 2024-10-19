#pragma once

namespace tempura::bayes {

template <typename T = double>
class Bernoulli {
public:
  Bernoulli(T p) : p_{p} {}

  template <typename Generator>
  auto sample(Generator& g) const -> bool {
    constexpr auto delta = Generator::max() - Generator::min();
    return g() < p_ * delta;
  }

  auto prob(bool x) const {
    return x ? p_ : 1 - p_;
  }

  auto logProb(bool x) const {
    return x ? log(p_) : log(1 - p_);
  }

  auto cdf(bool x) const {
    return x ? 1 : 1 - p_;
  }

private:
  T p_;
};

} // namespace tempura::bayes
