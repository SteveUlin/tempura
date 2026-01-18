#include "bayes/sym/constraints.h"

#include <cmath>
#include <numbers>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes::sym;

// Helper: verify transform(inverse(y)) == y (round-trip)
template <Constraint C>
auto testRoundTrip(double y, double tol = 1e-10) -> bool {
  double x = C::inverse(y);
  double y_recovered = C::transform(x);
  return expectNear(y_recovered, y, tol);
}

// Helper: verify Jacobian via finite differences
// d/dx[transform(x)] ≈ (transform(x+h) - transform(x-h)) / (2h)
// logJacobian(x) should equal log|d/dx[transform(x)]|
template <Constraint C>
auto testJacobianFiniteDiff(double x, double h = 1e-6, double tol = 1e-4) -> bool {
  double numerical_deriv = (C::transform(x + h) - C::transform(x - h)) / (2.0 * h);
  double log_numerical = std::log(std::abs(numerical_deriv));
  double log_analytic = C::logJacobian(x);
  return expectNear(log_analytic, log_numerical, tol);
}

auto main() -> int {
  // ===========================================================================
  // Unconstrained Tests
  // ===========================================================================

  "Unconstrained round-trip"_test = [] {
    testRoundTrip<Unconstrained>(0.0);
    testRoundTrip<Unconstrained>(1.5);
    testRoundTrip<Unconstrained>(-3.7);
    testRoundTrip<Unconstrained>(100.0);
  };

  "Unconstrained Jacobian"_test = [] {
    // Jacobian of identity is 1, so logJacobian = 0
    expectEq(Unconstrained::logJacobian(0.0), 0.0);
    expectEq(Unconstrained::logJacobian(5.0), 0.0);
    expectEq(Unconstrained::logJacobian(-10.0), 0.0);
  };

  // ===========================================================================
  // Positive Tests
  // ===========================================================================

  "Positive round-trip"_test = [] {
    testRoundTrip<Positive>(0.1);
    testRoundTrip<Positive>(1.0);
    testRoundTrip<Positive>(10.0);
    testRoundTrip<Positive>(0.001);
  };

  "Positive transform/inverse"_test = [] {
    // transform(0) = exp(0) = 1
    expectNear(Positive::transform(0.0), 1.0, 1e-10);
    // transform(1) = e
    expectNear(Positive::transform(1.0), std::numbers::e, 1e-10);
    // inverse(1) = log(1) = 0
    expectNear(Positive::inverse(1.0), 0.0, 1e-10);
    // inverse(e) = 1
    expectNear(Positive::inverse(std::numbers::e), 1.0, 1e-10);
  };

  "Positive Jacobian"_test = [] {
    // logJacobian(x) = x (since d/dx[exp(x)] = exp(x), log of that is x)
    expectEq(Positive::logJacobian(0.0), 0.0);
    expectEq(Positive::logJacobian(2.5), 2.5);
    expectEq(Positive::logJacobian(-1.0), -1.0);
  };

  "Positive Jacobian finite diff"_test = [] {
    testJacobianFiniteDiff<Positive>(0.0);
    testJacobianFiniteDiff<Positive>(1.0);
    testJacobianFiniteDiff<Positive>(-2.0);
    testJacobianFiniteDiff<Positive>(3.5);
  };

  // ===========================================================================
  // UnitInterval Tests
  // ===========================================================================

  "UnitInterval round-trip"_test = [] {
    testRoundTrip<UnitInterval>(0.5);
    testRoundTrip<UnitInterval>(0.1);
    testRoundTrip<UnitInterval>(0.9);
    testRoundTrip<UnitInterval>(0.01);
    testRoundTrip<UnitInterval>(0.99);
  };

  "UnitInterval transform/inverse"_test = [] {
    // sigmoid(0) = 0.5
    expectNear(UnitInterval::transform(0.0), 0.5, 1e-10);
    // inverse(0.5) = logit(0.5) = 0
    expectNear(UnitInterval::inverse(0.5), 0.0, 1e-10);
    // sigmoid approaches 1 for large positive x
    expectNear(UnitInterval::transform(10.0), 1.0, 1e-4);
    // sigmoid approaches 0 for large negative x
    expectNear(UnitInterval::transform(-10.0), 0.0, 1e-4);
  };

  "UnitInterval Jacobian at x=0"_test = [] {
    // At x=0: sigmoid(0)=0.5, so Jacobian = 0.5 * 0.5 = 0.25
    // logJacobian = log(0.25) ≈ -1.386
    double expected = std::log(0.25);
    expectNear(UnitInterval::logJacobian(0.0), expected, 1e-10);
  };

  "UnitInterval Jacobian finite diff"_test = [] {
    testJacobianFiniteDiff<UnitInterval>(0.0);
    testJacobianFiniteDiff<UnitInterval>(1.0);
    testJacobianFiniteDiff<UnitInterval>(-1.0);
    testJacobianFiniteDiff<UnitInterval>(2.0);
    testJacobianFiniteDiff<UnitInterval>(-3.0);
  };

  "UnitInterval numerical stability"_test = [] {
    // Test extreme values don't produce NaN/Inf
    double y_large = UnitInterval::transform(50.0);
    expectTrue(std::isfinite(y_large));
    expectNear(y_large, 1.0, 1e-10);

    double y_small = UnitInterval::transform(-50.0);
    expectTrue(std::isfinite(y_small));
    expectNear(y_small, 0.0, 1e-10);

    // logJacobian should also be stable
    double lj_large = UnitInterval::logJacobian(50.0);
    expectTrue(std::isfinite(lj_large));

    double lj_small = UnitInterval::logJacobian(-50.0);
    expectTrue(std::isfinite(lj_small));
  };

  // ===========================================================================
  // Bounded Tests
  // ===========================================================================

  "Bounded<0,10> round-trip"_test = [] {
    using B = Bounded<0, 10>;
    testRoundTrip<B>(5.0);
    testRoundTrip<B>(1.0);
    testRoundTrip<B>(9.0);
    testRoundTrip<B>(0.5);
  };

  "Bounded<-1,1> round-trip"_test = [] {
    using B = Bounded<-1, 1>;
    testRoundTrip<B>(0.0);
    testRoundTrip<B>(-0.5);
    testRoundTrip<B>(0.5);
    testRoundTrip<B>(-0.9);
  };

  "Bounded<0,10> transform/inverse"_test = [] {
    using B = Bounded<0, 10>;
    // At x=0, transform = 0 + 10 * 0.5 = 5
    expectNear(B::transform(0.0), 5.0, 1e-10);
    // inverse(5) should give 0
    expectNear(B::inverse(5.0), 0.0, 1e-10);
    // Large positive x approaches upper bound
    expectNear(B::transform(10.0), 10.0, 1e-3);
    // Large negative x approaches lower bound
    expectNear(B::transform(-10.0), 0.0, 1e-3);
  };

  "Bounded Jacobian finite diff"_test = [] {
    using B = Bounded<0, 10>;
    testJacobianFiniteDiff<B>(0.0);
    testJacobianFiniteDiff<B>(1.0);
    testJacobianFiniteDiff<B>(-1.0);
    testJacobianFiniteDiff<B>(2.0);
  };

  "Bounded<-1,1> Jacobian finite diff"_test = [] {
    using B = Bounded<-1, 1>;
    testJacobianFiniteDiff<B>(0.0);
    testJacobianFiniteDiff<B>(0.5);
    testJacobianFiniteDiff<B>(-0.5);
  };

  // ===========================================================================
  // LowerBounded Tests
  // ===========================================================================

  "LowerBounded<0> round-trip"_test = [] {
    using LB = LowerBounded<0>;
    testRoundTrip<LB>(0.5);
    testRoundTrip<LB>(1.0);
    testRoundTrip<LB>(10.0);
    testRoundTrip<LB>(0.01);
  };

  "LowerBounded<-5> round-trip"_test = [] {
    using LB = LowerBounded<-5>;
    testRoundTrip<LB>(-4.0);
    testRoundTrip<LB>(0.0);
    testRoundTrip<LB>(10.0);
  };

  "LowerBounded transform/inverse"_test = [] {
    using LB = LowerBounded<0>;
    // transform(0) = 0 + exp(0) = 1
    expectNear(LB::transform(0.0), 1.0, 1e-10);
    // inverse(1) = log(1 - 0) = 0
    expectNear(LB::inverse(1.0), 0.0, 1e-10);
  };

  "LowerBounded Jacobian finite diff"_test = [] {
    using LB = LowerBounded<0>;
    testJacobianFiniteDiff<LB>(0.0);
    testJacobianFiniteDiff<LB>(1.0);
    testJacobianFiniteDiff<LB>(-1.0);
    testJacobianFiniteDiff<LB>(2.0);
  };

  // ===========================================================================
  // UpperBounded Tests
  // ===========================================================================

  "UpperBounded<0> round-trip"_test = [] {
    using UB = UpperBounded<0>;
    testRoundTrip<UB>(-0.5);
    testRoundTrip<UB>(-1.0);
    testRoundTrip<UB>(-10.0);
    testRoundTrip<UB>(-0.01);
  };

  "UpperBounded<10> round-trip"_test = [] {
    using UB = UpperBounded<10>;
    testRoundTrip<UB>(9.0);
    testRoundTrip<UB>(5.0);
    testRoundTrip<UB>(0.0);
  };

  "UpperBounded transform/inverse"_test = [] {
    using UB = UpperBounded<0>;
    // transform(0) = 0 - exp(0) = -1
    expectNear(UB::transform(0.0), -1.0, 1e-10);
    // inverse(-1) = -log(0 - (-1)) = -log(1) = 0
    expectNear(UB::inverse(-1.0), 0.0, 1e-10);
  };

  "UpperBounded Jacobian finite diff"_test = [] {
    using UB = UpperBounded<0>;
    testJacobianFiniteDiff<UB>(0.0);
    testJacobianFiniteDiff<UB>(1.0);
    testJacobianFiniteDiff<UB>(-1.0);
    testJacobianFiniteDiff<UB>(2.0);
  };

  // ===========================================================================
  // Concept Satisfaction Tests
  // ===========================================================================

  "Constraint concept satisfaction"_test = [] {
    // These are compile-time checks, but we verify at runtime too
    expectTrue(Constraint<Unconstrained>);
    expectTrue(Constraint<Positive>);
    expectTrue(Constraint<UnitInterval>);
    expectTrue(Constraint<Bounded<0, 1>>);
    expectTrue(Constraint<Bounded<-10, 10>>);
    expectTrue(Constraint<LowerBounded<0>>);
    expectTrue(Constraint<UpperBounded<0>>);
  };

  return TestRegistry::result();
}
