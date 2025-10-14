# Gemini Code Assistant Context

## Project Overview

This is `tempura`, an experimental C++26 library for numerical methods. The library is header-only and focuses on `constexpr` and template metaprogramming to achieve compile-time symbolic math, automatic differentiation, and other features. The project uses C++26 and is built with CMake. The development environment is managed with Nix flakes.

The project is structured into three main directories:

*   `src`: Contains the source code of the library, organized into modules.
*   `test`: Contains the tests for the library, which are built and run using CTest.
*   `examples`: Contains example usage of the library.

## Building and Running

The project is built using CMake and Ninja. The development environment is managed with Nix flakes.

To build the project, you need to have Nix installed and configured. Then, you can enter the development environment by running:

```bash
nix develop
```

Once in the Nix shell, you can build the project using the following commands:

```bash
cmake -B build -G Ninja
cmake --build build
```

To run the tests, you can use the following command:

```bash
cd build && ctest
```

## Development Conventions

*   **Coding Style**: The project follows the Google Style Guide with some modifications. `clang-format` is used to enforce the style.
*   **Testing**: The project uses CTest for testing. Tests are located in the `test` directory and are built along with the library.
*   **Dependencies**: The project dependencies are managed with Nix flakes. The `flake.nix` file defines the development environment and all the necessary dependencies.
*   **Modules**: The library is organized into modules, each in its own subdirectory in the `src` directory. Most modules are header-only and are exposed as `INTERFACE` libraries in CMake.
