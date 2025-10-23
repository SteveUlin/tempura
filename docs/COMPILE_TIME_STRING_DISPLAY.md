# Displaying StaticString at Compile-Time via Compiler Errors

This guide explains techniques for displaying the contents of `StaticString` (tempura's compile-time string type) in compiler error messages. This is invaluable for debugging symbolic expressions, inspecting toString results, and understanding template instantiations.

## Quick Start

The easiest way to display a compile-time string is using the undefined template technique:

```cpp
#include "symbolic3/debug.h"

// IMPORTANT: Use specific namespace imports to avoid ambiguity
using namespace tempura::symbolic3;
using tempura::operator""_cts;  // Bring in _cts literal

// Create a string
constexpr auto msg = "Hello, World!"_cts;

// Display it in a compiler error
ShowStaticString<msg> show;  // ERROR: Will show "Hello, World!" in error message
```

Or use the convenience macro:

```cpp
SHOW_STATIC_STRING(toString(expr));
```

**⚠️ Important - Namespace Usage:**
- Do NOT use `using namespace tempura;` - causes `toString` ambiguity
- Use `using namespace tempura::symbolic3;` instead
- Explicitly import needed items: `using tempura::operator""_cts;`
- Reason: Multiple `toString` functions exist (json, matrix, symbolic3)

## Why This Works

C++ compilers display template parameters in error messages. When you try to instantiate an undefined template with a Non-Type Template Parameter (NTTP), the compiler shows the parameter's value in the error diagnostic.

For example:
```
error: aggregate 'ShowStaticString<tempura::StaticString<16>{"x0 * x0 + x1 * 3"}> display'
       has incomplete type and cannot be defined
```

The string content `"x0 * x0 + x1 * 3"` is clearly visible!

## Available Techniques

### Technique 1: Undefined Template (Recommended)

**Best for:** Quick debugging, inspecting expression strings, verifying toString output

```cpp
template <StaticString S>
struct ShowStaticString;  // Declared but never defined

void example() {
  constexpr auto x = Symbol<decltype([] {})>{};
  constexpr auto expr = x * x + Constant<5>{};
  constexpr auto str = toString(expr);

  ShowStaticString<str> show;  // ERROR shows "x0 * x0 + 5"
}
```

**Pros:**
- Simple and direct
- String appears clearly in error message
- Works with clangd, GCC, Clang
- No extra dependencies

**Cons:**
- Requires commenting out after debugging

### Technique 2: Static Assert

**Best for:** More controlled error messages, conditional debugging

```cpp
template <StaticString S>
consteval void show_string_in_error() {
  static_assert(S.size() == static_cast<size_t>(-1),  // Always false
                "String shown in template parameter S");
}

void example() {
  constexpr auto msg = "Debug message"_cts;
  show_string_in_error<msg>();  // ERROR shows message with string
}
```

**Pros:**
- Custom error message
- Can add context/explanation
- Slightly cleaner to read

**Cons:**
- Slightly more verbose
- String may be less prominent in error

### Technique 3: Macro Wrapper

**Best for:** Temporary debugging that's easy to enable/disable

```cpp
#include "symbolic3/debug.h"

void example() {
  constexpr auto x = Symbol<decltype([] {})>{};
  constexpr auto result = toString(x + x);

  SHOW_STATIC_STRING(result);  // Shows "x0 + x0"
}
```

**Pros:**
- Most convenient
- Consistent with other debug macros
- Easy to search for and remove

**Cons:**
- Macro-based (some prefer avoiding macros)

## Practical Examples

### Example 1: Debugging Symbolic Expression Simplification

```cpp
#include "symbolic3/debug.h"
#include "symbolic3/simplify.h"

void debug_simplification() {
  constexpr auto x = Symbol<decltype([] {})>{};

  // Original expression
  constexpr auto before = x * Constant<1>{} + Constant<0>{};
  constexpr auto before_str = toString(before);

  // After simplification
  constexpr auto after = simplify(before, default_context());
  constexpr auto after_str = toString(after);

  // Display both to verify simplification
  // Uncomment to see:
  // ShowStaticString<before_str> show1;  // "x0 * 1 + 0"
  // ShowStaticString<after_str> show2;   // "x0"
}
```

### Example 2: Inspecting Custom Variable Names

```cpp
void debug_custom_names() {
  constexpr auto alpha = Symbol<decltype([] {})>{};
  constexpr auto beta = Symbol<decltype([] {})>{};

  constexpr auto expr = alpha * alpha + beta;

  // Default names
  constexpr auto default_str = toString(expr);
  // ShowStaticString<default_str> s1;  // "x0 * x0 + x1"

  // Custom names
  constexpr auto ctx = makeSymbolNames(alpha, "α"_cts, beta, "β"_cts);
  constexpr auto custom_str = toString(expr, ctx);
  // ShowStaticString<custom_str> s2;  // "α * α + β"
}
```

### Example 3: Verifying toString Implementation

```cpp
void test_new_operator_display() {
  constexpr auto x = Symbol<decltype([] {})>{};

  // Testing a new operator's toString
  constexpr auto expr = sinh(x) + cosh(x);
  constexpr auto str = toString(expr);

  // Verify the output format
  // ShowStaticString<str> show;  // Check operator symbols are correct

  static_assert(str == "sinh(x0) + cosh(x0)"_cts);
}
```

### Example 4: Debugging Complex Nested Expressions

```cpp
void debug_nested_expression() {
  constexpr auto x = Symbol<decltype([] {})>{};
  constexpr auto y = Symbol<decltype([] {})>{};

  constexpr auto expr = sin(x * y) / (cos(x) + exp(y));
  constexpr auto str = toString(expr);

  // Verify precedence and parenthesization
  // ShowStaticString<str> show;  // "sin(x0 * x1) / (cos(x0) + exp(x1))"
}
```

## Integration with clangd

When using clangd in your editor (VSCode, Neovim, etc.):

1. Add the `ShowStaticString` instantiation
2. clangd will underline the error in red
3. Hover over the error to see the full diagnostic
4. The string content will be visible in the template parameter

**VSCode Example:**
```
[Error] implicit instantiation of undefined template
        'tempura::symbolic3::ShowStaticString<tempura::StaticString<16>{"x0 * x0 + x1 * 3"}>'
```

## Best Practices

1. **Comment Out After Debugging**
   - These errors are intentional but should not be committed
   - Use `// SHOW_STATIC_STRING(...)` when not actively debugging

2. **Use Descriptive Variable Names**
   ```cpp
   constexpr auto expr_before_simplify = toString(before);
   constexpr auto expr_after_simplify = toString(after);

   // ShowStaticString<expr_before_simplify> show1;
   // ShowStaticString<expr_after_simplify> show2;
   ```

3. **Combine with Type Display**
   ```cpp
   constexpr auto result = simplify(expr);

   // Show both type and string
   // CONSTEXPR_PRINT_TYPE(decltype(result));
   // SHOW_STATIC_STRING(toString(result));
   ```

4. **Use in Static Assertions for Permanent Tests**
   ```cpp
   "toString produces correct format"_test = [] {
     constexpr auto x = Symbol<decltype([] {})>{};
     constexpr auto str = toString(x + x);

     static_assert(str == "x0 + x0"_cts,
                   "Verify addition format");
   };
   ```

## Limitations

1. **String Length**: Very long strings (>1000 chars) may be truncated in some compilers
2. **Special Characters**: Unicode characters display correctly in GCC/Clang but may have issues in MSVC
3. **Compile Time**: Each instantiation adds to compile time (minimal impact)

## Troubleshooting

### "toString is ambiguous" error in clangd

**Symptom:** clangd shows ambiguity errors but GCC compiles fine

**Cause:** Multiple `toString` functions exist in different namespaces:
- `tempura::symbolic3::toString` (symbolic expressions)
- `tempura::matrix2::toString` (matrices)
- `tempura::toString` (JSON)

**Solution:** Use specific namespace imports instead of `using namespace tempura;`

```cpp
// ❌ BAD - causes ambiguity in clangd
using namespace tempura;
using namespace tempura::symbolic3;

// ✅ GOOD - explicit and unambiguous
using namespace tempura::symbolic3;
using tempura::StaticString;
using tempura::operator""_cts;
```

**Why clangd differs from GCC:**
- clangd is stricter about ambiguity detection
- GCC may use argument-dependent lookup (ADL) differently
- clangd reports errors earlier in the compilation process
- Always fix clangd errors for better code quality

### Template parameter not showing string content

**Symptom:** Error shows `ShowStaticString<...>` but no string content

**Cause:** The string might not be a constexpr value

**Solution:** Ensure the string is `constexpr`:
```cpp
constexpr auto str = toString(expr);  // ✅ Good
auto str = toString(expr);            // ❌ May not work
```

## Alternative: Runtime Display

If compile-time display isn't working or the string is too long:

```cpp
#include "symbolic3/to_string.h"

void runtime_display() {
  constexpr auto x = Symbol<decltype([] {})>{};
  constexpr auto expr = x * x + x;

  // Runtime conversion and display
  auto str = toStringRuntime(expr);
  std::print("Expression: {}\n", str);
}
```

## Related Tools

- `CONSTEXPR_PRINT_TYPE(T)` - Display types in errors
- `CONSTEXPR_PRINT_EXPR(expr)` - Display expression (less clear than ShowStaticString)
- `debug_print(expr)` - Runtime string display
- `debug_print_tree(expr)` - Runtime tree visualization

## See Also

- `src/symbolic3/debug.h` - All compile-time debugging utilities
- `src/symbolic3/to_string.h` - StaticString conversion functions
- `src/symbolic3/test/static_string_display_test.cpp` - Working examples
- `src/symbolic3/test/static_string_display_demo.cpp` - Interactive demonstration
