#pragma once

#include "core.h"
#include "matching.h"
#include "operators.h"
#include "constants.h"
#include "accessors.h"

/*
 * AESTHETIC VARIATIONS: Working Implementations
 *
 * This file contains working code for 6 different aesthetic approaches
 * to defining table-driven simplification rules.
 */

namespace tempura {
namespace aesthetic_variations {

// =============================================================================
// VARIATION 1: MINIMAL (Baseline)
// =============================================================================

namespace minimal {

// No additional infrastructure needed
template <typename... Rules>
struct RuleSet {};

// Example rules
struct Rule_Pow_Zero {
  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return match(expr, pow(AnyArg{}, 0_c));
  }

  template <Symbolic S>
  static constexpr auto apply(S) {
    return 1_c;
  }

  static constexpr const char* description = "x^0 → 1";
  static constexpr int priority = 100;
  static constexpr const char* category = "power";
};

struct Rule_Pow_One {
  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return match(expr, pow(AnyArg{}, 1_c));
  }

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return left(expr);
  }

  static constexpr const char* description = "x^1 → x";
  static constexpr int priority = 100;
  static constexpr const char* category = "power";
};

struct Rule_Mul_Zero {
  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return match(expr, AnyArg{} * 0_c);
  }

  template <Symbolic S>
  static constexpr auto apply(S) {
    return 0_c;
  }

  static constexpr const char* description = "x*0 → 0";
  static constexpr int priority = 150;
  static constexpr const char* category = "multiply";
};

using MinimalRules = RuleSet<Rule_Pow_Zero, Rule_Pow_One, Rule_Mul_Zero>;

} // namespace minimal

// =============================================================================
// VARIATION 2: CRTP BASE CLASS
// =============================================================================

namespace crtp_base {

// Base class with helpers
template <typename Derived>
struct Rule {
  // Default metadata
  static constexpr int priority = 50;
  static constexpr const char* category = "general";

  // Extraction helpers
  template <Symbolic S>
  static constexpr auto L(S expr) { return left(expr); }

  template <Symbolic S>
  static constexpr auto R(S expr) { return right(expr); }

  template <Symbolic S>
  static constexpr auto arg(S expr) { return operand(expr); }

  // Constant checks
  template <Symbolic S>
  static constexpr bool is_zero(S expr) { return match(expr, 0_c); }

  template <Symbolic S>
  static constexpr bool is_one(S expr) { return match(expr, 1_c); }

  // Delegate to derived
  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return Derived::matches_impl(expr);
  }

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return Derived::apply_impl(expr);
  }
};

// Example rules
struct Rule_Pow_Zero : Rule<Rule_Pow_Zero> {
  static constexpr const char* description = "x^0 → 1";
  static constexpr int priority = 100;
  static constexpr const char* category = "power";

  template <Symbolic S>
  static constexpr auto matches_impl(S expr) {
    return match(expr, pow(AnyArg{}, 0_c));
  }

  template <Symbolic S>
  static constexpr auto apply_impl(S) {
    return 1_c;
  }
};

struct Rule_Pow_One : Rule<Rule_Pow_One> {
  static constexpr const char* description = "x^1 → x";
  static constexpr int priority = 100;
  static constexpr const char* category = "power";

  template <Symbolic S>
  static constexpr auto matches_impl(S expr) {
    return match(expr, pow(AnyArg{}, 1_c));
  }

  template <Symbolic S>
  static constexpr auto apply_impl(S expr) {
    return L(expr);  // Use helper!
  }
};

struct Rule_Mul_Zero : Rule<Rule_Mul_Zero> {
  static constexpr const char* description = "x*0 → 0";
  static constexpr int priority = 150;
  static constexpr const char* category = "multiply";

  template <Symbolic S>
  static constexpr auto matches_impl(S expr) {
    return match(expr, AnyArg{} * 0_c);
  }

  template <Symbolic S>
  static constexpr auto apply_impl(S) {
    return 0_c;
  }
};

// Complex example showing helper benefits
struct Rule_Pow_Pow : Rule<Rule_Pow_Pow> {
  static constexpr const char* description = "(x^a)^b → x^(a*b)";
  static constexpr int priority = 80;
  static constexpr const char* category = "power";

  template <Symbolic S>
  static constexpr auto matches_impl(S expr) {
    return match(expr, pow(pow(AnyArg{}, AnyArg{}), AnyArg{}));
  }

  template <Symbolic S>
  static constexpr auto apply_impl(S expr) {
    // Compare: L(L(expr)), R(L(expr)), R(expr)
    // vs:      left(left(expr)), right(left(expr)), right(expr)
    return pow(L(L(expr)), R(L(expr)) * R(expr));
  }
};

template <typename... Rules>
struct RuleSet {};

using CRTPRules = RuleSet<Rule_Pow_Zero, Rule_Pow_One, Rule_Mul_Zero, Rule_Pow_Pow>;

} // namespace crtp_base

// =============================================================================
// VARIATION 3: MACRO DSL
// =============================================================================

namespace macro_dsl {

// Base with helpers
template <typename Derived>
struct MacroRuleBase {
  template <Symbolic S>
  static constexpr auto L(S expr) { return left(expr); }

  template <Symbolic S>
  static constexpr auto R(S expr) { return right(expr); }

  template <Symbolic S>
  static constexpr auto arg(S expr) { return operand(expr); }
};

// Simple rule macro
#define DEFINE_RULE(Name, Desc, Pri, Cat, Pattern, Transform) \
  struct Name : MacroRuleBase<Name> { \
    static constexpr const char* description = Desc; \
    static constexpr int priority = Pri; \
    static constexpr const char* category = Cat; \
    \
    template <Symbolic S> \
    static constexpr auto matches(S expr) { \
      return match(expr, Pattern); \
    } \
    \
    template <Symbolic S> \
    static constexpr auto apply([[maybe_unused]] S expr) Transform \
  }

// Complex rule macro
#define BEGIN_RULE(Name, Desc, Pri, Cat, Pattern) \
  struct Name : MacroRuleBase<Name> { \
    static constexpr const char* description = Desc; \
    static constexpr int priority = Pri; \
    static constexpr const char* category = Cat; \
    \
    template <Symbolic S> \
    static constexpr auto matches(S expr) { \
      return match(expr, Pattern); \
    } \
    \
    template <Symbolic S> \
    static constexpr auto apply([[maybe_unused]] S expr)

#define END_RULE }

// Example rules - notice how concise!
DEFINE_RULE(Rule_Pow_Zero, "x^0 → 1", 100, "power",
  pow(AnyArg{}, 0_c),
  { return 1_c; }
);

DEFINE_RULE(Rule_Pow_One, "x^1 → x", 100, "power",
  pow(AnyArg{}, 1_c),
  { return L(expr); }
);

DEFINE_RULE(Rule_Mul_Zero, "x*0 → 0", 150, "multiply",
  AnyArg{} * 0_c,
  { return 0_c; }
);

DEFINE_RULE(Rule_Add_Zero, "x+0 → x", 100, "addition",
  AnyArg{} + 0_c,
  { return L(expr); }
);

// Complex rule
BEGIN_RULE(Rule_Pow_Pow, "(x^a)^b → x^(a*b)", 80, "power",
  pow(pow(AnyArg{}, AnyArg{}), AnyArg{}))
{
  constexpr auto x = L(L(expr));
  constexpr auto a = R(L(expr));
  constexpr auto b = R(expr);
  return pow(x, a * b);
}
END_RULE;

// Distribution rule
BEGIN_RULE(Rule_Distribute, "a*(b+c) → a*b+a*c", 60, "distribute",
  AnyArg{} * (AnyArg{} + AnyArg{}))
{
  constexpr auto a = L(expr);
  constexpr auto b = L(R(expr));
  constexpr auto c = R(R(expr));
  return (a * b) + (a * c);
}
END_RULE;

template <typename... Rules>
struct RuleSet {};

using MacroRules = RuleSet<
  Rule_Pow_Zero,
  Rule_Pow_One,
  Rule_Mul_Zero,
  Rule_Add_Zero,
  Rule_Pow_Pow,
  Rule_Distribute
>;

} // namespace macro_dsl

// =============================================================================
// VARIATION 4: TEMPLATE HELPERS
// =============================================================================

namespace template_helpers {

// Helper: Rules that return constants
template <auto Pattern, auto Result>
struct ConstantRule {
  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return match(expr, Pattern);
  }

  template <Symbolic S>
  static constexpr auto apply(S) {
    return Result;
  }
};

// Helper: Rules that extract part of expression
template <auto Pattern, auto Extractor>
struct ExtractRule {
  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return match(expr, Pattern);
  }

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return Extractor(expr);
  }
};

// Helper: Rules with custom transform
template <auto Pattern, auto Transform>
struct CustomRule {
  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return match(expr, Pattern);
  }

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return Transform(expr);
  }
};

// Metadata decorator
template <typename Rule, int Pri, auto Desc, auto Cat>
struct WithMetadata : Rule {
  static constexpr int priority = Pri;
  static constexpr const char* description = Desc;
  static constexpr const char* category = Cat;
};

// Common extractors
constexpr auto extract_left = [](auto expr) { return left(expr); };
constexpr auto extract_right = [](auto expr) { return right(expr); };
constexpr auto extract_arg = [](auto expr) { return operand(expr); };

// String literals for metadata
inline constexpr const char pow_zero_desc[] = "x^0 → 1";
inline constexpr const char pow_one_desc[] = "x^1 → x";
inline constexpr const char mul_zero_desc[] = "x*0 → 0";
inline constexpr const char add_zero_desc[] = "x+0 → x";
inline constexpr const char pow_pow_desc[] = "(x^a)^b → x^(a*b)";
inline constexpr const char power_cat[] = "power";
inline constexpr const char mult_cat[] = "multiply";
inline constexpr const char add_cat[] = "addition";

// Example rules - very declarative!
using Rule_Pow_Zero = WithMetadata<
  ConstantRule<pow(AnyArg{}, 0_c), 1_c>,
  100, pow_zero_desc, power_cat
>;

using Rule_Pow_One = WithMetadata<
  ExtractRule<pow(AnyArg{}, 1_c), extract_left>,
  100, pow_one_desc, power_cat
>;

using Rule_Mul_Zero = WithMetadata<
  ConstantRule<AnyArg{} * 0_c, 0_c>,
  150, mul_zero_desc, mult_cat
>;

using Rule_Add_Zero = WithMetadata<
  ExtractRule<AnyArg{} + 0_c, extract_left>,
  100, add_zero_desc, add_cat
>;

// Custom transform
constexpr auto pow_pow_transform = [](auto expr) {
  constexpr auto x = left(left(expr));
  constexpr auto a = right(left(expr));
  constexpr auto b = right(expr);
  return pow(x, a * b);
};

using Rule_Pow_Pow = WithMetadata<
  CustomRule<pow(pow(AnyArg{}, AnyArg{}), AnyArg{}), pow_pow_transform>,
  80, pow_pow_desc, power_cat
>;

template <typename... Rules>
struct RuleSet {};

using TemplateRules = RuleSet<
  Rule_Pow_Zero,
  Rule_Pow_One,
  Rule_Mul_Zero,
  Rule_Add_Zero,
  Rule_Pow_Pow
>;

} // namespace template_helpers

// =============================================================================
// VARIATION 5: CONSTEXPR LAMBDA (C++26-like simulation)
// =============================================================================

namespace constexpr_lambda {

// Simulate C++26 designated initializers with lambdas
// (Real C++26 would be even cleaner)

template <typename PatternLambda, typename TransformLambda>
struct RuleDef {
  PatternLambda pattern_lambda;
  TransformLambda transform_lambda;
  const char* description;
  int priority;
  const char* category;
};

template <typename RuleDef>
struct Rule {
  static constexpr auto def = RuleDef{};
  static constexpr const char* description = def.description;
  static constexpr int priority = def.priority;
  static constexpr const char* category = def.category;

  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return def.pattern_lambda(expr);
  }

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return def.transform_lambda(expr);
  }
};

// Helper to create rule definition
template <typename PatternLambda, typename TransformLambda>
constexpr auto make_rule_def(
    PatternLambda pattern,
    TransformLambda transform,
    const char* desc,
    int pri,
    const char* cat) {
  return RuleDef<PatternLambda, TransformLambda>{
    pattern, transform, desc, pri, cat
  };
}

// Example rules
constexpr auto pow_zero_def = make_rule_def(
  [](auto expr) { return match(expr, pow(AnyArg{}, 0_c)); },
  [](auto) { return 1_c; },
  "x^0 → 1", 100, "power"
);

constexpr auto pow_one_def = make_rule_def(
  [](auto expr) { return match(expr, pow(AnyArg{}, 1_c)); },
  [](auto expr) { return left(expr); },
  "x^1 → x", 100, "power"
);

constexpr auto mul_zero_def = make_rule_def(
  [](auto expr) { return match(expr, AnyArg{} * 0_c); },
  [](auto) { return 0_c; },
  "x*0 → 0", 150, "multiply"
);

constexpr auto pow_pow_def = make_rule_def(
  [](auto expr) { return match(expr, pow(pow(AnyArg{}, AnyArg{}), AnyArg{})); },
  [](auto expr) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(expr);
    return pow(x, a * b);
  },
  "(x^a)^b → x^(a*b)", 80, "power"
);

using Rule_Pow_Zero = Rule<decltype(pow_zero_def)>;
using Rule_Pow_One = Rule<decltype(pow_one_def)>;
using Rule_Mul_Zero = Rule<decltype(mul_zero_def)>;
using Rule_Pow_Pow = Rule<decltype(pow_pow_def)>;

template <typename... Rules>
struct RuleSet {};

using LambdaRules = RuleSet<
  Rule_Pow_Zero,
  Rule_Pow_One,
  Rule_Mul_Zero,
  Rule_Pow_Pow
>;

} // namespace constexpr_lambda

// =============================================================================
// VARIATION 6: DECLARATIVE BUILDER
// =============================================================================

namespace declarative_builder {

// Forward declarations
template <typename Pattern>
struct PatternBuilder;

template <typename Pattern, typename Transform>
struct RuleBuilder;

// Final rule structure
template <typename Pattern, typename Transform,
          auto Desc, int Pri, auto Cat>
struct FinalRule {
  Pattern pattern;
  Transform transform;

  template <Symbolic S>
  static constexpr auto matches(S expr) {
    return match(expr, Pattern{});
  }

  template <Symbolic S>
  static constexpr auto apply(S expr) {
    return Transform::apply(expr);
  }

  static constexpr const char* description = Desc;
  static constexpr int priority = Pri;
  static constexpr const char* category = Cat;
};

// Rule builder with fluent API
template <typename Pattern, typename Transform>
struct RuleBuilder {
  // Build with metadata
  template <auto Desc, int Pri = 50, auto Cat = "general">
  constexpr auto build() const {
    return FinalRule<Pattern, Transform, Desc, Pri, Cat>{};
  }
};

// Pattern builder
template <typename Pattern>
struct PatternBuilder {
  // Transform to constant
  template <auto Value>
  constexpr auto to() const {
    struct TransformImpl {
      template <Symbolic S>
      static constexpr auto apply(S) { return Constant<Value>{}; }
    };
    return RuleBuilder<Pattern, TransformImpl>{};
  }

  // Transform with lambda
  template <typename Lambda>
  constexpr auto to(Lambda) const {
    return RuleBuilder<Pattern, Lambda>{};
  }

  // Common transforms
  constexpr auto to_left() const {
    struct TransformImpl {
      template <Symbolic S>
      static constexpr auto apply(S expr) { return left(expr); }
    };
    return RuleBuilder<Pattern, TransformImpl>{};
  }

  constexpr auto to_right() const {
    struct TransformImpl {
      template <Symbolic S>
      static constexpr auto apply(S expr) { return right(expr); }
    };
    return RuleBuilder<Pattern, TransformImpl>{};
  }

  constexpr auto to_arg() const {
    struct TransformImpl {
      template <Symbolic S>
      static constexpr auto apply(S expr) { return operand(expr); }
    };
    return RuleBuilder<Pattern, TransformImpl>{};
  }
};

// Entry point
template <typename Pattern>
constexpr auto when(Pattern) {
  return PatternBuilder<Pattern>{};
}

// String literals
inline constexpr const char pow_zero_desc[] = "x^0 → 1";
inline constexpr const char pow_one_desc[] = "x^1 → x";
inline constexpr const char mul_zero_desc[] = "x*0 → 0";
inline constexpr const char add_zero_desc[] = "x+0 → x";
inline constexpr const char pow_pow_desc[] = "(x^a)^b → x^(a*b)";
inline constexpr const char power_cat[] = "power";
inline constexpr const char mult_cat[] = "multiply";
inline constexpr const char add_cat[] = "addition";

// Example rules - VERY concise!
using Rule_Pow_Zero = decltype(
  when(pow(AnyArg{}, 0_c)).to<1>().build<pow_zero_desc, 100, power_cat>()
);

using Rule_Pow_One = decltype(
  when(pow(AnyArg{}, 1_c)).to_left().build<pow_one_desc, 100, power_cat>()
);

using Rule_Mul_Zero = decltype(
  when(AnyArg{} * 0_c).to<0>().build<mul_zero_desc, 150, mult_cat>()
);

using Rule_Add_Zero = decltype(
  when(AnyArg{} + 0_c).to_left().build<add_zero_desc, 100, add_cat>()
);

// Custom transform
struct PowPowTransform {
  template <Symbolic S>
  static constexpr auto apply(S expr) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(expr);
    return pow(x, a * b);
  }
};

using Rule_Pow_Pow = decltype(
  when(pow(pow(AnyArg{}, AnyArg{}), AnyArg{}))
    .to(PowPowTransform{})
    .build<pow_pow_desc, 80, power_cat>()
);

template <typename... Rules>
struct RuleSet {};

using BuilderRules = RuleSet<
  Rule_Pow_Zero,
  Rule_Pow_One,
  Rule_Mul_Zero,
  Rule_Add_Zero,
  Rule_Pow_Pow
>;

} // namespace declarative_builder

// =============================================================================
// APPLICATION ENGINE (shared by all variations)
// =============================================================================

// Apply first matching rule from a rule set
template <typename RuleSet, Symbolic S, std::size_t Index = 0>
constexpr auto applyRuleSetAesthetic(S expr);

// Helper to get Nth type from rule set
template <std::size_t N, typename... Rules>
struct GetRule;

template <typename First, typename... Rest>
struct GetRule<0, First, Rest...> {
  using type = First;
};

template <std::size_t N, typename First, typename... Rest>
struct GetRule<N, First, Rest...> {
  using type = typename GetRule<N-1, Rest...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, minimal::RuleSet<Rules...>> {
  using type = typename GetRule<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, crtp_base::RuleSet<Rules...>> {
  using type = typename GetRule<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, macro_dsl::RuleSet<Rules...>> {
  using type = typename GetRule<N, Rules...>::type;
};

// Helper to get N-th rule from a RuleSet - base implementation
template <std::size_t N, typename... Rules>
struct GetRuleImpl;

template <typename First, typename... Rest>
struct GetRuleImpl<0, First, Rest...> {
  using type = First;
};

template <std::size_t N, typename First, typename... Rest>
struct GetRuleImpl<N, First, Rest...> {
  using type = typename GetRuleImpl<N-1, Rest...>::type;
};

// Public interface - unwrap RuleSet
template <std::size_t N, typename RuleSet>
struct GetRule;

template <std::size_t N, typename... Rules>
struct GetRule<N, minimal::RuleSet<Rules...>> {
  using type = typename GetRuleImpl<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, crtp_base::RuleSet<Rules...>> {
  using type = typename GetRuleImpl<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, macro_dsl::RuleSet<Rules...>> {
  using type = typename GetRuleImpl<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, template_helpers::RuleSet<Rules...>> {
  using type = typename GetRuleImpl<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, constexpr_lambda::RuleSet<Rules...>> {
  using type = typename GetRuleImpl<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, declarative_builder::RuleSet<Rules...>> {
  using type = typename GetRuleImpl<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, constexpr_lambda::RuleSet<Rules...>> {
  using type = typename GetRule<N, Rules...>::type;
};

template <std::size_t N, typename... Rules>
struct GetRule<N, declarative_builder::RuleSet<Rules...>> {
  using type = typename GetRule<N, Rules...>::type;
};

// Count rules
template <typename RuleSet>
struct RuleCount;

template <typename... Rules>
struct RuleCount<minimal::RuleSet<Rules...>> {
  static constexpr std::size_t value = sizeof...(Rules);
};

template <typename... Rules>
struct RuleCount<crtp_base::RuleSet<Rules...>> {
  static constexpr std::size_t value = sizeof...(Rules);
};

template <typename... Rules>
struct RuleCount<macro_dsl::RuleSet<Rules...>> {
  static constexpr std::size_t value = sizeof...(Rules);
};

template <typename... Rules>
struct RuleCount<template_helpers::RuleSet<Rules...>> {
  static constexpr std::size_t value = sizeof...(Rules);
};

template <typename... Rules>
struct RuleCount<constexpr_lambda::RuleSet<Rules...>> {
  static constexpr std::size_t value = sizeof...(Rules);
};

template <typename... Rules>
struct RuleCount<declarative_builder::RuleSet<Rules...>> {
  static constexpr std::size_t value = sizeof...(Rules);
};

// Apply rule set
template <typename RuleSet, Symbolic S, std::size_t Index>
constexpr auto applyRuleSetAesthetic(S expr) {
  if constexpr (Index >= RuleCount<RuleSet>::value) {
    return expr;  // No rules matched
  } else {
    using Rule = typename GetRule<Index, RuleSet>::type;
    if constexpr (Rule::matches(expr)) {
      return Rule::apply(expr);
    } else {
      return applyRuleSetAesthetic<RuleSet, S, Index + 1>(expr);
    }
  }
}

} // namespace aesthetic_variations
} // namespace tempura
