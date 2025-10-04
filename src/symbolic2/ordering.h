#pragma once

#include "accessors.h"
#include "constants.h"
#include "core.h"
#include "matching.h"
#include "meta/containers.h"
#include "meta/type_id.h"
#include "meta/utility.h"
#include "operators.h"

// Symbolic expression ordering and comparison

namespace tempura {

// --- Symbolic Less Than ---

enum class PartialOrdering : char { kLess, kEqual, kGreater };

template <typename LhsOp, typename RhsOp>
constexpr auto opCompare(LhsOp, RhsOp) {
  constexpr static MinimalArray kOpOrder{
      // Special Constants
      kMeta<EOp>,
      kMeta<PiOp>,
      // Arithmetic
      kMeta<AddOp>,
      kMeta<SubOp>,
      kMeta<MulOp>,
      kMeta<DivOp>,
      // Power and Roots
      kMeta<PowOp>,
      kMeta<Atan2Op>,
      kMeta<SqrtOp>,
      // Exponentials and Logarithms
      kMeta<ExpOp>,
      kMeta<LogOp>,
      // Trigonometric Functions
      kMeta<SinOp>,
      kMeta<CosOp>,
      kMeta<TanOp>,
      // Inverse Trigonometric Functions
      kMeta<AsinOp>,
      kMeta<AcosOp>,
      kMeta<AtanOp>,
      // Hyperbolic Functions
      kMeta<SinhOp>,
      kMeta<CoshOp>,
      kMeta<TanhOp>,
      // Comparison Operators
      kMeta<EqOp>,
      kMeta<NeqOp>,
      kMeta<LtOp>,
      kMeta<LeqOp>,
      kMeta<GtOp>,
      kMeta<GeqOp>,
      // Logical
      kMeta<AndOp>,
      kMeta<OrOp>,
      kMeta<NotOp>,
      // Bitwise
      kMeta<BitAndOp>,
      kMeta<BitOrOp>,
      kMeta<BitXorOp>,
      kMeta<BitShiftLeftOp>,
      kMeta<BitShiftRightOp>,
  };
  constexpr auto getIndex = [](TypeId id) -> SizeT {
    for (SizeT i = 0; i < kOpOrder.size(); ++i) {
      if (kOpOrder[i] == id) {
        return i;
      }
    }
    return kOpOrder.size();
  };
  constexpr SizeT lhs_id = getIndex(kMeta<LhsOp>);
  static_assert(lhs_id < kOpOrder.size());
  constexpr SizeT rhs_id = getIndex(kMeta<RhsOp>);
  static_assert(rhs_id < kOpOrder.size());

  if (lhs_id < rhs_id) {
    return PartialOrdering::kLess;
  }
  if (lhs_id > rhs_id) {
    return PartialOrdering::kGreater;
  }
  return PartialOrdering::kEqual;
}
static_assert(opCompare(AddOp{}, LogOp{}) == PartialOrdering::kLess);

// A total ordering of symbols
//
// Values are sorted by what is "most likely to be simplified"
// 1. Expressions
// 2. Symbols
// 3. Constants
//
// The goal is the LHS of an expr be used at the first comparison
// point when comparing two like expressions. So the base x in x^3 or y
// in y*3.
constexpr auto symbolicCompare(Symbolic auto lhs, Symbolic auto rhs)
    -> PartialOrdering {
  // --- Category Comparison ---
  constexpr bool lhs_is_expr = match(lhs, AnyExpr{});
  constexpr bool rhs_is_expr = match(rhs, AnyExpr{});
  constexpr bool lhs_is_constant = match(lhs, AnyConstant{});
  constexpr bool rhs_is_constant = match(rhs, AnyConstant{});
  constexpr bool lhs_is_symbol = match(lhs, AnySymbol{});
  constexpr bool rhs_is_symbol = match(rhs, AnySymbol{});
  constexpr bool lhs_is_never = isSame<decltype(lhs), Never>;
  constexpr bool rhs_is_never = isSame<decltype(rhs), Never>;

  // Edge Case: Everything is less than Never
  if constexpr (lhs_is_never && lhs_is_never) {
    return PartialOrdering::kEqual;
  } else if constexpr (lhs_is_never) {
    return PartialOrdering::kGreater;
  } else if constexpr (rhs_is_never) {
    return PartialOrdering::kLess;
  }

  // Comparison Normalization
  // a => a + 0
  else if constexpr (match(lhs, AnyExpr{} + AnyExpr{}) &&
                     !match(rhs, AnyExpr{} + AnyExpr{})) {
    return symbolicCompare(lhs, rhs + 0_c);
  } else if constexpr (!match(lhs, AnyExpr{} + AnyExpr{}) &&
                       match(rhs, AnyExpr{} + AnyExpr{})) {
    return symbolicCompare(lhs + 0_c, rhs);
  }
  // a => a * 1
  else if constexpr (match(lhs, AnyExpr{} * AnyExpr{}) &&
                     !match(rhs, AnyExpr{} * AnyExpr{})) {
    return symbolicCompare(lhs, rhs * 1_c);
  } else if constexpr (!match(lhs, AnyExpr{} * AnyExpr{}) &&
                       match(rhs, AnyExpr{} * AnyExpr{})) {
    return symbolicCompare(lhs * 1_c, rhs);
  }
  // a => a ^ 1
  else if constexpr (match(lhs, pow(AnyArg{}, AnyArg{})) &&
                     !match(rhs, pow(AnyArg{}, AnyArg{}))) {
    return symbolicCompare(lhs, pow(rhs, 1_c));
  } else if constexpr (!match(lhs, pow(AnyArg{}, AnyArg{})) &&
                       match(rhs, pow(AnyArg{}, AnyArg{}))) {
    return symbolicCompare(pow(lhs, 1_c), rhs);
  }

  // First Expressions
  else if constexpr (lhs_is_expr && !rhs_is_expr) {
    return PartialOrdering::kLess;
  } else if constexpr (!lhs_is_expr && rhs_is_expr) {
    return PartialOrdering::kGreater;
  }

  // Then Symbols
  else if constexpr (lhs_is_symbol && !rhs_is_symbol) {
    return PartialOrdering::kLess;
  } else if constexpr (!lhs_is_symbol && rhs_is_symbol) {
    return PartialOrdering::kGreater;
  }

  // -- Within Category Comparison --
  //
  // Expressions: If operators are different, then compare them.
  else if constexpr (lhs_is_expr && rhs_is_expr) {
    constexpr auto get_arg_count =
        []<typename Op, typename... Args>(Expression<Op, Args...>) {
          return sizeof...(Args);
        };
    constexpr SizeT lhs_arg_count = get_arg_count(lhs);
    constexpr SizeT rhs_arg_count = get_arg_count(rhs);

    constexpr auto op_compare = opCompare(getOp(lhs), getOp(rhs));
    if constexpr (op_compare != PartialOrdering::kEqual) {
      return op_compare;
    } else if constexpr (lhs_arg_count < rhs_arg_count) {
      return PartialOrdering::kLess;
    } else if constexpr (lhs_arg_count > rhs_arg_count) {
      return PartialOrdering::kGreater;
    } else {
      constexpr auto compare_args_recursively =
          []<typename Op, typename... LhsArgs, typename... RhsArgs>(
              Expression<Op, LhsArgs...>, Expression<Op, RhsArgs...>) {
            PartialOrdering result = PartialOrdering::kEqual;
            [[maybe_unused]] auto compare_args =
                (((result = symbolicCompare(LhsArgs{}, RhsArgs{})),
                  result != PartialOrdering::kEqual) ||
                 ...);
            return result;
          };
      return compare_args_recursively(lhs, rhs);
    }
  }

  // Constants: Compare values
  else if constexpr (lhs_is_constant && rhs_is_constant) {
    if (lhs.value < rhs.value) {
      return PartialOrdering::kLess;
    }
    if (lhs.value > rhs.value) {
      return PartialOrdering::kGreater;
    }
    return PartialOrdering::kEqual;
  }

  else if constexpr (lhs_is_symbol && rhs_is_symbol) {
    if (kMeta<decltype(lhs)> < kMeta<decltype(rhs)>) {
      return PartialOrdering::kLess;
    }
    if (kMeta<decltype(lhs)> > kMeta<decltype(rhs)>) {
      return PartialOrdering::kGreater;
    }
    return PartialOrdering::kEqual;
  }

  else {
    static_assert(false,
                  "Invalid comparison between Symbolic types. Please check the "
                  "implementation.");
  }
}

constexpr auto symbolicLessThan(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return symbolicCompare(lhs, rhs) == PartialOrdering::kLess;
}

}  // namespace tempura
