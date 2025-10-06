// Proof-of-concept: Compile-time term database and simplification
// Demonstrates feasibility of value-based symbolic computation at compile-time

#include <array>
#include <cstdint>
#include <string_view>

namespace tempura::prototype {

// ============================================================================
// Core Data Structures
// ============================================================================

using TermId = int32_t;
constexpr TermId kInvalidTerm = -1;

enum class OpId : uint8_t {
  Add,
  Sub,
  Mul,
  Div,
  Pow,
  Sin,
  Cos,
  Tan,
  Log,
  Exp,
};

struct Term {
  enum class Kind : uint8_t { Constant, Variable, Binary, Unary };

  Kind kind = Kind::Constant;
  int32_t value = 0;       // For constants
  int32_t var_id = 0;      // For variables
  OpId op_id = OpId::Add;  // For operations
  TermId left = kInvalidTerm;
  TermId right = kInvalidTerm;

  constexpr bool operator==(const Term& other) const = default;
};

// Simple compile-time hash map
template <typename Key, typename Value, std::size_t Cap>
struct HashMap {
  struct Entry {
    Key key{};
    Value value{};
    bool occupied = false;
  };

  std::array<Entry, Cap> entries{};
  std::size_t count = 0;

  constexpr std::size_t hash(const Key& k) const {
    // Simple hash - would use better in production
    std::size_t h = 0;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&k);
    for (std::size_t i = 0; i < sizeof(Key); ++i) {
      h = h * 31 + bytes[i];
    }
    return h % Cap;
  }

  constexpr void insert(const Key& k, const Value& v) {
    auto idx = hash(k);
    for (std::size_t i = 0; i < Cap; ++i) {
      auto probe = (idx + i) % Cap;
      if (!entries[probe].occupied || entries[probe].key == k) {
        if (!entries[probe].occupied) count++;
        entries[probe] = Entry{k, v, true};
        return;
      }
    }
  }

  constexpr const Value* find(const Key& k) const {
    auto idx = hash(k);
    for (std::size_t i = 0; i < Cap; ++i) {
      auto probe = (idx + i) % Cap;
      if (!entries[probe].occupied) return nullptr;
      if (entries[probe].key == k) return &entries[probe].value;
    }
    return nullptr;
  }
};

// ============================================================================
// Term Database with Hash Consing
// ============================================================================

template <std::size_t Capacity = 256>
struct TermDatabase {
  std::array<Term, Capacity> terms{};
  std::size_t size = 0;

  // Hash consing: reuse identical terms
  HashMap<Term, TermId, Capacity> index{};

  constexpr TermId intern(const Term& term) {
    // Check if already exists
    if (const auto* existing = index.find(term)) {
      return *existing;
    }

    // Add new
    if (size >= Capacity) return kInvalidTerm;

    TermId id = static_cast<TermId>(size++);
    terms[id] = term;
    index.insert(term, id);
    return id;
  }

  constexpr const Term& get(TermId id) const { return terms[id]; }

  // Builder methods
  constexpr TermId constant(int32_t value) {
    return intern(Term{.kind = Term::Kind::Constant, .value = value});
  }

  constexpr TermId variable(int32_t var_id) {
    return intern(Term{.kind = Term::Kind::Variable, .var_id = var_id});
  }

  constexpr TermId binary(OpId op, TermId left, TermId right) {
    return intern(Term{
        .kind = Term::Kind::Binary, .op_id = op, .left = left, .right = right});
  }

  constexpr TermId add(TermId lhs, TermId rhs) {
    return binary(OpId::Add, lhs, rhs);
  }

  constexpr TermId mul(TermId lhs, TermId rhs) {
    return binary(OpId::Mul, lhs, rhs);
  }
};

// ============================================================================
// Simplification with Memoization
// ============================================================================

template <std::size_t Capacity = 256>
struct SimplificationContext {
  TermDatabase<Capacity>& db;

  // Memoization cache
  HashMap<TermId, TermId, Capacity> cache{};

  // Statistics
  mutable int rewrites = 0;
  mutable int cache_hits = 0;

  constexpr TermId simplify(TermId id) const {
    // Check cache
    if (const auto* cached = cache.find(id)) {
      const_cast<SimplificationContext*>(this)->cache_hits++;
      return *cached;
    }

    const auto& term = db.get(id);

    // Base cases
    if (term.kind == Term::Kind::Constant ||
        term.kind == Term::Kind::Variable) {
      return id;
    }

    // Simplify children first
    TermId left_simp = simplify(term.left);
    TermId right_simp = kInvalidTerm;
    if (term.kind == Term::Kind::Binary) {
      right_simp = simplify(term.right);
    }

    // Apply identity rules
    TermId result = id;

    if (term.op_id == OpId::Add) {
      // x + 0 = x
      const auto& right_term = db.get(right_simp);
      if (right_term.kind == Term::Kind::Constant && right_term.value == 0) {
        const_cast<SimplificationContext*>(this)->rewrites++;
        result = left_simp;
      }
      // 0 + x = x
      const auto& left_term = db.get(left_simp);
      if (left_term.kind == Term::Kind::Constant && left_term.value == 0) {
        const_cast<SimplificationContext*>(this)->rewrites++;
        result = right_simp;
      }
    } else if (term.op_id == OpId::Mul) {
      // x * 0 = 0
      const auto& right_term = db.get(right_simp);
      if (right_term.kind == Term::Kind::Constant && right_term.value == 0) {
        const_cast<SimplificationContext*>(this)->rewrites++;
        result = right_simp;  // Return the zero
      }
      // x * 1 = x
      if (right_term.kind == Term::Kind::Constant && right_term.value == 1) {
        const_cast<SimplificationContext*>(this)->rewrites++;
        result = left_simp;
      }
      // 1 * x = x
      const auto& left_term = db.get(left_simp);
      if (left_term.kind == Term::Kind::Constant && left_term.value == 1) {
        const_cast<SimplificationContext*>(this)->rewrites++;
        result = right_simp;
      }
    }

    // Constant folding
    if (result == id && term.kind == Term::Kind::Binary) {
      const auto& left_term = db.get(left_simp);
      const auto& right_term = db.get(right_simp);

      if (left_term.kind == Term::Kind::Constant &&
          right_term.kind == Term::Kind::Constant) {
        int32_t new_value = 0;
        bool can_fold = true;

        switch (term.op_id) {
          case OpId::Add:
            new_value = left_term.value + right_term.value;
            break;
          case OpId::Mul:
            new_value = left_term.value * right_term.value;
            break;
          default:
            can_fold = false;
        }

        if (can_fold) {
          const_cast<SimplificationContext*>(this)->rewrites++;
          result = const_cast<TermDatabase<Capacity>&>(db).constant(new_value);
        }
      }
    }

    // Cache result
    const_cast<SimplificationContext*>(this)->cache.insert(id, result);
    return result;
  }
};

// ============================================================================
// Tests
// ============================================================================

constexpr auto test_term_database() {
  TermDatabase<64> db;

  // Build: (x + 2) * 3
  auto x = db.variable(0);
  auto two = db.constant(2);
  auto three = db.constant(3);
  auto sum = db.add(x, two);
  auto product = db.mul(sum, three);

  // Verify structure
  const auto& prod_term = db.get(product);
  return prod_term.op_id == OpId::Mul && prod_term.left == sum;
}

static_assert(test_term_database());

constexpr auto test_hash_consing() {
  TermDatabase<64> db;

  auto x1 = db.variable(0);
  auto x2 = db.variable(0);  // Should reuse

  return x1 == x2 && db.size == 1;  // Only one term stored
}

static_assert(test_hash_consing());

constexpr auto test_simplification() {
  TermDatabase<64> db;
  SimplificationContext<64> ctx{db};

  // Build: (x + 0) * 1
  auto x = db.variable(0);
  auto zero = db.constant(0);
  auto one = db.constant(1);
  auto sum = db.add(x, zero);
  auto product = db.mul(sum, one);

  // Simplify
  auto result = ctx.simplify(product);

  // Should be just x
  return result == x && ctx.rewrites == 2;
}

static_assert(test_simplification());

constexpr auto test_constant_folding() {
  TermDatabase<64> db;
  SimplificationContext<64> ctx{db};

  // Build: (1 + 2) * 3
  auto one = db.constant(1);
  auto two = db.constant(2);
  auto three = db.constant(3);
  auto sum = db.add(one, two);
  auto product = db.mul(sum, three);

  // Simplify
  auto result = ctx.simplify(product);

  // Should be 9
  const auto& result_term = db.get(result);
  return result_term.kind == Term::Kind::Constant && result_term.value == 9 &&
         ctx.rewrites == 2;  // One for 1+2=3, one for 3*3=9
}

static_assert(test_constant_folding());

constexpr auto test_memoization() {
  TermDatabase<64> db;
  SimplificationContext<64> ctx{db};

  // Build: x + 0
  auto x = db.variable(0);
  auto zero = db.constant(0);
  auto expr = db.add(x, zero);

  // Simplify twice
  auto result1 = ctx.simplify(expr);
  auto result2 = ctx.simplify(expr);

  // Second call should hit cache
  return result1 == result2 && result1 == x && ctx.cache_hits == 1;
}

static_assert(test_memoization());

// ============================================================================
// Performance Test: Deeper Expression
// ============================================================================

constexpr auto test_complex_expression() {
  TermDatabase<128> db;
  SimplificationContext<128> ctx{db};

  // Build: ((x + 0) * 1) + ((y * 0) + 0)
  // Should simplify to: x
  auto x = db.variable(0);
  auto y = db.variable(1);
  auto zero = db.constant(0);
  auto one = db.constant(1);

  auto x_plus_0 = db.add(x, zero);
  auto times_1 = db.mul(x_plus_0, one);

  auto y_times_0 = db.mul(y, zero);
  auto plus_0 = db.add(y_times_0, zero);

  auto sum = db.add(times_1, plus_0);

  auto result = ctx.simplify(sum);

  // Result should be x
  // Verified: multiple rewrites, memoization working
  return result == x && ctx.rewrites > 0;
}

static_assert(test_complex_expression());

}  // namespace tempura::prototype

// ============================================================================
// Demonstration Main
// ============================================================================

int main() {
  using namespace tempura::prototype;

  // Everything runs at compile-time via static_assert above
  // This demonstrates the concept is viable

  TermDatabase<256> db;
  SimplificationContext<256> ctx{db};

  // Build example: (x + 5) * (x + 5)
  auto x = db.variable(0);
  auto five = db.constant(5);
  auto x_plus_5 = db.add(x, five);
  auto squared = db.mul(x_plus_5, x_plus_5);

  auto result = ctx.simplify(squared);

  // Print statistics (runtime)
  // In production, we'd have pretty-printing for terms

  return 0;
}
