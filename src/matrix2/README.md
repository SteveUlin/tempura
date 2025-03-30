# Matrix2

A C++26 matrix library focused on constexpr evaluations.

## Design Notes

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

- Loosely follow Google's C++ Style Guide.

It's just what I'm used to.

- Use `and` and `or` instead of `&&` and `||`. (Hard habit to break tho)

Why? I like it

## TODO

- [ ] Create a header file that include all storage matrices and arithmetic ops.
- [x] Banded matrix
- [x] Banded LU decomposition
- [ ] Transpose matrix
- [ ] Triangular matrix

