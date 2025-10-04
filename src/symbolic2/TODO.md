# Symbolic2 TODO List

## ✅ Completed (Phase 1 & 2)

### Template Instantiation Depth Issues (FIXED)

- ✅ Removed recursive `simplifySymbol` calls from identity functions
- ✅ Implemented depth-limiting mechanism in `simplifySymbolWithDepth`
- ✅ Fixed all identity functions to return transformed expressions without recursion
- ✅ All simplification tests passing (10/10)

### Test Suite (COMPLETE)

- ✅ All logarithm identity tests passing
- ✅ All exponential identity tests passing
- ✅ All trigonometric identity tests passing
- ✅ Complex expression test passing
- ✅ Basic symbolic tests passing (8/8)

## High Priority (Phase 3: Feature Parity)

## Medium Priority (Feature Parity)

### Port Old Tests

- [ ] Review `symbolic/operators_test.cpp` and port relevant tests
- [ ] Review `symbolic/matchers_test.cpp` and port relevant tests
- [ ] Review `symbolic/derivative_test.cpp` and plan derivative implementation
- [ ] Review `symbolic/to_string_test.cpp` and plan toString enhancement

### Missing Operators

- [ ] Comparison operators: `==`, `!=`, `<`, `<=`, `>`, `>=`
- [ ] Logical operators: `&&`, `||`, `!`
- [ ] Bitwise operators: `&`, `|`, `^`, `~`, `<<`, `>>`
- [ ] Additional math functions: `abs`, `sqrt`, `tan`, `atan`, etc.

### Simplification Rules

- [ ] Add more algebraic identities
- [ ] Add factorization rules
- [ ] Add polynomial expansion
- [ ] Add fraction simplification
- [ ] Add trigonometric identities (beyond constants)

## Low Priority (Nice to Have)

### Derivative Support

- [ ] Create `derivative.h` header
- [ ] Port derivative rules from old implementation
- [ ] Add chain rule support
- [ ] Add product rule
- [ ] Add quotient rule
- [ ] Add tests for derivative functionality

### ToString Enhancement

- [ ] Create `to_string.h` header
- [ ] Implement basic expression formatting
- [ ] Add pretty-printing options
- [ ] Add LaTeX output (optional)
- [ ] Add customizable formatting

### Documentation

- [ ] Create tutorial examples
- [ ] Document all pattern matching features
- [ ] Add API reference
- [ ] Create performance comparison with old implementation
- [ ] Add troubleshooting guide

### Performance

- [ ] Benchmark compilation times
- [ ] Compare template instantiation counts with old implementation
- [ ] Identify optimization opportunities
- [ ] Test with larger expressions

## Future Enhancements

### Advanced Features

- [ ] Matrix/vector symbolic computation
- [ ] Multi-variable calculus
- [ ] Integration (symbolic)
- [ ] Solving equations symbolically
- [ ] Series expansion

### Tooling

- [ ] Create migration script for old code
- [ ] Add expression visualizer
- [ ] Create debug helpers for template errors
- [ ] Add static analysis for expression complexity

## Research/Investigation

- [ ] Investigate memoization for simplification
- [ ] Research alternative simplification algorithms
- [ ] Explore ways to reduce template instantiation depth
- [ ] Consider hybrid compile-time/runtime approach for complex expressions

## Notes

### Template Depth Issue - Detailed Analysis

The current simplification algorithm uses recursive template instantiation. Each simplification step creates new types, and nested simplifications compound. Example:

```
(x + 1) * (x + 1)
→ distributes to: (x * (x + 1)) + (1 * (x + 1))
→ distributes further: (x*x + x*1) + (1*x + 1*1)
→ each sub-expression gets simplified...
```

Each arrow is a new level of template instantiation, and with 900 limit, deep nesting quickly exceeds the limit.

**Potential Solutions**:

1. Limit recursion explicitly in code
2. Use iteration instead of recursion where possible
3. Special-case common patterns
4. Evaluate constants earlier in the pipeline
5. Use "simplification strategies" that know when to stop

---

**Last Updated**: October 4, 2025
