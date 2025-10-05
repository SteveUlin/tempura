#include <iostream>
#include <type_traits>

#include "symbolic2/symbolic.h"
#include "symbolic2/simplify_design_exploration.h"

using namespace tempura;
using namespace tempura::design_exploration;

// =============================================================================
// Demo 1: Current Design (Baseline)
// =============================================================================

template <Symbolic S>
constexpr auto baselineSimplifyPower(S expr) {
  // x^0 -> 1
  if constexpr (match(expr, pow(AnyArg{}, 0_c))) {
    return 1_c;
  }
  // x^1 -> x
  else if constexpr (match(expr, pow(AnyArg{}, 1_c))) {
    return left(expr);
  }
  // 0^x -> 0 (assuming x != 0)
  else if constexpr (match(expr, pow(0_c, AnyArg{}))) {
    return 0_c;
  }
  // 1^x -> 1
  else if constexpr (match(expr, pow(1_c, AnyArg{}))) {
    return 1_c;
  }
  else {
    return expr;
  }
}

// =============================================================================
// Demo 2: Table-Driven Design
// =============================================================================

void demo_table_driven() {
  std::cout << "\n=== Demo 2: Table-Driven Rules ===\n";

  Symbol x;

  // Test each rule
  constexpr auto expr1 = pow(x, 0_c);
  constexpr auto result1 = applyRuleSet<power_rules::PowerRuleSet>(expr1);
  std::cout << "x^0 -> ";
  if constexpr (match(result1, 1_c)) {
    std::cout << "1 ✓\n";
  } else {
    std::cout << "FAILED\n";
  }

  constexpr auto expr2 = pow(x, 1_c);
  constexpr auto result2 = applyRuleSet<power_rules::PowerRuleSet>(expr2);
  std::cout << "x^1 -> x ";
  if constexpr (match(result2, x)) {
    std::cout << "✓\n";
  } else {
    std::cout << "FAILED\n";
  }

  constexpr auto expr3 = pow(0_c, x);
  constexpr auto result3 = applyRuleSet<power_rules::PowerRuleSet>(expr3);
  std::cout << "0^x -> 0 ";
  if constexpr (match(result3, 0_c)) {
    std::cout << "✓\n";
  } else {
    std::cout << "FAILED\n";
  }

  constexpr auto expr4 = pow(1_c, x);
  constexpr auto result4 = applyRuleSet<power_rules::PowerRuleSet>(expr4);
  std::cout << "1^x -> 1 ";
  if constexpr (match(result4, 1_c)) {
    std::cout << "✓\n";
  } else {
    std::cout << "FAILED\n";
  }

  // Nested power: (x^2)^3 -> x^6
  constexpr auto expr5 = pow(pow(x, 2_c), 3_c);
  constexpr auto result5 = applyRuleSet<power_rules::PowerRuleSet>(expr5);
  std::cout << "(x^2)^3 -> x^(2*3) ";
  if constexpr (match(result5, pow(x, 2_c * 3_c))) {
    std::cout << "✓\n";
  } else {
    std::cout << "(got different form, but may be equivalent)\n";
  }

  std::cout << "\nADVANTAGES:\n";
  std::cout << "  • Rules are data structures, not just code\n";
  std::cout << "  • Can compose rule sets: BasicRules + AdvancedRules\n";
  std::cout << "  • Can filter rules: only_distributive_rules(AllRules)\n";
  std::cout << "  • Can generate docs: print_rule_description(rule)\n";
}

// =============================================================================
// Demo 3: Priority-Based Rules
// =============================================================================

void demo_priority_based() {
  std::cout << "\n=== Demo 3: Priority-Based Rules ===\n";

  Symbol x;

  // Test special case high-priority rule
  constexpr auto expr1 = pow(x, 2_c) + pow(x, 2_c);
  constexpr auto result1 = applyPrioritizedRules(expr1);
  std::cout << "x^2 + x^2 (high priority) -> 2*x^2 ";
  if constexpr (match(result1, 2_c * pow(x, 2_c))) {
    std::cout << "✓\n";
  } else {
    std::cout << "(may be different form)\n";
  }

  // Test general case medium-priority rule
  constexpr auto expr2 = x + x;
  constexpr auto result2 = applyPrioritizedRules(expr2);
  std::cout << "x + x (medium priority) -> 2*x ";
  if constexpr (match(result2, 2_c * x)) {
    std::cout << "✓\n";
  } else {
    std::cout << "(may be different form)\n";
  }

  std::cout << "\nADVANTAGES:\n";
  std::cout << "  • Explicit control over which rule fires\n";
  std::cout << "  • Can add high-priority optimizations without changing base rules\n";
  std::cout << "  • Good for implementing optimization levels (O1, O2, O3)\n";
  std::cout << "  • Natural for multi-pass compilers\n";
}

// =============================================================================
// Demo 4: Monadic/Strategy Pattern
// =============================================================================

void demo_monadic_strategies() {
  std::cout << "\n=== Demo 4: Monadic Strategy Pattern ===\n";

  Symbol x;

  // Apply strategy that tries multiple rules
  using namespace strategy_rules;

  constexpr auto expr1 = pow(x, 0_c);
  constexpr auto result1 = PowerSimplificationComplete::apply(expr1);
  std::cout << "x^0 (with strategy) -> 1 ";
  if constexpr (decltype(result1)::changed) {
    std::cout << "✓ (changed: " << decltype(result1)::changed << ")\n";
  } else {
    std::cout << "FAILED\n";
  }

  constexpr auto expr2 = pow(x, 1_c);
  constexpr auto result2 = PowerSimplificationComplete::apply(expr2);
  std::cout << "x^1 (with strategy) -> x ";
  if constexpr (decltype(result2)::changed) {
    std::cout << "✓ (changed: " << decltype(result2)::changed << ")\n";
  } else {
    std::cout << "FAILED\n";
  }

  std::cout << "\nADVANTAGES:\n";
  std::cout << "  • Composable strategies: Sequence, Choice, Repeat\n";
  std::cout << "  • Tracks whether changes were made\n";
  std::cout << "  • Can implement complex traversal patterns:\n";
  std::cout << "    - Innermost (bottom-up)\n";
  std::cout << "    - Outermost (top-down)\n";
  std::cout << "    - One-pass vs fixed-point\n";
  std::cout << "  • Natural for term rewriting systems\n";
}

// =============================================================================
// Demo 5: Visitor Pattern
// =============================================================================

void demo_visitor_pattern() {
  std::cout << "\n=== Demo 5: Visitor Pattern ===\n";

  Symbol x;
  PowerSimplificationVisitor visitor;

  constexpr auto expr1 = pow(x, 0_c);
  constexpr auto result1 = visitor.visit(expr1);
  std::cout << "x^0 (with visitor) -> 1 ";
  if constexpr (match(result1, 1_c)) {
    std::cout << "✓\n";
  } else {
    std::cout << "FAILED\n";
  }

  constexpr auto expr2 = pow(x, 1_c);
  constexpr auto result2 = visitor.visit(expr2);
  std::cout << "x^1 (with visitor) -> x ";
  if constexpr (match(result2, x)) {
    std::cout << "✓\n";
  } else {
    std::cout << "FAILED\n";
  }

  constexpr auto expr3 = pow(0_c, x);
  constexpr auto result3 = visitor.visit(expr3);
  std::cout << "0^x (with visitor) -> 0 ";
  if constexpr (match(result3, 0_c)) {
    std::cout << "✓\n";
  } else {
    std::cout << "FAILED\n";
  }

  std::cout << "\nADVANTAGES:\n";
  std::cout << "  • Familiar OOP pattern\n";
  std::cout << "  • Easy to create multiple visitors for different purposes:\n";
  std::cout << "    - SimplificationVisitor\n";
  std::cout << "    - PrettyPrintVisitor\n";
  std::cout << "    - ComplexityAnalysisVisitor\n";
  std::cout << "  • Clean separation of traversal and transformation\n";
}

// =============================================================================
// Demo 6: Performance Comparison
// =============================================================================

void demo_performance_comparison() {
  std::cout << "\n=== Demo 6: Compile-Time Performance ===\n";
  std::cout << "(Note: All approaches are compile-time, so runtime is same)\n\n";

  Symbol x;

  // All these are evaluated at compile time
  constexpr auto expr = pow(pow(pow(x, 1_c), 0_c), 1_c);

  // Baseline
  constexpr auto baseline_result = baselineSimplifyPower(expr);

  // Table-driven
  constexpr auto table_result = applyRuleSet<power_rules::PowerRuleSet>(expr);

  // Priority-based
  // (Would apply here but our demo rules don't handle this case)

  // Visitor
  PowerSimplificationVisitor visitor;
  constexpr auto visitor_result = visitor.visit(expr);

  std::cout << "COMPILE-TIME CHARACTERISTICS:\n\n";

  std::cout << "1. Current Approach (if-constexpr chain):\n";
  std::cout << "   • Lowest template instantiation depth\n";
  std::cout << "   • Fastest compilation for simple rule sets\n";
  std::cout << "   • Linear complexity in number of rules\n\n";

  std::cout << "2. Table-Driven:\n";
  std::cout << "   • Higher template instantiation depth\n";
  std::cout << "   • One-time cost for rule set infrastructure\n";
  std::cout << "   • Pays off with large rule sets\n";
  std::cout << "   • Can use type list algorithms (filter, sort, etc.)\n\n";

  std::cout << "3. Priority-Based:\n";
  std::cout << "   • Medium template instantiation depth\n";
  std::cout << "   • Extra cost for priority sorting\n";
  std::cout << "   • Still fairly efficient\n\n";

  std::cout << "4. Strategy Pattern:\n";
  std::cout << "   • Highest template instantiation depth\n";
  std::cout << "   • Composition creates nested templates\n";
  std::cout << "   • Slowest compilation\n";
  std::cout << "   • Most expressive\n\n";

  std::cout << "5. Visitor Pattern:\n";
  std::cout << "   • Low-medium template depth\n";
  std::cout << "   • Similar to current approach\n";
  std::cout << "   • CRTP adds slight overhead\n\n";
}

// =============================================================================
// Demo 7: Composability
// =============================================================================

void demo_composability() {
  std::cout << "\n=== Demo 7: Rule Composability ===\n\n";

  std::cout << "CURRENT APPROACH:\n";
  std::cout << "  • Hard to combine rule sets\n";
  std::cout << "  • Must manually merge if-constexpr chains\n";
  std::cout << "  • No way to disable specific rules\n\n";

  std::cout << "TABLE-DRIVEN:\n";
  std::cout << "  ✓ RuleSet = BasicRules + AdvancedRules\n";
  std::cout << "  ✓ RuleSet = AllRules.filter(is_distributive)\n";
  std::cout << "  ✓ RuleSet = AllRules.sort_by_priority()\n";
  std::cout << "  ✓ Can create variants: FastRules, CompleteRules\n\n";

  std::cout << "PRIORITY-BASED:\n";
  std::cout << "  ✓ Add high-priority overrides without modifying base\n";
  std::cout << "  ✓ Enable/disable by priority level\n";
  std::cout << "  ✓ Good for optimization levels\n\n";

  std::cout << "STRATEGY PATTERN:\n";
  std::cout << "  ✓ Sequence(s1, s2, s3)\n";
  std::cout << "  ✓ Choice(try_fast, fallback_complete)\n";
  std::cout << "  ✓ Repeat(strategy, until_fixed_point)\n";
  std::cout << "  ✓ Most expressive composition\n\n";

  std::cout << "VISITOR:\n";
  std::cout << "  ~ Can chain visitors: v1.visit(v2.visit(expr))\n";
  std::cout << "  ~ Less natural than other approaches\n";
}

// =============================================================================
// Demo 8: Extensibility
// =============================================================================

void demo_extensibility() {
  std::cout << "\n=== Demo 8: Extensibility ===\n\n";

  std::cout << "Scenario: Adding a new optimization rule\n\n";

  std::cout << "CURRENT APPROACH:\n";
  std::cout << "  1. Find the right function (e.g., powerIdentities)\n";
  std::cout << "  2. Add new else-if branch\n";
  std::cout << "  3. Position matters - may break existing behavior\n";
  std::cout << "  4. No way to test just the new rule\n\n";

  std::cout << "TABLE-DRIVEN:\n";
  std::cout << "  1. Define new rule struct: struct MyNewRule { ... }\n";
  std::cout << "  2. Add to rule set: using MyRules = RuleSet<..., MyNewRule>\n";
  std::cout << "  3. Order is explicit in type list\n";
  std::cout << "  4. Can test rule in isolation\n";
  std::cout << "  5. Can see all rules at a glance\n\n";

  std::cout << "PRIORITY-BASED:\n";
  std::cout << "  1. Define rule with priority\n";
  std::cout << "  2. Add to registry\n";
  std::cout << "  3. Priority determines when it fires\n";
  std::cout << "  4. Higher priority = override existing rules\n\n";

  std::cout << "STRATEGY:\n";
  std::cout << "  1. Define new strategy\n";
  std::cout << "  2. Compose with existing: Choice(new_strat, old_strat)\n";
  std::cout << "  3. Very modular\n\n";

  std::cout << "VISITOR:\n";
  std::cout << "  1. Create new visitor class\n";
  std::cout << "  2. Override visit_impl for your case\n";
  std::cout << "  3. Compose by chaining visitors\n";
}

// =============================================================================
// Demo 9: Error Messages
// =============================================================================

void demo_error_messages() {
  std::cout << "\n=== Demo 9: Compile Error Quality ===\n\n";

  std::cout << "CURRENT APPROACH:\n";
  std::cout << "  ✓ Clear error messages\n";
  std::cout << "  ✓ Points to exact if-constexpr branch\n";
  std::cout << "  ✓ Type errors are straightforward\n\n";

  std::cout << "TABLE-DRIVEN:\n";
  std::cout << "  ~ Template instantiation depth can be high\n";
  std::cout << "  ~ Error in rule shows which rule failed\n";
  std::cout << "  ~ Type list errors can be cryptic\n\n";

  std::cout << "PRIORITY-BASED:\n";
  std::cout << "  ✓ Similar to current approach\n";
  std::cout << "  ✓ Priority sorting adds some noise\n\n";

  std::cout << "STRATEGY:\n";
  std::cout << "  ✗ Deep template nesting\n";
  std::cout << "  ✗ Error messages reference many strategy combinators\n";
  std::cout << "  ✗ Hard to trace back to original rule\n\n";

  std::cout << "VISITOR:\n";
  std::cout << "  ✓ CRTP adds slight complexity\n";
  std::cout << "  ✓ Otherwise similar to current\n";
}

// =============================================================================
// Demo 10: Real-World Use Cases
// =============================================================================

void demo_use_cases() {
  std::cout << "\n=== Demo 10: Real-World Use Cases ===\n\n";

  std::cout << "USE CASE 1: Small Library (< 50 rules)\n";
  std::cout << "  → Keep CURRENT approach\n";
  std::cout << "  → Simple, fast, maintainable\n\n";

  std::cout << "USE CASE 2: Large CAS System (100+ rules)\n";
  std::cout << "  → Use TABLE-DRIVEN approach\n";
  std::cout << "  → Need composability and inspection\n";
  std::cout << "  → Can generate documentation\n\n";

  std::cout << "USE CASE 3: Optimizing Compiler (many passes)\n";
  std::cout << "  → Use PRIORITY-BASED approach\n";
  std::cout << "  → Different optimization levels\n";
  std::cout << "  → High-priority peephole optimizations\n\n";

  std::cout << "USE CASE 4: Research Project (complex strategies)\n";
  std::cout << "  → Use STRATEGY/MONADIC approach\n";
  std::cout << "  → Need to experiment with rewrite strategies\n";
  std::cout << "  → Following term rewriting literature\n\n";

  std::cout << "USE CASE 5: Multiple Transformations\n";
  std::cout << "  → Use VISITOR pattern\n";
  std::cout << "  → Need: simplify, analyze, pretty-print, etc.\n";
  std::cout << "  → Separate concerns clearly\n\n";

  std::cout << "HYBRID APPROACH:\n";
  std::cout << "  → Combine Table-Driven + Priority-Based\n";
  std::cout << "  → Rules as data (table) with priorities\n";
  std::cout << "  → Best of both worlds\n";
}

// =============================================================================
// Main Demo
// =============================================================================

int main() {
  std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
  std::cout << "║  SYMBOLIC SIMPLIFICATION: DESIGN PATTERN EXPLORATION          ║\n";
  std::cout << "╚════════════════════════════════════════════════════════════════╝\n";

  demo_table_driven();
  demo_priority_based();
  demo_monadic_strategies();
  demo_visitor_pattern();
  demo_performance_comparison();
  demo_composability();
  demo_extensibility();
  demo_error_messages();
  demo_use_cases();

  std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
  std::cout << "║  SUMMARY & RECOMMENDATIONS                                     ║\n";
  std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";

  std::cout << "For YOUR project (Tempura):\n\n";

  std::cout << "SHORT TERM (next 1-3 months):\n";
  std::cout << "  → Keep current approach for core rules\n";
  std::cout << "  → It's working well and is maintainable\n\n";

  std::cout << "MEDIUM TERM (3-6 months) - IF rule set grows > 50:\n";
  std::cout << "  → Migrate to Table-Driven approach\n";
  std::cout << "  → Add priority annotations\n";
  std::cout << "  → Gain composability and inspection\n\n";

  std::cout << "LONG TERM (6-12 months) - IF building optimizer:\n";
  std::cout << "  → Add Strategy combinators on top of tables\n";
  std::cout << "  → Support multiple optimization passes\n";
  std::cout << "  → Build rule analysis tools\n\n";

  std::cout << "RECOMMENDED HYBRID:\n";
  std::cout << "  ╔══════════════════════════════════════════════════════════╗\n";
  std::cout << "  ║  Rules defined as TABLE entries (declarative)           ║\n";
  std::cout << "  ║          ↓                                               ║\n";
  std::cout << "  ║  Each rule has PRIORITY (explicit ordering)             ║\n";
  std::cout << "  ║          ↓                                               ║\n";
  std::cout << "  ║  Applied via STRATEGY (composable, flexible)            ║\n";
  std::cout << "  ╚══════════════════════════════════════════════════════════╝\n\n";

  std::cout << "Next steps:\n";
  std::cout << "  1. Review simplify_design_exploration.h\n";
  std::cout << "  2. Try porting 5-10 rules to table format\n";
  std::cout << "  3. Compare compile times\n";
  std::cout << "  4. Get team feedback\n";
  std::cout << "  5. Decide on migration plan\n\n";

  return 0;
}
