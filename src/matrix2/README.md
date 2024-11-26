## Design Notes

- Assume that the elements of a matrix are small <= 16 bytes. This allows for
efficient copying of elements instead of returning const refs. However, also
assume that the elements could be 16 bytes long, so use const refs whenever
referring to multiple elements.

- constexpr all the things!

- Use the term `Inplace` to indicate the object is stack allocated.

- Loosely follow Google's C++ Style Guide.

- Use `and` and `or` instead of `&&` and `||`. (Hard habit to break tho)

## TODO

- [ ] Rename Inline* to Inplace* to more closely match the C++ standard
