// Tests for let.h - identity tracking and LetNode
#include "symbolic4/core.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/let.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "LetNode wraps expression with identity"_test = [] {
    Symbol<struct X> x;
    auto expr = x * x;
    auto bound = let<struct MySquare>(expr);

    static_assert(is_let_node_v<decltype(bound)>);
    static_assert(!is_expression_v<decltype(bound)>);
    static_assert(!is_atom_v<decltype(bound)>);
  };

  "LetNode preserves underlying expression"_test = [] {
    Symbol<struct X> x;
    auto expr = x + x;
    auto bound = let<struct Sum>(expr);

    // The inner expression should be retrievable
    auto inner = bound.expr();
    static_assert(is_expression_v<decltype(inner)>);
    static_assert(std::is_same_v<get_op_t<decltype(inner)>, AddOp>);
  };

  "LetNode::sym returns symbol with same identity"_test = [] {
    auto bound = let<struct MyId>(Constant<42>{});
    auto sym = decltype(bound)::sym();

    static_assert(is_atom_v<decltype(sym)>);
    static_assert(std::is_same_v<get_id_t<decltype(sym)>, struct MyId>);
  };

  "IdSet operations"_test = [] {
    // Empty set
    using Empty = IdSet<>;
    static_assert(Empty::size == 0);
    static_assert(!id_set_contains_v<struct A, Empty>);

    // Single element
    using Single = IdSet<struct A>;
    static_assert(Single::size == 1);
    static_assert(id_set_contains_v<struct A, Single>);
    static_assert(!id_set_contains_v<struct B, Single>);

    // Multiple elements
    using Multi = IdSet<struct A, struct B, struct C>;
    static_assert(Multi::size == 3);
    static_assert(id_set_contains_v<struct A, Multi>);
    static_assert(id_set_contains_v<struct B, Multi>);
    static_assert(id_set_contains_v<struct C, Multi>);
    static_assert(!id_set_contains_v<struct D, Multi>);
  };

  "IdSet insert"_test = [] {
    using Empty = IdSet<>;

    // Insert into empty
    using One = id_set_insert_t<struct A, Empty>;
    static_assert(One::size == 1);
    static_assert(id_set_contains_v<struct A, One>);

    // Insert new
    using Two = id_set_insert_t<struct B, One>;
    static_assert(Two::size == 2);
    static_assert(id_set_contains_v<struct A, Two>);
    static_assert(id_set_contains_v<struct B, Two>);

    // Insert duplicate (should not change size)
    using Still2 = id_set_insert_t<struct A, Two>;
    static_assert(Still2::size == 2);
  };

  "get_let_id and get_let_expr type traits"_test = [] {
    Symbol<struct X> x;
    auto bound = let<struct Square>(x * x);

    using Id = get_let_id_t<decltype(bound)>;
    using Expr = get_let_expr_t<decltype(bound)>;

    static_assert(std::is_same_v<Id, struct Square>);
    static_assert(is_expression_v<Expr>);
    static_assert(std::is_same_v<get_op_t<Expr>, MulOp>);
  };

  "LetNode with TEMPURA_LET macro"_test = [] {
    Symbol<struct X> x;
    TEMPURA_LET(squared, x * x);

    static_assert(is_let_node_v<decltype(squared)>);

    // The macro generates a unique id type
    using Id = get_let_id_t<decltype(squared)>;
    static_assert(!std::is_same_v<Id, void>);
  };

  // ============================================================================
  // Evaluation through LetNode
  // ============================================================================

  "LetNode can be evaluated"_test = [] {
    // A simple interpreter that understands LetNode
    Symbol<struct X> x;
    auto bound = let<struct Square>(x * x);

    // To evaluate, we need to handle LetNode - just evaluate the inner expr
    auto inner = bound.expr();
    auto result = evaluate(inner, x = 3.0);
    expectNear(result, 9.0);
  };

  return TestRegistry::result();
}
