# Symbolic3 Learning Resources

Curated references for understanding the theory and practice behind symbolic3's design.

## Term Rewriting Systems & Computer Algebra

### Books

- **"Term Rewriting and All That"** by Baader & Nipkow - Formal foundation for rewrite systems, termination, and confluence
- **"Computer Algebra and Symbolic Computation"** by Joel S. Cohen - Practical CAS algorithms including polynomial simplification

### Key Concepts

- **Abstract Rewriting Systems (ARS)** - General theory of rewriting
- **Termination and Confluence (Church-Rosser)** - Ensures simplification terminates with unique results
- **Knuth-Bendix Completion** - Algorithm for creating confluent term rewriting systems
- **Canonical Forms** - Unique representation for equivalent mathematical objects

### Online Resources

- Wikipedia: "Canonical form" - Overview with examples from various mathematical domains
- GeeksforGeeks: "Canonical and Standard Form" - Introduction to canonical representations

## C++ Template Metaprogramming for Symbolic Computation

### Talks & Videos

- **CppCon 2023: "Symbolic Calculus for High-performance Computing From Scratch Using C++23"** by Vincent Reverdy - Directly relevant to symbolic3's approach
- YouTube: "C++ Expression Templates" - Foundation technique for compile-time symbolic manipulation

### Papers & Articles

- "Compile-Time Symbolic Differentiation Using C++ Expression Templates" (Kourounis et al.) - Academic validation of the approach
- "The C++ Template Metaprogramming Technique That Runs Code at Compile Time" (Medium) - High-level TMP overview

## Debugging Template Metaprograms

### Practical Techniques

- Stack Overflow: "How to debug C++ constexpr functions" - Practical debugging patterns
- "Printf-style debugging for C++ template metaprograms" by Ivan Čukić - Undefined template trick for type inspection

### Tools

- **Templight** - Clang-based tool for visualizing template instantiation
- **CLion's Constexpr Debugger** - Step through `constexpr` execution at compile-time
- Compiler flags: `-ftime-report`, `-ftime-trace`, `-ftemplate-depth`

## Related Work

### Symbolic Math Libraries

- SymbolicC++ - Early C++ symbolic computation library
- Symengine - Fast symbolic manipulation engine in C++
- GiNaC - C++ library for symbolic mathematics

### Expression Template Libraries

- Eigen - Uses expression templates for linear algebra
- Blaze - High-performance math library with expression templates
- Boost.Proto - Domain-specific embedded language toolkit

## Implementation Patterns

### Relevant Design Patterns

- Expression Templates - Delayed evaluation through type encoding
- Type Erasure - When runtime polymorphism is needed
- Policy-Based Design - Configurable behavior through template parameters
- Combinator Pattern - Composable transformation strategies

### C++20/23/26 Features Used

- Concepts - Type constraints for strategy composition
- `constexpr` - Compile-time evaluation
- Stateless lambdas - Unique type identity
- Template parameter packs - Variadic operations
- `std::format` - Modern string formatting

## See Also

- `README.md` - Quick start and usage guide
- `NEXT_STEPS.md` - Future development plans
- `DEBUGGING.md` - Practical debugging guide
- Example programs in `examples/` directory
- Test suite in `test/` directory
