// Tests for just — the immediate-value sender.

#include "task/task.h"

#include <string>
#include <tuple>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "just - single value"_test = [] {
    auto r = syncWait(just(42));
    if (!expectTrue(r.has_value())) return;
    expectEq(std::get<0>(*r), 42);
  };

  "just - multiple heterogeneous values"_test = [] {
    auto r = syncWait(just(1, 2.5, std::string{"hi"}));
    if (!expectTrue(r.has_value())) return;
    auto [a, b, c] = *r;
    expectEq(a, 1);
    expectEq(b, 2.5);
    expectEq(c, std::string{"hi"});
  };

  "just - accepts lvalue arguments and decay-copies them"_test = [] {
    // Regression: JustSender's ctor took `Args&&` — a plain rvalue reference
    // once Args is the fixed class parameter — so just(lvalue) was ill-formed.
    int x = 7;
    std::string s = "keep";
    auto r = syncWait(just(x, s));
    if (!expectTrue(r.has_value())) return;
    auto [a, b] = *r;
    expectEq(a, 7);
    expectEq(b, std::string{"keep"});
    expectEq(x, 7);                   // lvalues are COPIED, not moved-from
    expectEq(s, std::string{"keep"});
  };

  "just - empty completion carries no values"_test = [] {
    auto r = syncWait(just());
    expectTrue(r.has_value());
  };

  return TestRegistry::result();
}
