# Matrix2

A C++26 matrix library focused on constexpr evaluations.

## Design Notes

- Assume that the elements of a matrix are small <= 16 bytes. This allows for
efficient copying of elements instead of returning const refs. However, also
assume that the elements could be 16 bytes long, so use refs and pointers
whenever referring to multiple elements.

Why? This assumption helps keep the code simple and efficient. If the elements
is less <= 16 bytes, it is feasible that the compiler will keep the return
value inside the registers.

- All classes that take a matrix in their constructor should make a copy or
move the matrix into the class. 

If users want to avoid copies without fully transferring ownership, they can
pass `MatRef<MatrixT>{m}`;

Why? This keeps interfaces and ownership rules clear and consistent.
You immediately know that `Transpose<Dense<...>>` owns the matrix, while
`Transpose<MatRef<Dense<...>>>` does not.

- constexpr all the things!

Why? C++26 offers more constexpr math support, this library is built around
constexpr.

## Style and Conventions

- Use the term `Inplace` to indicate the object is stack allocated.

This matches the C++ standard library's naming convention.

- Loosely follow Google's C++ Style Guide.

- Use `and` and `or` instead of `&&` and `||`. (Hard habit to break tho)

Why? I like it

## TODO

- [ ] Rename Inline* to Inplace* to more closely match the C++ standard
