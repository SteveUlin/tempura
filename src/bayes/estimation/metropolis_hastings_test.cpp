#include "bayes/estimation/metropolis_hastings.h"

#include <cmath>
#include <numeric>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "sample from standard normal"_test = [] {
    // Target: N(0,1), log p(x) = -x²/2 (ignoring constants)
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{1.0});

    Chain chain{kernel, std::mt19937_64{42}, 0.0};

    chain.advance(1000);  // burn-in
    auto samples = chain.take(10'000);

    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    double mean = sum / samples.size();

    double sum_sq = 0.0;
    for (auto x : samples) {
      sum_sq += (x - mean) * (x - mean);
    }
    double var = sum_sq / samples.size();

    // Standard error of mean = 1/√N ≈ 0.01, tolerance ~10 SE
    expectNear(0.0, mean, 0.1);
    expectNear(1.0, var, 0.15);
  };

  "sample from shifted normal"_test = [] {
    // Target: N(5, 2²), log p(x) = -(x-5)²/8
    auto log_target = [](double x) { return -(x - 5.0) * (x - 5.0) / 8.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, GaussianWalk{1.5});

    Chain chain{kernel, std::mt19937_64{123}, 5.0};

    chain.advance(1000);  // burn-in
    auto samples = chain.take(10'000);

    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    double mean = sum / samples.size();

    double sum_sq = 0.0;
    for (auto x : samples) {
      sum_sq += (x - mean) * (x - mean);
    }
    double var = sum_sq / samples.size();

    // True mean = 5, variance = 4
    expectNear(5.0, mean, 0.2);
    expectNear(4.0, var, 0.5);
  };

  "acceptance rate tracking"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{0.5});

    Chain chain{kernel, std::mt19937_64{999}, 0.0};

    chain.advance(500);  // burn-in
    auto [samples, rate] = chain.takeWithStats(5000);

    // With step size 0.5 for N(0,1), acceptance should be reasonably high
    expectTrue(rate > 0.3);
    expectTrue(rate < 0.9);
    expectEq(samples.size(), std::size_t{5000});
  };

  "cumulative acceptance stats"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{1.0});

    Chain chain{kernel, std::mt19937_64{42}, 0.0};

    chain.advance(1000);
    expectEq(chain.totalSteps(), std::size_t{1000});
    expectTrue(chain.acceptedSteps() > 0);

    double rate = chain.acceptanceRate();
    expectTrue(rate > 0.2);
    expectTrue(rate < 0.9);

    chain.resetStats();
    expectEq(chain.totalSteps(), std::size_t{0});
    expectEq(chain.acceptedSteps(), std::size_t{0});
  };

  "thinning reduces autocorrelation"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };

    auto kernel1 = makeMetropolisHastings<double>(log_target, UniformWalk{1.0});
    auto kernel2 = makeMetropolisHastings<double>(log_target, UniformWalk{1.0});

    Chain chain1{kernel1, std::mt19937_64{42}, 0.0};
    Chain chain2{kernel2, std::mt19937_64{42}, 0.0};

    chain1.advance(100);
    chain2.advance(100);

    auto samples_thin1 = chain1.take(1000, /*thin=*/1);
    auto samples_thin5 = chain2.take(1000, /*thin=*/5);

    // Compute lag-1 autocorrelation
    auto autocorr = [](const std::vector<double>& s) {
      double mean = std::accumulate(s.begin(), s.end(), 0.0) /
                    static_cast<double>(s.size());
      double var = 0.0;
      double cov = 0.0;
      for (std::size_t i = 0; i < s.size(); ++i) {
        var += (s[i] - mean) * (s[i] - mean);
        if (i > 0) {
          cov += (s[i] - mean) * (s[i - 1] - mean);
        }
      }
      return cov / var;
    };

    double ac1 = autocorr(samples_thin1);
    double ac5 = autocorr(samples_thin5);

    // Thinning should reduce autocorrelation
    expectTrue(std::abs(ac5) < std::abs(ac1));
  };

  "bimodal distribution"_test = [] {
    // Target: mixture of N(-3, 0.5²) and N(3, 0.5²)
    auto log_target = [](double x) {
      using std::exp;
      using std::log;
      double p1 = exp(-(x + 3.0) * (x + 3.0) / 0.5);
      double p2 = exp(-(x - 3.0) * (x - 3.0) / 0.5);
      return log(p1 + p2);
    };

    // Larger step size helps jump between modes
    auto kernel = makeMetropolisHastings<double>(log_target, GaussianWalk{2.0});

    Chain chain{kernel, std::mt19937_64{777}, 0.0};

    chain.advance(2000);  // burn-in
    auto samples = chain.take(20'000);

    // Count samples in each mode
    int left_count = 0;
    int right_count = 0;
    for (auto x : samples) {
      if (x < 0)
        ++left_count;
      else
        ++right_count;
    }

    // Should visit both modes (at least 20% in each)
    double left_frac = static_cast<double>(left_count) / samples.size();
    double right_frac = static_cast<double>(right_count) / samples.size();
    expectTrue(left_frac > 0.2);
    expectTrue(right_frac > 0.2);
  };

  "next() single step iteration"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{2.0});

    Chain chain{kernel, std::mt19937_64{42}, 0.0};

    std::vector<double> samples;
    for (int i = 0; i < 200; ++i) {
      samples.push_back(chain.next());
    }

    expectEq(chain.totalSteps(), std::size_t{200});
    expectTrue(chain.acceptedSteps() > 30);
    expectTrue(chain.acceptedSteps() < 170);
  };

  "burn-in discards initial samples"_test = [] {
    // Start far from mode, verify burn-in helps
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{0.5});

    Chain chain{kernel, std::mt19937_64{42}, 10.0};  // start at x=10, far from mode

    chain.advance(500);  // burn-in
    auto samples = chain.take(100);

    // After burn-in, samples should be near 0
    double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / 100;
    expectTrue(std::abs(mean) < 1.0);
  };

  "chain state access"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{1.0});

    Chain chain{kernel, std::mt19937_64{42}, 5.0};

    expectEq(chain.state(), 5.0);
    expectNear(chain.logProb(), -12.5);  // -5²/2

    chain.next();
    // State should have changed (or stayed same if rejected)
    expectTrue(chain.totalSteps() == 1);
  };

  "setState for checkpointing"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{1.0});

    Chain chain{kernel, std::mt19937_64{42}, 0.0};

    chain.advance(100);
    double saved_state = chain.state();

    chain.advance(100);
    expectNeq(chain.state(), saved_state);  // State has moved

    chain.setState(saved_state);
    expectEq(chain.state(), saved_state);
    expectNear(chain.logProb(), -saved_state * saved_state / 2.0);
  };

  "handles -inf log probability"_test = [] {
    // Target with bounded support: only x > 0 is valid
    auto log_target = [](double x) {
      if (x <= 0.0) return -std::numeric_limits<double>::infinity();
      return -x;  // Exponential(1)
    };

    auto kernel = makeMetropolisHastings<double>(log_target, UniformWalk{2.0});

    Chain chain{kernel, std::mt19937_64{42}, 1.0};  // Start in valid region

    // Run chain - should never get stuck even with proposals into x <= 0
    chain.advance(1000);
    auto samples = chain.take(1000);

    // All samples should be positive
    for (auto x : samples) {
      expectTrue(x > 0.0);
    }

    // Mean of Exp(1) is 1
    double sum = 0.0;
    for (auto x : samples) sum += x;
    expectNear(1.0, sum / samples.size(), 0.2);
  };

  "reproducibility with same seed"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };

    auto kernel1 = makeMetropolisHastings<double>(log_target, GaussianWalk{0.5});
    auto kernel2 = makeMetropolisHastings<double>(log_target, GaussianWalk{0.5});

    Chain chain1{kernel1, std::mt19937_64{12345}, 0.0};
    Chain chain2{kernel2, std::mt19937_64{12345}, 0.0};

    auto samples1 = chain1.take(100);
    auto samples2 = chain2.take(100);

    // Should produce identical samples
    for (std::size_t i = 0; i < samples1.size(); ++i) {
      expectEq(samples1[i], samples2[i]);
    }
  };

  return TestRegistry::result();
}
