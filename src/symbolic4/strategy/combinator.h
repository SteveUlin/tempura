#pragma once

#include "meta/utility.h"
#include "symbolic4/core.h"
#include "symbolic4/strategy/pattern.h"

// ============================================================================
// STRATEGY COMBINATORS
// ============================================================================
//
// This implements Stratego-style rewriting strategies for compile-time term
// transformation. The approach separates *what* to rewrite (rules) from *where*
// and *when* to apply rewrites (strategies).
//
// THEORETICAL BACKGROUND
// ----------------------
// Traditional term rewriting applies rules exhaustively until a normal form.
// This works when the rule system is confluent (order doesn't matter) and
// terminating. But real transformations often need control:
//
//   - Apply rules in phases (parse → desugar → optimize → codegen)
//   - Avoid non-termination in non-confluent systems
//   - Choose specific normal forms when multiple exist
//
// Strategy combinators solve this by making traversal programmable. Rather
// than a fixed "apply everywhere until done" semantics, you compose traversal
// from primitives. See Visser's "Stratego: A Language for Program
// Transformation Based on Rewriting Strategies" (RTA 2001).
//
// OUR DESIGN
// ----------
// This is a compile-time (constexpr) implementation where:
//   - Strategies are types with an `apply(E)` method
//   - Success returns the transformed expression
//   - Failure returns `Never{}` (a sentinel type)
//   - Type changes encode transformation (E → E' means progress)
//
// Using types for failure rather than std::optional lets us:
//   - Track progress at compile time (E same as result = no change)
//   - Avoid runtime overhead
//   - Get precise return types
//
// PRIMITIVES
// ----------
//   Id       - Identity: always succeeds, returns input unchanged
//   Fail     - Always fails (returns Never)
//   Seq      - Sequential: s1 >> s2 applies s1 then s2 to result
//   Choice   - Deterministic choice: s1 | s2 tries s1, falls back to s2
//   Try      - Attempt: try_(s) applies s, returns unchanged on failure
//   All      - Congruence: apply s to all children (fail if any child fails)
//
// DERIVED TRAVERSALS
// ------------------
//   BottomUp   - Post-order: process children, then node
//   TopDown    - Pre-order: process node, then children
//   Innermost  - Bottom-up fixpoint: repeat until no more changes
//   Outermost  - Top-down fixpoint: repeat until no more changes
//
// MISSING FROM STANDARD STRATEGO (potential extensions)
// -----------------------------------------------------
//   one(s)    - Apply s to exactly one child (first that succeeds)
//   some(s)   - Apply s to at least one child (all that succeed)
//   rec x(s)  - Named recursive strategy reference
//   s1 < s2 + s3 - Guarded choice (if s1 then s2 else s3)
//
// The `one` and `some` combinators enable different traversal patterns:
//   - all(s): transform ALL children or fail (strict)
//   - one(s): transform exactly ONE child (useful for searching)
//   - some(s): transform children that match (permissive)
//
// ALTERNATIVE PARADIGMS
// ---------------------
// E-graphs and equality saturation (see egg/egglog) take a different approach:
// rather than committing to one rewrite path, they explore all rewrites
// simultaneously and extract an optimal term at the end. This avoids phase
// ordering problems but requires more complex infrastructure.
//
// For our use case (symbolic differentiation, simplification), traditional
// strategies suffice and integrate naturally with compile-time evaluation.
//
// RECURSION AND TERMINATION
// -------------------------
// Innermost/Outermost use a MaxDepth parameter to bound recursion. This is
// necessary because:
//   1. C++ template instantiation depth is bounded
//   2. Non-terminating rewrites must fail gracefully
//   3. Compile-time evaluation needs finite bounds
//
// If rewriting doesn't reach a fixpoint within MaxDepth iterations, we
// return Never{} (failure). Callers should increase MaxDepth or restructure
// rules if this occurs legitimately.
//
// REFERENCES
// ----------
// - Visser, "Stratego: A Language for Program Transformation" (RTA 2001)
// - Sloane, "Lightweight Language Processing in Kiama" (GTTSE 2009)
// - Willsey et al., "egg: Fast and Extensible Equality Saturation" (POPL 2021)
// - Spoofax documentation: https://spoofax.dev/references/stratego/
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// PRIMITIVES
// ============================================================================

// Id: always succeeds, returns input unchanged.
// The identity element for Seq (s >> Id = Id >> s = s).
struct Id {
  template <Symbolic E>
  constexpr auto apply(E e) const {
    return e;
  }
};

// Fail: always fails (returns Never).
// The identity element for Choice (s | Fail = Fail | s = s).
// Useful for explicit failure or as a base case in recursive strategies.
struct Fail {
  template <Symbolic E>
  constexpr auto apply(E) const {
    return Never{};
  }
};

// Seq (>>): sequential composition.
// Applies s1, then applies s2 to the result. Fails if either fails.
// This is left-to-right function composition: (s1 >> s2)(e) = s2(s1(e))
template <typename S1, typename S2>
struct Seq {
  [[no_unique_address]] S1 s1;
  [[no_unique_address]] S2 s2;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    auto r = s1.apply(e);
    if constexpr (isSame<decltype(r), Never>) {
      return Never{};
    } else {
      return s2.apply(r);
    }
  }
};

// Choice (|): deterministic left-biased choice with local backtracking.
// Tries s1 first. If s1 fails OR returns the input unchanged, tries s2.
// This is Stratego's <+ operator, not non-deterministic choice (+).
//
// The "unchanged = try alternative" semantics means:
//   rule1 | rule2 | rule3
// keeps trying until something makes progress or all fail.
template <typename S1, typename S2>
struct Choice {
  [[no_unique_address]] S1 s1;
  [[no_unique_address]] S2 s2;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    auto r = s1.apply(e);
    if constexpr (isSame<decltype(r), Never> || isSame<decltype(r), E>) {
      return s2.apply(e);
    } else {
      return r;
    }
  }
};

// Try: attempts strategy s, recovers from failure by returning input unchanged.
// Equivalent to: s | Id
// Called "attempt" in Kiama (since "try" is a C++ keyword).
template <typename S>
struct Try {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    auto r = s.apply(e);
    if constexpr (isSame<decltype(r), Never>) {
      return e;
    } else {
      return r;
    }
  }
};

// All: applies strategy s to ALL direct children. Fails if any child fails.
// For non-expressions (atoms, constants), succeeds immediately (no children).
//
// This is the workhorse for tree traversal. Combined with Seq/Choice:
//   bottomup(s) = all(bottomup(s)) >> s    (children first, then this node)
//   topdown(s)  = s >> all(topdown(s))     (this node first, then children)
//
// Related combinators not implemented here:
//   one(s)  - apply s to exactly one child (first success), fail if none
//   some(s) - apply s to all children that succeed, fail if none succeed
//
// The one/some variants enable different search strategies:
//   - all: strict transformation (all children must transform)
//   - one: search (find first transformable child)
//   - some: permissive transformation (transform what you can)
template <typename S>
struct All {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    if constexpr (!is_expression_v<E>) {
      return e;  // Atoms/constants have no children
    } else {
      return applyToChildren(e);
    }
  }

 private:
  // Nullary expression (no arguments)
  template <typename Op>
  constexpr auto applyToChildren(Expression<Op> e) const {
    return e;
  }

  // Unary expression
  template <typename Op, Symbolic A0>
  constexpr auto applyToChildren(Expression<Op, A0> e) const {
    auto r0 = s.apply(e.template arg<0>());
    if constexpr (isSame<decltype(r0), Never>) {
      return Never{};
    } else {
      return Expression<Op, decltype(r0)>{r0};
    }
  }

  // Binary expression
  template <typename Op, Symbolic A0, Symbolic A1>
  constexpr auto applyToChildren(Expression<Op, A0, A1> e) const {
    auto r0 = s.apply(e.template arg<0>());
    if constexpr (isSame<decltype(r0), Never>) {
      return Never{};
    } else {
      auto r1 = s.apply(e.template arg<1>());
      if constexpr (isSame<decltype(r1), Never>) {
        return Never{};
      } else {
        return Expression<Op, decltype(r0), decltype(r1)>{r0, r1};
      }
    }
  }

  // Ternary expression
  template <typename Op, Symbolic A0, Symbolic A1, Symbolic A2>
  constexpr auto applyToChildren(Expression<Op, A0, A1, A2> e) const {
    auto r0 = s.apply(e.template arg<0>());
    if constexpr (isSame<decltype(r0), Never>) {
      return Never{};
    } else {
      auto r1 = s.apply(e.template arg<1>());
      if constexpr (isSame<decltype(r1), Never>) {
        return Never{};
      } else {
        auto r2 = s.apply(e.template arg<2>());
        if constexpr (isSame<decltype(r2), Never>) {
          return Never{};
        } else {
          return Expression<Op, decltype(r0), decltype(r1), decltype(r2)>{
              r0, r1, r2};
        }
      }
    }
  }
};

// ============================================================================
// DERIVED TRAVERSALS
// ============================================================================
//
// These combine the primitives to achieve full-tree traversal patterns.
// The key insight is that `all` recurses through children while the outer
// strategy controls when to apply the rule.
//
// SINGLE-PASS TRAVERSALS (visit each node once):
//   bottomup(s) - post-order: children before parent
//   topdown(s)  - pre-order: parent before children
//
// FIXPOINT TRAVERSALS (repeat until no changes):
//   innermost(s) - bottom-up fixpoint, preferred for normalization
//   outermost(s) - top-down fixpoint, useful for expansion
//
// WHY INNERMOST FOR NORMALIZATION?
// Innermost rewrites innermost redexes first, which means:
//   1. Arguments are simplified before the parent rule fires
//   2. Rule patterns can assume normalized arguments
//   3. Each rule application sees the "most reduced" children
//
// Example: simplify(0 + (1 + x))
//   Innermost: first simplify (1 + x) → stuck, then 0 + (1+x) → (1+x)
//   Outermost: try 0 + (1+x) → (1+x), done (one step)
//
// For simplification rules that expect normalized arguments (like 0+x → x),
// innermost ensures arguments are already in normal form.
//
// ============================================================================

// BottomUp: single-pass post-order traversal.
// Processes all children first, then applies s to the parent.
// Mathematical definition: bottomup(s) = all(bottomup(s)) >> s
template <typename S>
struct BottomUp {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    return s.apply(recurse(e));
  }

 private:
  template <Symbolic E>
  constexpr auto recurse(E e) const {
    return All<BottomUp>{*this}.apply(e);
  }
};

// TopDown: single-pass pre-order traversal.
// Applies s to the node first, then processes children.
// Mathematical definition: topdown(s) = s >> all(topdown(s))
template <typename S>
struct TopDown {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    return recurse(s.apply(e));
  }

 private:
  template <Symbolic E>
  constexpr auto recurse(E e) const {
    if constexpr (isSame<E, Never>) {
      return Never{};
    } else {
      return All<TopDown>{*this}.apply(e);
    }
  }
};

// Innermost: bottom-up fixpoint iteration.
// Repeatedly applies s bottom-up until reaching a normal form (no more changes).
// Mathematical definition: innermost(s) = bottomup(try(s >> innermost(s)))
//
// This is the standard strategy for term normalization. It ensures:
//   - Rules see fully-normalized arguments
//   - Changes propagate upward correctly
//   - Termination when rule system is terminating
//
// MaxDepth bounds template recursion. If exceeded, returns Never{} (failure).
// Increase MaxDepth for complex expressions, or restructure rules if
// non-termination is suspected.
template <typename S, SizeT MaxDepth = 20>
struct Innermost {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    return loop<E, 0>(e);
  }

 private:
  template <Symbolic E, SizeT Depth>
  constexpr auto loop(E e) const {
    if constexpr (Depth >= MaxDepth) {
      return Never{};  // Exceeded depth: likely non-terminating rules
    } else {
      // First, recursively normalize all children
      auto children_done = All<Innermost>{*this}.apply(e);
      if constexpr (isSame<decltype(children_done), Never>) {
        return e;  // Child normalization failed, return input
      } else {
        // Then try to apply s at this node
        auto here = s.apply(children_done);
        if constexpr (isSame<decltype(here), Never> ||
                      isSame<decltype(here), decltype(children_done)>) {
          return children_done;  // s didn't apply, we're done here
        } else {
          // s succeeded and changed something: recurse on the result
          return loop<decltype(here), Depth + 1>(here);
        }
      }
    }
  }
};

// Outermost: top-down fixpoint iteration.
// Repeatedly applies s top-down until reaching a normal form.
// Mathematical definition: outermost(s) = topdown(try(s >> outermost(s)))
//
// Useful when:
//   - Rules expand/unfold definitions
//   - You want to process outer structure before diving into details
//   - Rules don't require normalized arguments
//
// Generally less common than innermost for simplification tasks.
template <typename S, SizeT MaxDepth = 20>
struct Outermost {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    return loop<E, 0>(e);
  }

 private:
  template <Symbolic E, SizeT Depth>
  constexpr auto loop(E e) const {
    if constexpr (Depth >= MaxDepth) {
      return Never{};  // Exceeded depth: likely non-terminating rules
    } else {
      // First try to apply s at this node
      auto here = s.apply(e);
      if constexpr (isSame<decltype(here), Never> || isSame<decltype(here), E>) {
        // s didn't apply here, descend into children
        return All<Outermost>{*this}.apply(e);
      } else {
        // s succeeded: recurse on the transformed result
        return loop<decltype(here), Depth + 1>(here);
      }
    }
  }
};

// ============================================================================
// FIXED-POINT COMBINATOR
// ============================================================================
//
// Fix enables defining recursive strategies compositionally. Instead of
// creating a dedicated type for each recursive pattern (like BottomUp<S>),
// you can express the recursion inline:
//
//   auto my_bottomup = fix([s](auto rec) { return all(rec) >> s; });
//
// This says: "bottomup applies s after recursively processing children."
// The lambda receives 'rec' (the fixed point itself) and returns a strategy
// that uses 'rec' for recursive calls.
//
// HOW IT WORKS
// ------------
// The key insight is that Fix<F> is a finite type, even though it represents
// infinite recursion. The type F (the lambda) is fixed at compile time;
// recursion happens at the value level when apply() calls f(*this).
//
// Without Fix, writing `bottomup(s) = all(bottomup(s)) >> s` directly creates
// an infinite type: Seq<All<Seq<All<Seq<...>>>>, S>. With Fix, the type is
// just Fix<Lambda>, and the unfolding happens during execution.
//
// EXAMPLES
// --------
// The classic traversals expressed with fix:
//
//   bottomup(s) = fix([s](auto rec) { return all(rec) >> s; })
//   topdown(s)  = fix([s](auto rec) { return s >> all(rec); })
//
// More exotic patterns become easy to express:
//
//   // Apply s somewhere top-down (first success wins)
//   oncetd(s) = fix([s](auto rec) { return s | one(rec); })
//
//   // Apply s down the leftmost spine only
//   spinetd(s) = fix([s](auto rec) { return s >> try_(one(rec)); })
//
// COMPARISON TO DEDICATED TYPES
// -----------------------------
// Fix is more flexible but creates a new strategy object on each recursive
// call. The dedicated types (BottomUp, TopDown) avoid this by using *this
// directly. For compile-time evaluation this rarely matters, but for very
// deep trees you might prefer the dedicated versions.
//
// Fix is valuable for:
//   - Experimenting with new traversal patterns
//   - One-off recursive strategies
//   - Understanding the mathematical structure of recursion schemes
//
// ============================================================================

template <typename F>
struct Fix {
  [[no_unique_address]] F f;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    // f(*this) produces a strategy that uses *this for recursive calls
    return f(*this).apply(e);
  }
};

template <typename F>
constexpr auto fix(F f) {
  return Fix<F>{f};
}

// ============================================================================
// OPERATORS
// ============================================================================

template <typename S1, typename S2>
constexpr auto operator>>(S1 s1, S2 s2) {
  return Seq<S1, S2>{s1, s2};
}

template <typename S1, typename S2>
constexpr auto operator|(S1 s1, S2 s2) {
  return Choice<S1, S2>{s1, s2};
}

// ============================================================================
// FACTORIES
// ============================================================================

template <typename S>
constexpr auto try_(S s) {
  return Try<S>{s};
}

template <typename S>
constexpr auto all(S s) {
  return All<S>{s};
}

template <typename S>
constexpr auto bottomup(S s) {
  return BottomUp<S>{s};
}

template <typename S>
constexpr auto topdown(S s) {
  return TopDown<S>{s};
}

template <typename S>
constexpr auto innermost(S s) {
  return Innermost<S>{s};
}

template <typename S>
constexpr auto outermost(S s) {
  return Outermost<S>{s};
}

// ============================================================================
// POTENTIAL EXTENSIONS (not yet implemented)
// ============================================================================
//
// These combinators from Stratego/Kiama could be added if needed:
//
// ONE AND SOME TRAVERSALS
// -----------------------
// one(s): Apply s to exactly one child (the first that succeeds).
//         Fails if no child succeeds. Useful for searching.
//
// some(s): Apply s to all children that succeed (at least one must).
//          Returns a term where successful children are transformed.
//          Fails only if ALL children fail.
//
// Implementation sketch for one:
//
//   template <typename S>
//   struct One {
//     S s;
//     template <Symbolic E>
//     constexpr auto apply(E e) const {
//       // Try s on each child in order, return on first success
//       // Fail if no child succeeds
//     }
//   };
//
// REDUCE (Kiama)
// --------------
// reduce(s): Like repeat(some(s)), bottom-up fixpoint that:
//   1. Try s at current node
//   2. If fails, try some(reduce(s)) on children
//   3. Repeat until fixpoint
//
// More permissive than innermost - doesn't require ALL children to succeed.
//
// GUARDED CHOICE
// --------------
// s1 < s2 + s3: If s1 succeeds, apply s2 to result; else apply s3 to original.
// Unlike (s1 >> s2) | s3, the guard s1's result is discarded before s2.
//
// Useful for: "if this pattern matches, do X; otherwise do Y"
// without polluting the term with intermediate transformations.
//
// CONGRUENCE OPERATORS
// --------------------
// For each constructor C(a,b,c), a congruence C(s1,s2,s3) applies
// distinct strategies to each argument position.
//
// Example: Add(simplify, Id) applies simplify to left arg, Id to right.
//
// In our type-based system, this could be:
//   template <typename Op, typename... Strategies>
//   struct Congruence { ... };
//
// MEMOIZATION
// -----------
// For expensive strategies on large terms with sharing, memoization avoids
// recomputing results for structurally identical subterms.
//
// In our constexpr context, memoization is less critical since the compiler
// may already cache template instantiations. But for runtime strategies
// (if we ever support those), a Memo<S> combinator would be valuable.
//
// TRACING / DEBUGGING
// -------------------
// A Trace<S> combinator that logs (at compile-time via static_assert messages,
// or at runtime via print) each application of S. Useful for debugging
// non-terminating or unexpected rewrites.
//
// ============================================================================

}  // namespace tempura::symbolic4
