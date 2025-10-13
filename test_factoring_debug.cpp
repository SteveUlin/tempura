#include <print>

#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

int main() {
  constexpr Symbol x;
  constexpr Symbol a;
  constexpr Symbol b;
  constexpr auto ctx = default_context();

  // Test: x*a + x*b should factor to x*(a+b)
  constexpr auto expr = x * a + x * b;

  std::print("Testing: x*a + x*b\n\n");

  // Try applying AdditionRuleCategories::Factoring directly
  constexpr auto factoring_result =
      AdditionRuleCategories::Factoring.apply(expr, ctx);
  constexpr bool factoring_changed =
      !std::is_same_v<decltype(factoring_result), decltype(expr)>;
  std::print("AdditionRuleCategories::Factoring changed expression: {}\n",
             factoring_changed);

  constexpr auto expected = x * (a + b);
  constexpr bool factoring_matches = match(factoring_result, expected);
  std::print("Factoring result matches x*(a+b): {}\n\n", factoring_matches);

  // Try ascent_collection (which includes factoring)
  constexpr auto ascent_result = ascent_collection.apply(expr, ctx);
  constexpr bool ascent_changed =
      !std::is_same_v<decltype(ascent_result), decltype(expr)>;
  std::print("ascent_collection changed expression: {}\n", ascent_changed);
  constexpr bool ascent_matches = match(ascent_result, expected);
  std::print("ascent_collection result matches x*(a+b): {}\n\n",
             ascent_matches);

  // Try all ascent_rules
  constexpr auto ascent_all_result = ascent_rules.apply(expr, ctx);
  constexpr bool ascent_all_changed =
      !std::is_same_v<decltype(ascent_all_result), decltype(expr)>;
  std::print("ascent_rules changed expression: {}\n", ascent_all_changed);
  constexpr bool ascent_all_matches = match(ascent_all_result, expected);
  std::print("ascent_rules result matches x*(a+b): {}\n\n", ascent_all_matches);

  // Try bottomup with ascent_rules
  std::print("Testing bottomup:\n");
  constexpr auto bottomup_result = bottomup(ascent_rules).apply(expr, ctx);
  constexpr bool bottomup_matches = match(bottomup_result, expected);
  std::print("bottomup(ascent_rules) matches x*(a+b): {}\n", bottomup_matches);

  // Try the ascent_phase
  constexpr auto ascent_phase_result = ascent_phase.apply(expr, ctx);
  constexpr bool ascent_phase_matches = match(ascent_phase_result, expected);
  std::print("ascent_phase matches x*(a+b): {}\n", ascent_phase_matches);

  // Try two_phase_core (without fixpoint)
  constexpr auto core_result = two_phase_core.apply(expr, ctx);
  constexpr bool core_matches = match(core_result, expected);
  std::print("two_phase_core matches x*(a+b): {}\n", core_matches);

  // Apply a second iteration to the factored form
  std::print("\nTesting what happens to x*(a+b) on second iteration:\n");
  constexpr auto factored_form = x * (a + b);
  constexpr auto second_iter = two_phase_core.apply(factored_form, ctx);
  constexpr bool second_same_as_input =
      std::is_same_v<decltype(second_iter), decltype(factored_form)>;
  std::print("Second iteration preserves x*(a+b): {}\n", second_same_as_input);
  constexpr bool second_back_to_unfactored = match(second_iter, x * a + x * b);
  std::print("Second iteration reverts to x*a + x*b: {}\n",
             second_back_to_unfactored);

  // Now try with full pipeline
  std::print("\nTesting full pipeline:\n");
  constexpr auto result = two_stage_simplify(expr, ctx);
  constexpr bool matches_xa_plus_b = match(result, x * (a + b));
  constexpr bool matches_a_plus_b_x = match(result, (a + b) * x);
  constexpr bool matches_xb_plus_a = match(result, x * (b + a));
  constexpr bool matches_b_plus_a_x = match(result, (b + a) * x);
  constexpr bool matches_unfactored = match(result, x * a + x * b);

  std::print("two_stage_simplify matches x*(a+b): {}\n", matches_xa_plus_b);
  std::print("two_stage_simplify matches (a+b)*x: {}\n", matches_a_plus_b_x);
  std::print("two_stage_simplify matches x*(b+a): {}\n", matches_xb_plus_a);
  std::print("two_stage_simplify matches (b+a)*x: {}\n", matches_b_plus_a_x);
  std::print("two_stage_simplify matches x*a + x*b (unfactored): {}\n",
             matches_unfactored);

  std::print("\nConclusion: The result is in SOME other form!\n");

  return 0;
}
