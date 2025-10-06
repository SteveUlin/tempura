#pragma once

#include <type_traits>

#include "symbolic3/context.h"
#include "symbolic3/core.h"

// Strategy infrastructure using concepts
// Simple and clean - no CRTP, no deducing this

namespace tempura::symbolic3 {

// ============================================================================
// Strategy Concept
// ============================================================================

// A strategy is any type with an apply method that takes an expression and
// context
template <typename S>
concept Strategy =
    requires(S const& strat, Symbol<> sym, TransformContext<> ctx) {
      { strat.apply(sym, ctx) } -> Symbolic;
    };

// ============================================================================
// Basic Strategies
// ============================================================================

// Identity: returns expression unchanged
struct Identity {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context) const {
    return expr;
  }
};

// Fail: always returns Never (used in choice combinators)
struct Fail {
  template <Symbolic S, typename Context>
  constexpr auto apply(S, Context) const {
    return Never{};
  }
};

// ============================================================================
// Composition Combinators
// ============================================================================

// Sequence: apply first strategy, then second
template <typename S1, typename S2>
struct Sequence {
  S1 first;
  S2 second;

  template <Symbolic S, typename Context>
  constexpr auto apply(this auto const& self, S expr, Context ctx) {
    auto intermediate = self.first.apply(expr, ctx);
    if constexpr (std::is_same_v<decltype(intermediate), Never>) {
      return Never{};
    } else {
      return self.second.apply(intermediate, ctx);
    }
  }
};

// Operator overload for convenient syntax: s1 >> s2
template <Strategy S1, Strategy S2>
constexpr auto operator>>(S1 const& s1, S2 const& s2) {
  return Sequence{s1, s2};
}

// Choice: try first strategy, if fails/no-change, try second
template <Strategy S1, Strategy S2>
struct Choice {
  S1 first;
  S2 second;

  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    if constexpr (requires { first.apply(expr, ctx); }) {
      auto result = first.apply(expr, ctx);

      // If first failed, try second
      if constexpr (std::is_same_v<decltype(result), Never>) {
        return second.apply(expr, ctx);
      }
      // Check if first made no change - try second
      else if constexpr (std::is_same_v<decltype(result), S>) {
        return second.apply(expr, ctx);
      }
      // First succeeded and made a change - return it
      else {
        return result;
      }
    } else {
      return second.apply(expr, ctx);
    }
  }
};

// Operator overload for convenient syntax: s1 | s2
template <Strategy S1, Strategy S2>
constexpr auto operator|(S1 const& s1, S2 const& s2) {
  return Choice<S1, S2>{s1, s2};
}

// Try: apply strategy, if fails return original expression
template <Strategy S>
struct Try {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    auto result = strategy.apply(expr, ctx);
    if constexpr (std::is_same_v<decltype(result), Never>) {
      return expr;
    } else {
      return result;
    }
  }
};

template <Strategy S>
constexpr auto try_strategy(S const& strat) {
  return Try<S>{strat};
}

// ============================================================================
// Conditional Combinators
// ============================================================================

// When: apply strategy only if predicate holds
template <typename Pred, Strategy S>
struct When {
  Pred predicate;
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    if (predicate(expr, ctx)) {
      return strategy.apply(expr, ctx);
    }
    return expr;
  }
};

template <typename Pred, Strategy S>
constexpr auto when(Pred pred, S const& strat) {
  return When<Pred, S>{pred, strat};
}

// ============================================================================
// Recursion Combinators
// ============================================================================

// FixPoint: apply strategy repeatedly until no change or depth limit
template <Strategy S, std::size_t MaxDepth = 20>
struct FixPoint {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    if constexpr (ctx.depth >= MaxDepth) {
      return expr;  // Depth limit reached
    } else {
      auto result = strategy.apply(expr, ctx);

      // Check if we've reached a fixed point
      constexpr bool unchanged = std::is_same_v<decltype(result), Expr>;
      constexpr bool failed = std::is_same_v<decltype(result), Never>;

      if constexpr (unchanged || failed) {
        return expr;
      } else {
        // Continue iteration with incremented depth
        auto new_ctx = ctx.template increment_depth<1>();
        return apply(result, new_ctx);
      }
    }
  }
};

// Repeat: apply strategy exactly N times
template <Strategy S, std::size_t N>
struct Repeat {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    if constexpr (N == 0) {
      return expr;
    } else {
      auto result = strategy.apply(expr, ctx);
      if constexpr (std::is_same_v<decltype(result), Never>) {
        return Never{};
      } else if constexpr (N == 1) {
        return result;
      } else {
        return Repeat<S, N - 1>{strategy}.apply(result, ctx);
      }
    }
  }
};

}  // namespace tempura::symbolic3

// ============================================================================
// Design Notes
// ============================================================================

/*

KEY IMPROVEMENTS WITH CONCEPT-BASED DESIGN:

1. **No CRTP Boilerplate**

   Before (CRTP):
   ```cpp
   template <typename Impl>
   struct Strategy {
     template <Symbolic S, typename Context>
     constexpr auto apply(S expr, Context ctx) const {
       return static_cast<const Impl*>(this)->apply_impl(expr, ctx);
     }
   };

   struct MyStrategy : Strategy<MyStrategy> {
     template <Symbolic S, typename Context>
     constexpr auto apply_impl(S expr, Context ctx) const {
       // implementation
     }
   };
   ```

   After (concepts):
   ```cpp
   struct MyStrategy {
     template <Symbolic S, typename Context>
     constexpr auto apply(S expr, Context ctx) const {
       // implementation - simple and direct
     }
   };
   ```

2. **Simple Member Access**
   - Direct member access: `strategy.apply(expr, ctx)`
   - No casting needed
   - No special syntax
   - Clear and readable

3. **Concept-Based Checking**
   - Strategy is a concept, not a base class
   - Any type with `apply(Symbolic, Context)` satisfies it
   - No inheritance required
   - Duck typing with compile-time verification

4. **Clear Composition**
   ```cpp
   // Combinators just call member strategies directly
   auto intermediate = first.apply(expr, ctx);
   return second.apply(intermediate, ctx);
   ```

5. **Better Composition**
   - Composition types (Sequence, Choice) are simple structs
   - No inheritance hierarchy
   - Flat structure
   - Easy to understand and debug

USAGE EXAMPLE:

```cpp
// Simple strategy - just implement apply()
struct FoldConstants {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    if (!ctx.mode.fold_numeric_constants) {
      return expr;
    }
    // ... folding logic
  }
};

// Compose strategies using operators
constexpr auto pipeline = FoldConstants{} >> ApplyAlgebraicRules{};

// Use it - natural syntax
constexpr auto result = pipeline.apply(expr, ctx);
```

BENEFITS:

1. **Simplicity**: No boilerplate, no CRTP, no inheritance
2. **Clarity**: Direct member access, standard syntax
3. **Error messages**: Clean template instantiations
4. **Flexibility**: Concept-based, not inheritance-based
5. **Modern**: Uses C++20 concepts properly

WHY NOT DEDUCING THIS?

While deducing this (C++23) is useful for mixin patterns where you need
explicit access to the derived type, we don't need it here because:
- Concepts already provide the polymorphism we need
- We don't need to access the derived type explicitly
- Simpler syntax is better when it works
- The Strategy concept constrains everything we need

*/
