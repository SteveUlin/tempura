#include "src/symbolic.h"

namespace tempura::symbolic {

template <SymbolicOperator OP, typename... Args>
struct MatchExpression;

template <SymbolicOperator OP, Matcher... Args>
struct MatchExpression<OP, Args...> {};

template <SymbolicOperator OP>
struct MatchExpression<OP, AnyNTerms> {};

} // namespace tempura::symbolic
