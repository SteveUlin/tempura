// BMJ Weekend Submissions - Symbolic Auto-Diff Implementation
// Statistical Rethinking 2025 - Homework B01
//
// Demonstrates symbolic4's SumOver and differentiate() for automatic gradients.
//
// Model:
//   a ~ Normal(-2, 1)
//   sigma ~ Exponential(1)
//   b[L] ~ Normal(0, sigma)
//   logit(p[L]) = a + b[L]
//   k[L] ~ Binomial(n[L], p[L])

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <numeric>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "bayes/estimation/metropolis_hastings.h"
#include "csv.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/symbolic4.h"

using namespace tempura;
using namespace tempura::symbolic4;

// ============================================================================
// Data
// ============================================================================

struct CountryData {
  std::string name;
  int64_t country_id;
  int64_t total;
  int64_t weekend;
};

auto loadData(const std::string& path) -> std::vector<CountryData> {
  std::ifstream file{path};
  if (!file) {
    std::println(stderr, "Error: Cannot open {}", path);
    return {};
  }

  CsvReader reader{file};
  reader.skipRows(1);

  std::vector<CountryData> countries(39);
  while (reader.nextRow()) {
    std::string name = reader.get<std::string>(2);
    int64_t id = reader.get<int64_t>(3);
    int64_t weekend = reader.get<int64_t>(4);

    countries[id].name = name;
    countries[id].country_id = id;
    countries[id].total++;
    countries[id].weekend += weekend;
  }

  std::vector<CountryData> result;
  for (const auto& c : countries) {
    if (c.total > 0) result.push_back(c);
  }
  return result;
}

// ============================================================================
// Model
// ============================================================================
//
//   a ~ Normal(-2, 1)
//   sigma ~ Exponential(1)
//   b[i] ~ Normal(0, sigma)
//   k[i] ~ Binomial(n[i], sigmoid(a + b[i]))
//
// ============================================================================

struct Countries {};

// Parameters
inline auto a_rv = normal<struct A>(-2_c, 1_c);          // a ~ Normal(-2, 1)
inline auto sigma_rv = exponential<struct Sigma>(1_c);  // sigma ~ Exp(1), auto Jacobian
inline auto sigma = sigma_rv.constrainedExpr();         // exp(z_sigma)

inline auto b_rv = plate<Countries, struct B>(normal(0_c, sigma));  // b[i] ~ Normal(0, sigma)

// Data symbol
constexpr IndexedSymbol<struct N, Countries> n_sym;

// Derived quantity
inline auto p = 1_c / (1_c + exp(-(a_rv.constrainedExpr() + b_rv.sym())));

// Likelihood
inline auto k_rv = plate<Countries, struct K>(binomial(n_sym, p));  // k[i] ~ Binomial(n[i], p[i])

// Log-posterior - automatic!
inline auto log_posterior = jointLogProb(a_rv, sigma_rv, b_rv, k_rv);

// ============================================================================
// Evaluation Helper
// ============================================================================

auto makeBindings(double a_val, double z_sigma_val,
                  std::span<const double> b_vals,
                  std::span<const double> k_vals,
                  std::span<const double> n_vals) {
  using BSym = decltype(b_rv.freeSym());
  using KSym = decltype(k_rv.freeSym());
  using NSym = decltype(n_sym);
  return BinderPack{
      a_rv.freeSym() = a_val,
      sigma_rv.freeSym() = z_sigma_val,
      IndexedBinding<BSym, 1>{b_vals},
      IndexedBinding<KSym, 1>{k_vals},
      IndexedBinding<NSym, 1>{n_vals}};
}

template <typename Expr>
auto eval(Expr& expr, double a_val, double z_sigma_val,
          std::span<const double> b_vals,
          std::span<const double> k_vals,
          std::span<const double> n_vals) -> double {
  auto bindings = makeBindings(a_val, z_sigma_val, b_vals, k_vals, n_vals);
  return evaluateIndexed(expr, bindings);
}

// ============================================================================
// Summary Statistics
// ============================================================================

struct Summary {
  double mean, lo, hi;
};

auto summarize(std::vector<double> samples) -> Summary {
  double mean = std::accumulate(samples.begin(), samples.end(), 0.0) /
                static_cast<double>(samples.size());
  std::ranges::sort(samples);
  auto lo_idx = static_cast<std::size_t>(0.055 * static_cast<double>(samples.size() - 1));
  auto hi_idx = static_cast<std::size_t>(0.945 * static_cast<double>(samples.size() - 1));
  return {mean, samples[lo_idx], samples[hi_idx]};
}

auto invLogit(double x) -> double { return 1.0 / (1.0 + std::exp(-x)); }

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::mt19937_64 rng{42};

  std::println(R"(
=== BMJ WEEKEND SUBMISSIONS - SYMBOLIC AUTO-DIFF ===

Model: logit(p[L]) = a + b[L]
  a ~ Normal(-2, 1)
  sigma ~ Exponential(1)
  b[L] ~ Normal(0, sigma)
  k[L] ~ Binomial(n[L], p[L])
)");

  // Load data
  auto countries = loadData("examples/stat_rethinking/BMJSubmissions.csv");
  if (countries.empty()) return 1;

  std::ranges::sort(countries, [](const auto& l, const auto& r) {
    return l.country_id < r.country_id;
  });

  const std::size_t kNumCountries = countries.size();
  std::println("Loaded {} countries\n", kNumCountries);

  std::vector<double> k_obs(kNumCountries), n_obs(kNumCountries);
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    k_obs[i] = static_cast<double>(countries[i].weekend);
    n_obs[i] = static_cast<double>(countries[i].total);
  }

  // Gradients computed automatically via differentiate()
  auto grad_a = differentiate(log_posterior, a_rv.freeSym());
  auto grad_z_sigma = differentiate(log_posterior, sigma_rv.freeSym());

  std::println("Symbolic expressions:");
  std::println("  log_posterior: {} bytes", sizeof(log_posterior));
  std::println("  grad_a: {} bytes", sizeof(grad_a));
  std::println("  grad_z_sigma: {} bytes\n", sizeof(grad_z_sigma));

  // Verify gradients via finite difference
  std::vector<double> test_b(kNumCountries);
  std::normal_distribution<double> normal_dist{0.0, 0.1};
  for (auto& b_i : test_b) b_i = normal_dist(rng);

  double test_a = -2.0, test_z_sigma = std::log(0.5);
  constexpr double kEps = 1e-6;

  double sym_grad_a = eval(grad_a, test_a, test_z_sigma, test_b, k_obs, n_obs);
  double fd_grad_a = (eval(log_posterior, test_a + kEps, test_z_sigma, test_b, k_obs, n_obs) -
                      eval(log_posterior, test_a - kEps, test_z_sigma, test_b, k_obs, n_obs)) /
                     (2 * kEps);

  double sym_grad_ls = eval(grad_z_sigma, test_a, test_z_sigma, test_b, k_obs, n_obs);
  double fd_grad_ls = (eval(log_posterior, test_a, test_z_sigma + kEps, test_b, k_obs, n_obs) -
                       eval(log_posterior, test_a, test_z_sigma - kEps, test_b, k_obs, n_obs)) /
                      (2 * kEps);

  std::println("Gradient verification:");
  std::println("  grad_a:         symbolic={:.6f}  fin_diff={:.6f}  diff={:.2e}",
               sym_grad_a, fd_grad_a, std::abs(sym_grad_a - fd_grad_a));
  std::println("  grad_z_sigma: symbolic={:.6f}  fin_diff={:.6f}  diff={:.2e}\n",
               sym_grad_ls, fd_grad_ls, std::abs(sym_grad_ls - fd_grad_ls));

  // HMC Sampling
  constexpr std::size_t kStateDim = 2 + 38;
  using State = std::array<double, kStateDim>;

  auto log_posterior_fn = [&](const State& state) {
    return eval(log_posterior, state[0], state[1],
                std::span{state.data() + 2, kNumCountries}, k_obs, n_obs);
  };

  auto grad_fn = [&](const State& state) {
    State grad{};
    std::vector<double> b_vals(state.begin() + 2, state.end());

    // Symbolic gradients for scalar params
    grad[0] = eval(grad_a, state[0], state[1], b_vals, k_obs, n_obs);
    grad[1] = eval(grad_z_sigma, state[0], state[1], b_vals, k_obs, n_obs);

    // Finite diff for indexed params (symbolic indexed gradients not yet supported)
    constexpr double eps = 1e-7;
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double orig = b_vals[i];
      b_vals[i] = orig + eps;
      double lp_plus = eval(log_posterior, state[0], state[1], b_vals, k_obs, n_obs);
      b_vals[i] = orig - eps;
      double lp_minus = eval(log_posterior, state[0], state[1], b_vals, k_obs, n_obs);
      grad[2 + i] = (lp_plus - lp_minus) / (2 * eps);
      b_vals[i] = orig;
    }
    return grad;
  };

  auto hmc = bayes::makeHMC<double, kStateDim>(log_posterior_fn, grad_fn, 0.01, 20);

  State initial{};
  initial[0] = -2.0;
  initial[1] = std::log(0.5);

  bayes::Chain chain{hmc, rng, initial};

  std::println("HMC: burn-in 500, collecting 2000 samples...");
  chain.advance(500);
  chain.resetStats();
  auto [samples, acceptance_rate] = chain.takeWithStats(2000);
  std::println("Acceptance rate: {:.1f}%\n", acceptance_rate * 100);

  // Posterior summaries
  std::vector<double> global_mean_samples, sigma_samples;
  for (const auto& s : samples) {
    global_mean_samples.push_back(invLogit(s[0]));
    sigma_samples.push_back(std::exp(s[1]));
  }

  auto global_summary = summarize(global_mean_samples);
  auto sigma_summary = summarize(sigma_samples);

  std::println("=== POSTERIOR ESTIMATES ===\n");
  std::println("Global intercept: inv_logit(a) = {:.1f}% [{:.1f}%, {:.1f}%]",
               global_summary.mean * 100, global_summary.lo * 100, global_summary.hi * 100);
  std::println("Between-country SD: sigma = {:.3f} [{:.3f}, {:.3f}]\n",
               sigma_summary.mean, sigma_summary.lo, sigma_summary.hi);

  // Country rankings
  std::vector<std::vector<double>> p_samples(kNumCountries);
  for (const auto& s : samples) {
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      p_samples[i].push_back(invLogit(s[0] + s[2 + i]));
    }
  }

  struct Result {
    std::string name;
    int64_t n;
    Summary posterior;
  };

  std::vector<Result> results;
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    results.push_back({countries[i].name, countries[i].total, summarize(p_samples[i])});
  }

  std::ranges::sort(results, [](const auto& l, const auto& r) {
    return l.posterior.mean > r.posterior.mean;
  });

  std::println("{:18} {:>6}  {:>22}", "Country", "n", "Posterior (89% CI)");
  std::println("{:-<50}", "");
  for (const auto& r : results) {
    std::println("{:18} {:6}  {:5.1f}% [{:4.1f}%, {:4.1f}%]",
                 r.name.substr(0, 18), r.n,
                 r.posterior.mean * 100, r.posterior.lo * 100, r.posterior.hi * 100);
  }

  return 0;
}
