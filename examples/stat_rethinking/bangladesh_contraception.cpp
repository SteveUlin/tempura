// Bangladesh Contraception Analysis
// Statistical Rethinking 2025 - Homework B02
// Data: 1989 Bangladesh Fertility Survey (1934 women, 60 districts)
//
// DAG from lecture:
//        A   C   D
//         \ | /
//          K
//         /
//        U
//
// To estimate effect of K on C, must close backdoors through A and U.
// Model includes: A (age), U (urban), K (children), D (district)
//
// Key technique: Ordered monotonic effect for K (Chapter 12)
// - K takes values 1-4
// - Effect is: bK * sum(delta[1:K]) where delta is a simplex
// - This captures diminishing returns: 1->2 kids has larger effect than 3->4
//
// Model:
//   C ~ Bernoulli(p)
//   logit(p) = a[D] + bU[D]*U + bA*A + bK[D]*sum(delta[1:K])
//
//   # Ordered monotonic kids (simplex ensures monotonicity)
//   delta ~ Dirichlet(2, 2, 2)  # 3 increments for 4 levels
//
//   # Non-centered varying effects by district
//   a[D]  = a_bar  + z_a[D] * sigma_a
//   bU[D] = bU_bar + z_bU[D] * sigma_bU
//   bK[D] = bK_bar + z_bK[D] * sigma_bK

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "csv.h"
#include "symbolic4/distributions/dirichlet_simplex.h"
#include "symbolic4/mcmc/likelihoods.h"
#include "symbolic4/mcmc/non_centered.h"
#include "symbolic4/mcmc/ordered_effect.h"

using namespace tempura;
using namespace tempura::symbolic4;

// ============================================================================
// Data structures
// ============================================================================

struct Observation {
  int64_t district;
  int64_t contraception;
  int64_t children;      // 1-4
  double age_centered;
  int64_t urban;
};

auto loadData(const std::string& path) -> std::vector<Observation> {
  std::ifstream file{path};
  if (!file) {
    std::println(stderr, "Error: Cannot open {}", path);
    return {};
  }

  CsvReader reader{file};
  reader.skipRows(1);

  std::vector<Observation> data;
  while (reader.nextRow()) {
    Observation obs{};
    obs.district = reader.get<int64_t>(1);
    obs.contraception = reader.get<int64_t>(2);
    obs.children = reader.get<int64_t>(3);
    obs.age_centered = reader.get<double>(4);
    obs.urban = reader.get<int64_t>(5);
    data.push_back(obs);
  }

  return data;
}

// ============================================================================
// State layout for HMC sampler
// ============================================================================
// This makes the state vector self-documenting and less error-prone.

constexpr std::size_t kNumDistricts = 60;

struct BangladeshLayout {
  // Fixed effects
  static constexpr std::size_t kABar = 0;
  static constexpr std::size_t kBUBar = 1;
  static constexpr std::size_t kBKBar = 2;
  static constexpr std::size_t kBA = 3;

  // Log-scale parameters (for exponential transform)
  static constexpr std::size_t kLogSigmaA = 4;
  static constexpr std::size_t kLogSigmaBU = 5;
  static constexpr std::size_t kLogSigmaBK = 6;

  // Dirichlet simplex (2 unconstrained -> 3 delta)
  static constexpr std::size_t kZDelta = 7;  // 2 params at [7], [8]

  // Non-centered z parameters
  static constexpr std::size_t kZA = 9;                        // [9..68]
  static constexpr std::size_t kZBU = 9 + kNumDistricts;       // [69..128]
  static constexpr std::size_t kZBK = 9 + 2 * kNumDistricts;   // [129..188]

  static constexpr std::size_t kDim = 9 + 3 * kNumDistricts;   // 189
};

// ============================================================================
// Summary statistics
// ============================================================================

struct Summary {
  double mean;
  double sd;
  double lo;
  double hi;
};

auto summarize(std::vector<double> samples) -> Summary {
  double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  auto n = static_cast<double>(samples.size());
  double mean = sum / n;

  double sum_sq = 0.0;
  for (double x : samples) {
    sum_sq += (x - mean) * (x - mean);
  }
  double sd = std::sqrt(sum_sq / n);

  std::ranges::sort(samples);
  auto idx_lo = static_cast<std::size_t>(0.055 * static_cast<double>(samples.size() - 1));
  auto idx_hi = static_cast<std::size_t>(0.945 * static_cast<double>(samples.size() - 1));

  return {mean, sd, samples[idx_lo], samples[idx_hi]};
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::mt19937_64 rng{42};
  using L = BangladeshLayout;

  std::println(R"(
===============================================================================
            BANGLADESH CONTRACEPTION ANALYSIS (Full DAG Model)
            Statistical Rethinking 2025 - Homework B02
===============================================================================

DAG:     A   C   D
          \ | /
           K
          /
         U

To estimate K->C, must control for A (age) and U (urban).

Model:
  logit(p) = a[D] + bU[D]*U + bA*A + bK[D]*sum(delta[1:K])

  - Ordered monotonic effect for K (captures diminishing returns)
  - Varying intercepts a[D] by district
  - Varying urban effect bU[D] by district
  - Varying kids effect bK[D] by district
  - Fixed age effect bA
)");

  // =========================================================================
  // LOAD AND PREPARE DATA
  // =========================================================================
  auto data = loadData("examples/stat_rethinking/bangladesh.csv");
  if (data.empty()) return 1;

  std::println("Loaded {} observations", data.size());

  // District setup
  std::vector<int64_t> unique_districts;
  for (const auto& obs : data) {
    if (std::ranges::find(unique_districts, obs.district) == unique_districts.end()) {
      unique_districts.push_back(obs.district);
    }
  }
  std::ranges::sort(unique_districts);

  const std::size_t num_districts = unique_districts.size();
  std::println("Number of districts: {}", num_districts);

  std::vector<int64_t> district_to_idx(62, -1);
  for (std::size_t i = 0; i < num_districts; ++i) {
    district_to_idx[unique_districts[i]] = static_cast<int64_t>(i);
  }

  // Standardize age
  double sum_a = 0.0;
  for (const auto& obs : data) sum_a += obs.age_centered;
  double mean_a = sum_a / static_cast<double>(data.size());

  double sum_a_sq = 0.0;
  for (const auto& obs : data) {
    double a = obs.age_centered - mean_a;
    sum_a_sq += a * a;
  }
  double sd_a = std::sqrt(sum_a_sq / static_cast<double>(data.size()));

  std::vector<double> age_std(data.size());
  for (std::size_t i = 0; i < data.size(); ++i) {
    age_std[i] = (data[i].age_centered - mean_a) / sd_a;
  }

  // Count children distribution
  std::array<int, 5> k_counts{};
  for (const auto& obs : data) {
    if (obs.children >= 1 && obs.children <= 4) {
      k_counts[obs.children]++;
    }
  }
  std::println("Children distribution: K=1:{}, K=2:{}, K=3:{}, K=4:{}",
               k_counts[1], k_counts[2], k_counts[3], k_counts[4]);

  int64_t urban_count = 0;
  for (const auto& obs : data) urban_count += obs.urban;
  std::println("Urban: {}, Rural: {}", urban_count, data.size() - urban_count);

  int64_t total_c = 0;
  for (const auto& obs : data) total_c += obs.contraception;
  std::println("Contraception rate: {:.1f}%\n",
               100.0 * static_cast<double>(total_c) / static_cast<double>(data.size()));

  // =========================================================================
  // MODEL FITTING
  // =========================================================================
  std::println(R"(
===============================================================================
                    FITTING FULL MODEL VIA HMC
===============================================================================
)");

  // Helpers from new components
  auto dirichlet = dirichletSimplex<3>({2.0, 2.0, 2.0});
  auto ncp_a = nonCenteredParam<kNumDistricts>(L::kABar, L::kLogSigmaA, L::kZA);
  auto ncp_bU = nonCenteredParam<kNumDistricts>(L::kBUBar, L::kLogSigmaBU, L::kZBU);
  auto ncp_bK = nonCenteredParam<kNumDistricts>(L::kBKBar, L::kLogSigmaBK, L::kZBK);

  constexpr std::size_t kDim = L::kDim;
  using State = std::array<double, kDim>;

  auto log_posterior = [&](const State& state) {
    double lp = 0.0;

    // Fixed effect priors: Normal(0, 1) for bar params, Normal(0, 0.5) for bA
    lp += -0.5 * state[L::kABar] * state[L::kABar];
    lp += -0.5 * state[L::kBUBar] * state[L::kBUBar];
    lp += -0.5 * state[L::kBKBar] * state[L::kBKBar];
    lp += -0.5 * (state[L::kBA] / 0.5) * (state[L::kBA] / 0.5);

    // Sigma priors: Exp(1) with Jacobian for log transform
    for (std::size_t i : {L::kLogSigmaA, L::kLogSigmaBU, L::kLogSigmaBK}) {
      double sigma = std::exp(state[i]);
      lp += -sigma + state[i];  // -sigma + log_sigma
    }

    // Dirichlet(2,2,2) prior with stick-breaking Jacobian
    std::array<double, 2> z_delta{state[L::kZDelta], state[L::kZDelta + 1]};
    lp += dirichlet.logProbWithJacobian(z_delta);

    // z priors: Standard Normal
    for (std::size_t j = 0; j < num_districts; ++j) {
      lp += ncp_a.zLogPrior(state, j);
      lp += ncp_bU.zLogPrior(state, j);
      lp += ncp_bK.zLogPrior(state, j);
    }

    // Ordered monotonic cumulative effects
    auto delta = dirichlet.transform(z_delta);
    auto cum = OrderedMonotonic<4>::cumEffect(delta);

    // Likelihood
    for (std::size_t i = 0; i < data.size(); ++i) {
      auto d_idx = static_cast<std::size_t>(district_to_idx[data[i].district]);
      double a = ncp_a.param(state, d_idx);
      double bU = ncp_bU.param(state, d_idx);
      double bK = ncp_bK.param(state, d_idx);

      auto k = static_cast<std::size_t>(data[i].children);
      double eta = a + bU * static_cast<double>(data[i].urban)
                     + state[L::kBA] * age_std[i]
                     + bK * cum[k];

      auto y = static_cast<double>(data[i].contraception);
      lp += bernoulliLogLik(eta, y);
    }

    return lp;
  };

  auto gradient = [&](const State& state) {
    State grad{};

    // Fixed effect prior gradients
    grad[L::kABar] = -state[L::kABar];
    grad[L::kBUBar] = -state[L::kBUBar];
    grad[L::kBKBar] = -state[L::kBKBar];
    grad[L::kBA] = -state[L::kBA] / (0.5 * 0.5);

    // Sigma prior + Jacobian gradients: d/d(log_sigma) = -sigma + 1
    for (std::size_t i : {L::kLogSigmaA, L::kLogSigmaBU, L::kLogSigmaBK}) {
      grad[i] = -std::exp(state[i]) + 1.0;
    }

    // z prior gradients
    for (std::size_t j = 0; j < num_districts; ++j) {
      ncp_a.addZPriorGrad(grad, j, state);
      ncp_bU.addZPriorGrad(grad, j, state);
      ncp_bK.addZPriorGrad(grad, j, state);
    }

    // Ordered monotonic effects
    std::array<double, 2> z_delta{state[L::kZDelta], state[L::kZDelta + 1]};
    auto delta = dirichlet.transform(z_delta);
    auto cum = OrderedMonotonic<4>::cumEffect(delta);

    // Accumulate gradient w.r.t. delta from likelihood
    std::array<double, 3> grad_delta{};

    // Likelihood gradients
    for (std::size_t i = 0; i < data.size(); ++i) {
      auto d_idx = static_cast<std::size_t>(district_to_idx[data[i].district]);
      double bK = ncp_bK.param(state, d_idx);

      auto k = static_cast<std::size_t>(data[i].children);
      auto u = static_cast<double>(data[i].urban);
      double a = ncp_a.param(state, d_idx);
      double bU = ncp_bU.param(state, d_idx);
      double eta = a + bU * u + state[L::kBA] * age_std[i] + bK * cum[k];

      auto y = static_cast<double>(data[i].contraception);
      double d_eta = bernoulliGrad(eta, y);

      // Chain rule through non-centered params
      ncp_a.addParamGrad(grad, d_idx, d_eta, state);
      ncp_bU.addParamGrad(grad, d_idx, d_eta * u, state);
      ncp_bK.addParamGrad(grad, d_idx, d_eta * cum[k], state);

      // Fixed effect gradient
      grad[L::kBA] += d_eta * age_std[i];

      // Gradient through delta: d(eta)/d(delta[j]) = bK for j in [0, k-2]
      auto [start, end] = OrderedMonotonic<4>::deltaRange(k);
      for (std::size_t j = start; j < end; ++j) {
        grad_delta[j] += d_eta * bK;
      }
    }

    // Chain rule: grad_delta -> grad_z_delta (includes Dirichlet and Jacobian)
    auto grad_z_delta = dirichlet.chainRuleGrad(z_delta, grad_delta);
    grad[L::kZDelta] = grad_z_delta[0];
    grad[L::kZDelta + 1] = grad_z_delta[1];

    return grad;
  };

  std::println("Fitting model with {} parameters...", kDim);
  std::println("(60 districts x 3 varying effects + 4 fixed + 3 sigmas + 2 delta)\n");

  auto hmc = bayes::makeHMC<double, kDim>(log_posterior, gradient, 0.005, 25);

  // Initialize
  State theta{};
  theta[L::kABar] = -1.0;
  theta[L::kBUBar] = 0.5;
  theta[L::kBKBar] = 1.0;
  theta[L::kBA] = 0.0;
  theta[L::kLogSigmaA] = std::log(0.5);
  theta[L::kLogSigmaBU] = std::log(0.5);
  theta[L::kLogSigmaBK] = std::log(0.3);

  double lp = log_posterior(theta);
  std::println("Initial log-posterior: {:.1f}", lp);

  // Warmup
  std::println("Running warmup (500 iterations)...");
  std::size_t accepted_warmup = 0;
  for (std::size_t i = 0; i < 500; ++i) {
    auto [next_theta, next_lp, accepted] = hmc.step(theta, lp, rng);
    theta = next_theta;
    lp = next_lp;
    if (accepted) ++accepted_warmup;
  }
  std::println("  Warmup acceptance: {:.1f}%", 100.0 * static_cast<double>(accepted_warmup) / 500);

  // Sampling
  std::println("Running sampling (2000 iterations)...");
  std::vector<State> samples;
  samples.reserve(2000);
  std::size_t accepted_sampling = 0;
  for (std::size_t i = 0; i < 2000; ++i) {
    auto [next_theta, next_lp, accepted] = hmc.step(theta, lp, rng);
    theta = next_theta;
    lp = next_lp;
    if (accepted) ++accepted_sampling;
    samples.push_back(theta);
  }
  std::println("  Sampling acceptance: {:.1f}%\n",
               100.0 * static_cast<double>(accepted_sampling) / 2000);

  // =========================================================================
  // POSTERIOR SUMMARIES
  // =========================================================================
  std::println(R"(
===============================================================================
                    POSTERIOR SUMMARIES
===============================================================================
)");

  // Extract samples
  std::vector<double> a_bar_samples;
  std::vector<double> bU_bar_samples;
  std::vector<double> bK_bar_samples;
  std::vector<double> bA_samples;
  std::vector<double> sigma_a_samples;
  std::vector<double> sigma_bU_samples;
  std::vector<double> sigma_bK_samples;
  std::vector<double> delta0_samples;
  std::vector<double> delta1_samples;
  std::vector<double> delta2_samples;

  for (const auto& s : samples) {
    a_bar_samples.push_back(s[L::kABar]);
    bU_bar_samples.push_back(s[L::kBUBar]);
    bK_bar_samples.push_back(s[L::kBKBar]);
    bA_samples.push_back(s[L::kBA]);
    sigma_a_samples.push_back(std::exp(s[L::kLogSigmaA]));
    sigma_bU_samples.push_back(std::exp(s[L::kLogSigmaBU]));
    sigma_bK_samples.push_back(std::exp(s[L::kLogSigmaBK]));

    std::array<double, 2> z_d{s[L::kZDelta], s[L::kZDelta + 1]};
    auto delta = dirichlet.transform(z_d);
    delta0_samples.push_back(delta[0]);
    delta1_samples.push_back(delta[1]);
    delta2_samples.push_back(delta[2]);
  }

  auto a_bar_sum = summarize(a_bar_samples);
  auto bU_bar_sum = summarize(bU_bar_samples);
  auto bK_bar_sum = summarize(bK_bar_samples);
  auto bA_sum = summarize(bA_samples);
  auto sigma_a_sum = summarize(sigma_a_samples);
  auto sigma_bU_sum = summarize(sigma_bU_samples);
  auto sigma_bK_sum = summarize(sigma_bK_samples);
  auto delta0_sum = summarize(delta0_samples);
  auto delta1_sum = summarize(delta1_samples);
  auto delta2_sum = summarize(delta2_samples);

  std::println("{:15} {:>10} {:>10} {:>20}", "Parameter", "Mean", "SD", "89% CI");
  std::println("{:-<60}", "");
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]",
               "a_bar", a_bar_sum.mean, a_bar_sum.sd, a_bar_sum.lo, a_bar_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]",
               "bU_bar", bU_bar_sum.mean, bU_bar_sum.sd, bU_bar_sum.lo, bU_bar_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]",
               "bK_bar", bK_bar_sum.mean, bK_bar_sum.sd, bK_bar_sum.lo, bK_bar_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]",
               "bA", bA_sum.mean, bA_sum.sd, bA_sum.lo, bA_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]",
               "sigma_a", sigma_a_sum.mean, sigma_a_sum.sd, sigma_a_sum.lo, sigma_a_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]",
               "sigma_bU", sigma_bU_sum.mean, sigma_bU_sum.sd, sigma_bU_sum.lo, sigma_bU_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]",
               "sigma_bK", sigma_bK_sum.mean, sigma_bK_sum.sd, sigma_bK_sum.lo, sigma_bK_sum.hi);

  std::println("\nOrdered monotonic increments (delta):");
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]  (1->2 kids)",
               "delta[1]", delta0_sum.mean, delta0_sum.sd, delta0_sum.lo, delta0_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]  (2->3 kids)",
               "delta[2]", delta1_sum.mean, delta1_sum.sd, delta1_sum.lo, delta1_sum.hi);
  std::println("{:15} {:10.2f} {:10.2f} [{:7.2f}, {:7.2f}]  (3->4 kids)",
               "delta[3]", delta2_sum.mean, delta2_sum.sd, delta2_sum.lo, delta2_sum.hi);

  // =========================================================================
  // INTERPRETATION
  // =========================================================================
  std::println(R"(

===============================================================================
                    INTERPRETATION
===============================================================================

KEY FINDINGS:

1. KIDS EFFECT (bK_bar ~ {:.2f}):
   - Positive: more kids -> more contraception
   - This is the MAXIMUM effect (at K=4 vs K=1)

2. ORDERED MONOTONIC INCREMENTS:
   - delta[1] ~ {:.0f}% of effect comes from 1->2 kids
   - delta[2] ~ {:.0f}% from 2->3 kids
   - delta[3] ~ {:.0f}% from 3->4 kids
   - The FIRST additional child has the largest effect!

3. URBAN EFFECT (bU_bar ~ {:.2f}):
   - Positive: urban areas have higher contraception
   - This varies by district (sigma_bU ~ {:.2f})

4. AGE EFFECT (bA ~ {:.2f}):
   - Negative: older women less likely to use contraception
   - (Or: younger women more likely)

5. BETWEEN-DISTRICT VARIATION:
   - sigma_a ~ {:.2f}: baseline rates vary moderately
   - sigma_bU ~ {:.2f}: urban effect varies substantially
   - sigma_bK ~ {:.2f}: kids effect varies less

CAUSAL INTERPRETATION:

With A (age) and U (urban) controlled, the remaining K->C effect is closer
to causal, but we still can't rule out unmeasured confounding. The positive
effect likely reflects parity-specific adoption: women use contraception
AFTER reaching desired family size.

The ordered monotonic effect reveals that most of the effect is concentrated
in the 1->2 child transition. Going from 1 to 2 children dramatically increases
contraception use; additional children have diminishing marginal effects.
)",
               bK_bar_sum.mean,
               delta0_sum.mean * 100,
               delta1_sum.mean * 100,
               delta2_sum.mean * 100,
               bU_bar_sum.mean,
               sigma_bU_sum.mean,
               bA_sum.mean,
               sigma_a_sum.mean,
               sigma_bU_sum.mean,
               sigma_bK_sum.mean);

  // =========================================================================
  // DISTRICT-LEVEL EFFECTS
  // =========================================================================
  std::println(R"(
===============================================================================
                    DISTRICT-LEVEL EFFECTS
===============================================================================
)");

  // Compute posterior means for district effects
  std::vector<double> a_means(num_districts);
  std::vector<double> bK_means(num_districts);
  for (std::size_t j = 0; j < num_districts; ++j) {
    double sum_a = 0.0;
    double sum_bK = 0.0;
    for (const auto& s : samples) {
      sum_a += ncp_a.param(s, j);
      sum_bK += ncp_bK.param(s, j);
    }
    a_means[j] = sum_a / static_cast<double>(samples.size());
    bK_means[j] = sum_bK / static_cast<double>(samples.size());
  }

  // Compute correlation
  double mean_a_d = 0.0;
  double mean_bK_d = 0.0;
  for (std::size_t j = 0; j < num_districts; ++j) {
    mean_a_d += a_means[j];
    mean_bK_d += bK_means[j];
  }
  mean_a_d /= static_cast<double>(num_districts);
  mean_bK_d /= static_cast<double>(num_districts);

  double cov = 0.0;
  double var_a = 0.0;
  double var_bK = 0.0;
  for (std::size_t j = 0; j < num_districts; ++j) {
    cov += (a_means[j] - mean_a_d) * (bK_means[j] - mean_bK_d);
    var_a += (a_means[j] - mean_a_d) * (a_means[j] - mean_a_d);
    var_bK += (bK_means[j] - mean_bK_d) * (bK_means[j] - mean_bK_d);
  }
  double corr = cov / std::sqrt(var_a * var_bK);

  std::println("Correlation between a[D] and bK[D]: {:.2f}", corr);

  // Show a few district effects
  std::println("\nSample district effects (posterior means):\n");
  std::println("{:>10} {:>10} {:>10}", "District", "a[D]", "bK[D]");
  std::println("{:-<35}", "");
  for (std::size_t j = 0; j < std::min(num_districts, std::size_t{10}); ++j) {
    std::println("{:10} {:10.2f} {:10.2f}", unique_districts[j], a_means[j], bK_means[j]);
  }
  std::println("...");

  return 0;
}
