// Tests for symbolic4/indexed - plate notation
#include "symbolic4/distributions/distributions.h"
#include "symbolic4/distributions/multivariate.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/indexed/sum_over_diff.h"
#include "symbolic4/interpreter/simplify.h"
#include "unit.h"

#include <cmath>
#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // IndexedSymbol and Dim
  // ===========================================================================

  "IndexedSymbol has correct types"_test = [] {
    struct Obs {};
    struct ThetaId {};

    using Sym = IndexedSymbol<ThetaId, Obs>;
    static_assert(std::is_same_v<typename Sym::id_type, ThetaId>);
    static_assert(std::is_same_v<typename Sym::dims_list, TypeList<Obs>>);
    static_assert(Sym::ndims == 1);
    static_assert(Sym::has_dim<Obs>);
    static_assert(is_indexed_symbol_v<Sym>);
    static_assert(!is_indexed_symbol_v<Symbol<ThetaId>>);
  };

  "2D IndexedSymbol has correct types"_test = [] {
    struct Countries {};
    struct Years {};
    struct YId {};

    using Sym = IndexedSymbol<YId, Countries, Years>;
    static_assert(std::is_same_v<typename Sym::id_type, YId>);
    static_assert(std::is_same_v<typename Sym::dims_list, TypeList<Countries, Years>>);
    static_assert(Sym::ndims == 2);
    static_assert(Sym::has_dim<Countries>);
    static_assert(Sym::has_dim<Years>);
  };

  "Shape and IndexedValuesND"_test = [] {
    struct Countries {};
    struct Years {};

    std::vector<double> data = {1, 2, 3, 4, 5, 6};  // 2x3
    auto binding = indexed(data, Shape<Countries, Years>{2, 3});

    expectNear(1.0, binding.at(0, 0), 1e-10);
    expectNear(2.0, binding.at(0, 1), 1e-10);
    expectNear(3.0, binding.at(0, 2), 1e-10);
    expectNear(4.0, binding.at(1, 0), 1e-10);
    expectNear(5.0, binding.at(1, 1), 1e-10);
    expectNear(6.0, binding.at(1, 2), 1e-10);
  };

  "IndexedBinding 2D"_test = [] {
    struct Countries {};
    struct Years {};
    struct YId {};

    using Sym = IndexedSymbol<YId, Countries, Years>;
    std::vector<double> data = {1, 2, 3, 4, 5, 6};  // 2x3
    IndexedBinding<Sym, 2> binding{data, {2, 3}};

    expectNear(1.0, binding.at(0, 0), 1e-10);
    expectNear(3.0, binding.at(0, 2), 1e-10);
    expectNear(4.0, binding.at(1, 0), 1e-10);
    expectEq(binding.dimSize<Countries>(), 2UL);
    expectEq(binding.dimSize<Years>(), 3UL);
  };

  // ===========================================================================
  // SumOver
  // ===========================================================================

  "SumOver construction"_test = [] {
    struct Obs {};
    Symbol<struct X> x;

    auto sum = sumOver<Obs>(x * x);
    static_assert(is_sum_over_v<decltype(sum)>);
    static_assert(std::is_same_v<typename decltype(sum)::dim_tag, Obs>);
  };

  "SumOver evaluation with indexed binding"_test = [] {
    struct Obs {};
    struct ThetaId {};

    IndexedSymbol<ThetaId, Obs> theta;
    auto sum = sumOver<Obs>(theta * theta);

    std::vector<double> values = {1.0, 2.0, 3.0};
    auto binding = IndexedBinding<IndexedSymbol<ThetaId, Obs>>{values};

    auto bp = BinderPack{binding};
    using Interp = IndexedEval<BinderPack<>, decltype(bp)>;
    typename Interp::context_type ctx{{}, bp, {}};

    double result = indexedFold<Interp>(sum, ctx);
    // 1^2 + 2^2 + 3^2 = 1 + 4 + 9 = 14
    expectNear(14.0, result, 1e-10);
  };

  // ===========================================================================
  // IndexedBinding
  // ===========================================================================

  "IndexedBinding stores values"_test = [] {
    struct ThetaId {};
    struct Obs {};

    std::vector<double> values = {0.1, 0.2, 0.3};
    IndexedBinding<IndexedSymbol<ThetaId, Obs>> binding{values};

    expectEq(binding.size(), 3UL);
    expectNear(0.1, binding.at(0), 1e-10);
    expectNear(0.2, binding.at(1), 1e-10);
    expectNear(0.3, binding.at(2), 1e-10);
  };

  "indexed() creates IndexedValues"_test = [] {
    std::vector<double> v = {1.0, 2.0, 3.0};
    auto iv = indexed(v);
    expectEq(iv.size(), 3UL);
    expectNear(1.0, iv[0], 1e-10);
  };

  // ===========================================================================
  // plate() function
  // ===========================================================================

  "plate creates IndexedStochasticNode"_test = [] {
    struct Obs {};
    auto alpha = halfNormal(5.0);
    auto theta = plate<Obs>(beta(alpha, lit(3.0)));

    static_assert(is_indexed_random_var_v<decltype(theta)>);
    static_assert(IsIndexedRandomVar<decltype(theta)>);
  };

  "IndexedStochasticNode::sym returns IndexedSymbol"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));
    auto sym = theta.sym();

    static_assert(is_indexed_symbol_v<decltype(sym)>);
  };

  "IndexedStochasticNode::logProb returns SumOver"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));
    auto lp = theta.logProb();

    static_assert(is_sum_over_v<decltype(lp)>);
  };

  // ===========================================================================
  // evaluateIndexed
  // ===========================================================================

  "evaluateIndexed with scalar and indexed bindings"_test = [] {
    struct Obs {};
    struct ThetaId {};
    struct AlphaId {};

    // Manual construction for testing
    IndexedSymbol<ThetaId, Obs> theta;
    Symbol<AlphaId> alpha;

    // Expression: sum_i(alpha * theta[i])
    auto expr = sumOver<Obs>(alpha * theta);

    std::vector<double> theta_vals = {1.0, 2.0, 3.0};
    auto theta_binding = IndexedBinding<IndexedSymbol<ThetaId, Obs>>{theta_vals};
    auto alpha_binding = alpha = 2.0;

    double result = evaluateIndexed(expr, BinderPack{alpha_binding, theta_binding});
    // 2.0 * (1 + 2 + 3) = 12.0
    expectNear(12.0, result, 1e-10);
  };

  // ===========================================================================
  // Integration with distributions
  // ===========================================================================

  "plate with beta distribution logProb"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));
    auto lp = theta.logProb();

    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double result = evaluateIndexed(lp, theta = indexed(theta_vals));

    // Normalized log Beta: (alpha-1)*log(x) + (beta-1)*log(1-x) - log B(alpha, beta)
    // log B(2, 3) = lgamma(2) + lgamma(3) - lgamma(5) = -2.4849...
    double alpha = 2.0, b = 3.0;
    double log_beta_norm = std::lgamma(alpha) + std::lgamma(b) - std::lgamma(alpha + b);
    auto expected_at = [=](double x) {
      return (alpha - 1.0) * std::log(x) + (b - 1.0) * std::log(1.0 - x) - log_beta_norm;
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  "plate with scalar parameter"_test = [] {
    struct Obs {};
    auto alpha = halfNormal(5.0);
    auto theta = plate<Obs>(beta(alpha, lit(3.0)));

    // Just the plate's log-prob (not joint)
    auto lp = theta.logProb();

    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double result = evaluateIndexed(lp, alpha = 2.0, theta = indexed(theta_vals));

    // Normalized log Beta: (alpha-1)*log(x) + (beta-1)*log(1-x) - log B(alpha, beta)
    // Here alpha=2 (bound above), beta=3
    double a = 2.0, b = 3.0;
    double log_beta_norm = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    auto expected_at = [=](double x) {
      return (a - 1.0) * std::log(x) + (b - 1.0) * std::log(1.0 - x) - log_beta_norm;
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  // ===========================================================================
  // Differentiation through SumOver
  // ===========================================================================

  "diff pushes through SumOver"_test = [] {
    struct Obs {};
    Symbol<struct X> x;
    Symbol<struct Y> y;

    // sum_i(x * y[i]) where y is indexed
    // diff w.r.t. x should give sum_i(y[i])
    // But for simplicity, test with just x: sum_i(x^2) -> sum_i(2*x)
    auto sum = sumOver<Obs>(x * x);
    auto d_sum = diff(sum, x);

    static_assert(is_sum_over_v<decltype(d_sum)>);
  };

  "diff of SumOver evaluates correctly"_test = [] {
    struct Obs {};
    struct ThetaId {};

    IndexedSymbol<ThetaId, Obs> theta;
    Symbol<struct Alpha> alpha;

    // sum_i(alpha * theta[i]) -> diff w.r.t alpha = sum_i(theta[i])
    auto sum = sumOver<Obs>(alpha * theta);
    auto d_alpha = diff(sum, alpha);

    std::vector<double> theta_vals = {1.0, 2.0, 3.0};
    auto theta_binding = IndexedBinding<IndexedSymbol<ThetaId, Obs>>{theta_vals};

    double result = evaluateIndexed(d_alpha, BinderPack{alpha = 5.0, theta_binding});
    // d/d_alpha [sum_i(alpha * theta[i])] = sum_i(theta[i]) = 1 + 2 + 3 = 6
    expectNear(6.0, result, 1e-10);
  };

  // ===========================================================================
  // Arithmetic with SumOver
  // ===========================================================================

  "SumOver + scalar expression"_test = [] {
    struct Obs {};
    struct ThetaId {};

    IndexedSymbol<ThetaId, Obs> theta;
    Symbol<struct C> c;

    auto sum = sumOver<Obs>(theta);
    auto expr = sum + c;

    std::vector<double> theta_vals = {1.0, 2.0, 3.0};
    auto theta_binding = IndexedBinding<IndexedSymbol<ThetaId, Obs>>{theta_vals};

    double result = evaluateIndexed(expr, BinderPack{c = 10.0, theta_binding});
    // (1 + 2 + 3) + 10 = 16
    expectNear(16.0, result, 1e-10);
  };

  // ===========================================================================
  // Nested plates
  // ===========================================================================

  "nested plate creates 2D indexed node"_test = [] {
    struct Countries {};
    struct Years {};

    // plate<Years>(plate<Countries>(...)) creates 2D symbol
    auto theta = plate<Years>(plate<Countries>(normal(lit(0.0), lit(1.0))));

    using NodeType = decltype(theta);
    using DimsType = typename NodeType::dims_list;

    static_assert(is_indexed_random_var_v<NodeType>);
    static_assert(std::is_same_v<DimsType, TypeList<Countries, Years>>);

    // sym() should be IndexedSymbol<Id, Countries, Years>
    auto sym = theta.sym();
    static_assert(sym.ndims == 2);
    static_assert(sym.has_dim<Countries>);
    static_assert(sym.has_dim<Years>);
  };

  "nested SumOver evaluation"_test = [] {
    struct Countries {};
    struct Years {};
    struct YId {};

    // 2D symbol varying over both dimensions
    IndexedSymbol<YId, Countries, Years> y;
    auto sum = sumOver<Countries>(sumOver<Years>(y));

    // 2x3 data
    std::vector<double> data = {1, 2, 3, 4, 5, 6};
    IndexedBinding<IndexedSymbol<YId, Countries, Years>, 2> binding{data, {2, 3}};

    auto bp = BinderPack{binding};
    using Interp = IndexedEval<BinderPack<>, decltype(bp)>;
    typename Interp::context_type ctx{{}, bp, {}};

    double result = indexedFold<Interp>(sum, ctx);
    // Sum all elements: 1+2+3+4+5+6 = 21
    expectNear(21.0, result, 1e-10);
  };

  "explicitly nested plates"_test = [] {
    struct Countries {};
    struct Years {};

    // y[c,y] explicitly indexed by both Countries and Years
    auto y = plate<Years>(plate<Countries>(normal(lit(0.0), lit(1.0))));

    using YDims = typename decltype(y)::dims_list;
    static_assert(std::is_same_v<YDims, TypeList<Countries, Years>>);
  };

  "nested plate evaluation with broadcasting"_test = [] {
    struct Countries {};
    struct Years {};

    // Create nodes manually for controlled testing
    // alpha[c] - 1D parameter indexed by Countries
    struct AlphaId {};
    IndexedSymbol<AlphaId, Countries> alpha_sym;

    // y[c,y] - 2D observation indexed by Countries, Years
    struct YId {};
    IndexedSymbol<YId, Countries, Years> y_sym;

    // Expression: sum over both dims of (y - alpha)^2
    // Each y[c,y] uses alpha[c] (broadcasting)
    auto diff_sq = (y_sym - alpha_sym) * (y_sym - alpha_sym);
    auto expr = sumOver<Countries>(sumOver<Years>(diff_sq));

    // Data: alpha = [1, 2] (2 countries)
    //       y = [[1, 2, 3], [4, 5, 6]] (2 countries x 3 years)
    std::vector<double> alpha_data = {1.0, 2.0};
    std::vector<double> y_data = {1, 2, 3, 4, 5, 6};

    IndexedBinding<IndexedSymbol<AlphaId, Countries>, 1> alpha_bind{alpha_data};
    IndexedBinding<IndexedSymbol<YId, Countries, Years>, 2> y_bind{y_data, {2, 3}};

    double result = evaluateIndexed(expr, BinderPack{alpha_bind, y_bind});

    // For c=0: alpha=1, y=[1,2,3], diff_sq = [0,1,4] -> sum=5
    // For c=1: alpha=2, y=[4,5,6], diff_sq = [4,9,16] -> sum=29
    // Total = 5 + 29 = 34
    expectNear(34.0, result, 1e-10);
  };

  // ===========================================================================
  // Observed data
  // ===========================================================================

  "observe creates Observed wrapper"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);

    static_assert(is_observed_v<decltype(theta_obs)>);
    static_assert(!is_observed_v<decltype(theta)>);
  };

  "makeObservedBinding creates correct binding"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);
    auto binding = makeObservedBinding(theta_obs);

    expectEq(binding.size(), 3UL);
    expectNear(0.3, binding.at(0), 1e-10);
  };

  "observed plate log-prob evaluation"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);

    // Get log-prob expression
    auto lp = observedLogProb(theta_obs);

    // Create binding from observed data
    auto binding = makeObservedBinding(theta_obs);

    // Evaluate
    double result = evaluateIndexed(lp, BinderPack{binding});

    // Normalized log Beta: (alpha-1)*log(x) + (beta-1)*log(1-x) - log B(alpha, beta)
    double alpha = 2.0, b = 3.0;
    double log_beta_norm = std::lgamma(alpha) + std::lgamma(b) - std::lgamma(alpha + b);
    auto expected_at = [=](double x) {
      return (alpha - 1.0) * std::log(x) + (b - 1.0) * std::log(1.0 - x) - log_beta_norm;
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  "observed with scalar parameter"_test = [] {
    struct Obs {};
    auto alpha = halfNormal(5.0);
    auto theta = plate<Obs>(beta(alpha, lit(3.0)));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);

    auto lp = observedLogProb(theta_obs);
    auto binding = makeObservedBinding(theta_obs);

    // Evaluate with both scalar and observed bindings
    double result = evaluateIndexed(lp, BinderPack{alpha = 2.0, binding});

    // Normalized log Beta with alpha=2, beta=3
    double a = 2.0, b = 3.0;
    double log_beta_norm = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
    auto expected_at = [=](double x) {
      return (a - 1.0) * std::log(x) + (b - 1.0) * std::log(1.0 - x) - log_beta_norm;
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  // ===========================================================================
  // Multivariate distributions
  // ===========================================================================

  "VectorSymbol has correct types"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 3>;

    static_assert(is_vector_symbol_v<Vec>);
    static_assert(Vec::dim == 3);

    // Access components
    auto x0 = Vec::component<0>();
    auto x1 = Vec::component<1>();
    auto x2 = Vec::component<2>();

    static_assert(is_vector_component_v<decltype(x0)>);
    static_assert(decltype(x0)::index == 0);
    static_assert(decltype(x1)::index == 1);
    static_assert(decltype(x2)::index == 2);
  };

  "VectorBinding stores values"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 3>;
    VectorBinding<Vec> binding{1.0, 2.0, 3.0};

    expectNear(1.0, binding[0], 1e-10);
    expectNear(2.0, binding[1], 1e-10);
    expectNear(3.0, binding[2], 1e-10);
    expectNear(1.0, binding.get<0>(), 1e-10);
    expectNear(3.0, binding.get<2>(), 1e-10);
  };

  "DiagonalMVN log-prob expression"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;

    // 2D MVN with diagonal covariance
    auto mu = std::tuple{lit(0.0), lit(0.0)};
    auto sigma = std::tuple{lit(1.0), lit(2.0)};
    auto dist = diagMvNormal(mu, sigma);

    auto lp = dist.logProbFor(Vec{});
    // lp should be: -0.5 * (x0/1)^2 + -0.5 * (x1/2)^2 = -0.5*x0^2 - 0.125*x1^2

    // Just verify it compiles and is symbolic
    static_assert(Symbolic<decltype(lp)>);
  };

  return TestRegistry::result();
}
