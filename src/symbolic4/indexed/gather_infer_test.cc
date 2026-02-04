// Integration tests for gather with indexed evaluation
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "unit.h"

#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Gather with hierarchical model structure
  // ===========================================================================

  "gather in hierarchical model - manual log-prob"_test = [] {
    // Mini hierarchical model:
    // a[g] for g in Groups (3 groups)
    // idx[i] for i in Obs (4 observations) - group assignments
    //
    // We compute: SumOver<Obs>(gather(a, idx)^2) as a simple test
    // This exercises gather across plates.

    struct Groups {};
    struct Obs {};

    IndexedSymbol<struct AId, Groups> a;
    IndexedSymbol<struct IdxId, Obs> idx;

    // Simple sum of squared gathered values
    auto gathered = gather(a, idx);
    auto total = sumOver<Obs>(gathered * gathered);

    // Data
    std::vector<double> a_data = {1.0, 2.0, 3.0};  // Group values
    std::vector<double> idx_data = {0.0, 1.0, 1.0, 2.0};  // Group assignments

    IndexedBinding<decltype(a), 1> a_bind{std::span<const double>(a_data)};
    IndexedBinding<decltype(idx), 1> idx_bind{std::span<const double>(idx_data)};

    double result = evaluateIndexed(total, a_bind, idx_bind);

    // Manual calculation:
    // gather(a, idx) = [a[0], a[1], a[1], a[2]] = [1, 2, 2, 3]
    // sum of squares = 1 + 4 + 4 + 9 = 18
    expectNear(18.0, result, 1e-10);
  };

  "gather gradient via finite differences"_test = [] {
    // When we differentiate w.r.t. a[j], the gradient accumulates contributions
    // from all observations that use a[j].
    //
    // Test: f = SumOver<Obs>(gather(a, idx)^2)
    // d/d(a[j])[f] = SumOver<Obs where idx[i]==j>(2 * a[j])
    //              = (count of i where idx[i]==j) * 2 * a[j]

    struct Groups {};
    struct Obs {};

    IndexedSymbol<struct AId, Groups> a;
    IndexedSymbol<struct IdxId, Obs> idx;

    auto f = sumOver<Obs>(gather(a, idx) * gather(a, idx));

    std::vector<double> a_data = {1.0, 2.0};  // 2 groups
    std::vector<double> idx_data = {0.0, 0.0, 1.0};  // [group0, group0, group1]

    IndexedBinding<decltype(a), 1> a_bind{std::span<const double>(a_data)};
    IndexedBinding<decltype(idx), 1> idx_bind{std::span<const double>(idx_data)};

    double f_val = evaluateIndexed(f, a_bind, idx_bind);

    // f = a[0]^2 + a[0]^2 + a[1]^2 = 1 + 1 + 4 = 6
    expectNear(6.0, f_val, 1e-10);

    // Gradient d/d(a[0])[f] = 2*a[0] + 2*a[0] = 4*1.0 = 4.0
    // Gradient d/d(a[1])[f] = 2*a[1] = 2*2.0 = 4.0

    auto check_gradient = [&](SizeT group_idx, double expected_grad) {
      double eps = 1e-6;
      std::vector<double> a_plus = a_data;
      std::vector<double> a_minus = a_data;
      a_plus[group_idx] += eps;
      a_minus[group_idx] -= eps;

      IndexedBinding<decltype(a), 1> a_plus_bind{std::span<const double>(a_plus)};
      IndexedBinding<decltype(a), 1> a_minus_bind{std::span<const double>(a_minus)};

      double f_plus = evaluateIndexed(f, a_plus_bind, idx_bind);
      double f_minus = evaluateIndexed(f, a_minus_bind, idx_bind);

      double numerical_grad = (f_plus - f_minus) / (2 * eps);
      expectNear(expected_grad, numerical_grad, 1e-4);
    };

    check_gradient(0, 4.0);  // d/d(a[0])
    check_gradient(1, 4.0);  // d/d(a[1])
  };

  return TestRegistry::result();
}
