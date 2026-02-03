// Tests for symbolic4/indexed/dim.h
#include "symbolic4/indexed/dim.h"
#include "unit.h"

#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // IndexedSymbol type traits
  // ===========================================================================

  "IndexedSymbol is recognized by type trait"_test = [] {
    struct Obs {};
    struct Id {};
    using Sym = IndexedSymbol<Id, Obs>;

    static_assert(is_indexed_symbol_v<Sym>);
    static_assert(!is_indexed_symbol_v<int>);
  };

  "IndexedSymbol has correct ndims"_test = [] {
    struct D1 {};
    struct D2 {};
    struct D3 {};
    struct Id {};

    static_assert(IndexedSymbol<Id, D1>::ndims == 1);
    static_assert(IndexedSymbol<Id, D1, D2>::ndims == 2);
    static_assert(IndexedSymbol<Id, D1, D2, D3>::ndims == 3);
  };

  "IndexedSymbol has_dim works"_test = [] {
    struct Countries {};
    struct Years {};
    struct Other {};
    struct Id {};

    using Sym = IndexedSymbol<Id, Countries, Years>;

    static_assert(Sym::has_dim<Countries>);
    static_assert(Sym::has_dim<Years>);
    static_assert(!Sym::has_dim<Other>);
  };

  "indexed_symbol_ndims_v helper"_test = [] {
    struct D1 {};
    struct D2 {};
    struct Id {};

    using Sym1D = IndexedSymbol<Id, D1>;
    using Sym2D = IndexedSymbol<Id, D1, D2>;

    static_assert(indexed_symbol_ndims_v<Sym1D> == 1);
    static_assert(indexed_symbol_ndims_v<Sym2D> == 2);
  };

  // ===========================================================================
  // Shape
  // ===========================================================================

  "Shape stores sizes"_test = [] {
    struct D1 {};
    struct D2 {};
    Shape<D1, D2> shape{3, 5};

    expectEq(3UL, shape[0]);
    expectEq(5UL, shape[1]);
    expectEq(2UL, shape.size());
  };

  "Shape total() computes product"_test = [] {
    struct D1 {};
    struct D2 {};
    struct D3 {};

    Shape<D1, D2> shape2d{3, 5};
    expectEq(15UL, shape2d.total());

    Shape<D1, D2, D3> shape3d{2, 3, 4};
    expectEq(24UL, shape3d.total());
  };

  "Shape get<I>() accessor"_test = [] {
    struct D1 {};
    struct D2 {};
    Shape<D1, D2> shape{10, 20};

    expectEq(10UL, shape.get<0>());
    expectEq(20UL, shape.get<1>());
  };

  // ===========================================================================
  // IndexedValues (1D)
  // ===========================================================================

  "IndexedValues from vector"_test = [] {
    std::vector<double> data = {1.0, 2.0, 3.0};
    IndexedValues iv{data};

    expectEq(3UL, iv.size());
    expectNear(1.0, iv[0], 1e-10);
    expectNear(2.0, iv[1], 1e-10);
    expectNear(3.0, iv[2], 1e-10);
  };

  "indexed() factory for 1D"_test = [] {
    std::vector<double> data = {1.0, 2.0, 3.0};
    auto iv = indexed(data);

    expectEq(3UL, iv.size());
    expectNear(2.0, iv[1], 1e-10);
  };

  // ===========================================================================
  // IndexedValuesND (multi-dimensional)
  // ===========================================================================

  "IndexedValuesND 2D access"_test = [] {
    struct Rows {};
    struct Cols {};
    // 2x3 matrix in row-major: [[1,2,3],[4,5,6]]
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    Shape<Rows, Cols> shape{2, 3};
    IndexedValuesND<Rows, Cols> iv{data, shape};

    expectNear(1.0, iv.at(0, 0), 1e-10);
    expectNear(3.0, iv.at(0, 2), 1e-10);
    expectNear(4.0, iv.at(1, 0), 1e-10);
    expectNear(6.0, iv.at(1, 2), 1e-10);
  };

  "indexed() factory for 2D"_test = [] {
    struct D1 {};
    struct D2 {};
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0};
    auto iv = indexed(data, Shape<D1, D2>{2, 2});

    expectNear(1.0, iv.at(0, 0), 1e-10);
    expectNear(4.0, iv.at(1, 1), 1e-10);
  };

  // ===========================================================================
  // IndexedBinding (1D)
  // ===========================================================================

  "IndexedBinding 1D stores values"_test = [] {
    struct Obs {};
    struct Id {};
    using Sym = IndexedSymbol<Id, Obs>;

    std::vector<double> data = {0.3, 0.5, 0.7};
    IndexedBinding<Sym, 1> binding{std::span<const double>(data)};

    expectEq(3UL, binding.size());
    expectNear(0.3, binding.at(0), 1e-10);
    expectNear(0.7, binding.at(2), 1e-10);
  };

  "IndexedBinding 1D dimSize"_test = [] {
    struct Obs {};
    struct Id {};
    using Sym = IndexedSymbol<Id, Obs>;

    std::vector<double> data = {1.0, 2.0, 3.0, 4.0};
    IndexedBinding<Sym, 1> binding{std::span<const double>(data)};

    expectEq(4UL, binding.dimSize<Obs>());
  };

  // ===========================================================================
  // IndexedBinding (2D)
  // ===========================================================================

  "IndexedBinding 2D stores values"_test = [] {
    struct Countries {};
    struct Years {};
    struct Id {};
    using Sym = IndexedSymbol<Id, Countries, Years>;

    // 2 countries x 3 years
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    IndexedBinding<Sym, 2> binding{std::span<const double>(data), {2, 3}};

    expectNear(1.0, binding.at(0, 0), 1e-10);
    expectNear(3.0, binding.at(0, 2), 1e-10);
    expectNear(6.0, binding.at(1, 2), 1e-10);
  };

  "IndexedBinding 2D dimSize"_test = [] {
    struct Countries {};
    struct Years {};
    struct Id {};
    using Sym = IndexedSymbol<Id, Countries, Years>;

    std::vector<double> data(6);
    IndexedBinding<Sym, 2> binding{std::span<const double>(data), {2, 3}};

    expectEq(2UL, binding.dimSize<Countries>());
    expectEq(3UL, binding.dimSize<Years>());
  };

  // ===========================================================================
  // is_indexed_binding_v type trait
  // ===========================================================================

  "is_indexed_binding_v identifies bindings"_test = [] {
    struct Obs {};
    struct Id {};
    using Sym = IndexedSymbol<Id, Obs>;
    using Binding = IndexedBinding<Sym, 1>;

    static_assert(is_indexed_binding_v<Binding>);
    static_assert(!is_indexed_binding_v<int>);
  };

  // ===========================================================================
  // symbolic4::detail::At helper
  // ===========================================================================

  "symbolic4::detail::At extracts types from TypeList"_test = [] {
    struct A {};
    struct B {};
    struct C {};

    static_assert(std::is_same_v<symbolic4::detail::At<0, TypeList<A, B, C>>, A>);
    static_assert(std::is_same_v<symbolic4::detail::At<1, TypeList<A, B, C>>, B>);
    static_assert(std::is_same_v<symbolic4::detail::At<2, TypeList<A, B, C>>, C>);
  };

  // ===========================================================================
  // symbolic4::detail::IndexOf helper
  // ===========================================================================

  "symbolic4::detail::IndexOf finds type index"_test = [] {
    struct A {};
    struct B {};
    struct C {};
    struct D {};

    static_assert(symbolic4::detail::IndexOf<A, TypeList<A, B, C>> == 0);
    static_assert(symbolic4::detail::IndexOf<B, TypeList<A, B, C>> == 1);
    static_assert(symbolic4::detail::IndexOf<C, TypeList<A, B, C>> == 2);
    static_assert(symbolic4::detail::IndexOf<D, TypeList<A, B, C>> == static_cast<SizeT>(-1));
  };

  return TestRegistry::result();
}
