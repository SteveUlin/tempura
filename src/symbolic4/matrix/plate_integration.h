#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/matrix/eval.h"
#include "symbolic4/matrix/ops.h"
#include "symbolic4/matrix/types.h"

// ============================================================================
// plate_integration.h - Matrix-valued plate expressions
// ============================================================================
//
// Extends the plate system to handle matrix-valued (vector-valued) expressions.
// When we have a plate of multivariate normal RVs, each observation is a
// K-dimensional vector. The data is stored as an N x K matrix where:
//   - N = plate dimension (number of observations)
//   - K = vector dimension (dimensionality of each observation)
//
// Key insight: A plate of MVN produces N K-dimensional vectors.
// The data is an N x K matrix where row i is observation i.
//
// Usage:
//   struct Observations {};  // Plate dimension tag
//   struct Dims {};          // Vector dimension tag
//
//   auto mu = dimVector<Dims>();
//   auto L = cholCov<Dims>();
//   auto y = plate<Observations>(mvnCholesky(mu, L));
//   // y[i] is K-dimensional for each i in Observations
//
//   // Bind N x K data matrix to plate of vectors
//   std::vector<double> data = ...;  // N * K elements, row-major
//   auto binding = y = indexedVectors<Observations, Dims>(data, n_obs, k_dim);
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// IndexedVectorSymbol - A plate of vectors
// ============================================================================
//
// Represents N vectors of dimension K, indexed by a plate dimension tag.
// When evaluated at plate index i, produces the i-th K-dimensional vector.
//
// Template parameters:
//   Id        - Unique identifier for this indexed vector
//   PlateDim  - Dimension tag for the plate (indexes over observations)
//   VecDim    - Dimension tag for the vector dimension (indexes over components)

template <typename Id, typename PlateDim, typename VecDim>
struct IndexedVectorSymbol : SymbolicTag {
  using id_type = Id;
  using plate_dim = PlateDim;
  using vec_dim = VecDim;
  static constexpr SizeT ndims = 1;  // Indexed by plate dimension only

  // Check if this symbol has a specific plate dimension
  template <typename DimTag>
  static constexpr bool has_plate_dim = std::is_same_v<DimTag, PlateDim>;
};

// Type traits
template <typename T>
struct IsIndexedVectorSymbol : std::false_type {};

template <typename Id, typename PlateDim, typename VecDim>
struct IsIndexedVectorSymbol<IndexedVectorSymbol<Id, PlateDim, VecDim>> : std::true_type {};

template <typename T>
constexpr bool is_indexed_vector_symbol_v = IsIndexedVectorSymbol<T>::value;

// ============================================================================
// IndexedVectorBinding - Binds a plate of vectors to N x K data
// ============================================================================
//
// Stores an N x K matrix in row-major order. When accessed at plate index i,
// returns a view of row i as a K-dimensional vector.
//
// Template parameters:
//   Sym - The IndexedVectorSymbol being bound

template <typename Sym>
  requires is_indexed_vector_symbol_v<Sym>
struct IndexedVectorBinding {
  using symbol_type = Sym;
  using plate_dim = typename Sym::plate_dim;
  using vec_dim = typename Sym::vec_dim;

  std::span<const double> data_;  // N x K, row-major
  SizeT n_obs_;                   // N = plate size
  SizeT k_dim_;                   // K = vector dimension

  IndexedVectorBinding(std::span<const double> data, SizeT n_obs, SizeT k_dim)
      : data_{data}, n_obs_{n_obs}, k_dim_{k_dim} {
    assert(data.size() >= n_obs * k_dim);
  }

  IndexedVectorBinding(const std::vector<double>& data, SizeT n_obs, SizeT k_dim)
      : data_{data}, n_obs_{n_obs}, k_dim_{k_dim} {
    assert(data.size() >= n_obs * k_dim);
  }

  // Number of observations (plate size)
  auto plateSize() const -> SizeT { return n_obs_; }

  // Vector dimension
  auto vecDim() const -> SizeT { return k_dim_; }

  // Get dimension size by tag
  template <typename DimTag>
  auto dimSize() const -> SizeT {
    if constexpr (std::is_same_v<DimTag, plate_dim>) {
      return n_obs_;
    } else if constexpr (std::is_same_v<DimTag, vec_dim>) {
      return k_dim_;
    } else {
      static_assert(std::is_same_v<DimTag, plate_dim> || std::is_same_v<DimTag, vec_dim>,
                    "Unknown dimension tag");
      return 0;
    }
  }

  // Access element at (plate_index, component_index)
  auto at(SizeT plate_idx, SizeT component_idx) const -> double {
    assert(plate_idx < n_obs_ && component_idx < k_dim_);
    return data_[plate_idx * k_dim_ + component_idx];
  }

  // Access row i as a view (returns a span of K elements)
  auto row(SizeT i) const -> std::span<const double> {
    assert(i < n_obs_);
    return data_.subspan(i * k_dim_, k_dim_);
  }

  // BinderPack lookup interface
  auto operator[](Sym) const -> const IndexedVectorBinding& { return *this; }
};

// Type traits for IndexedVectorBinding
template <typename T>
struct IsIndexedVectorBinding : std::false_type {};

template <typename Sym>
struct IsIndexedVectorBinding<IndexedVectorBinding<Sym>> : std::true_type {};

template <typename T>
constexpr bool is_indexed_vector_binding_v = IsIndexedVectorBinding<T>::value;

// ============================================================================
// IndexedVectorValues - Helper for binding syntax
// ============================================================================
//
// Allows: y = indexedVectors<Obs, Dims>(data, n, k)

template <typename PlateDim, typename VecDim>
struct IndexedVectorValues {
  std::span<const double> data;
  SizeT n_obs;
  SizeT k_dim;

  IndexedVectorValues(std::span<const double> d, SizeT n, SizeT k)
      : data{d}, n_obs{n}, k_dim{k} {}
  IndexedVectorValues(const std::vector<double>& v, SizeT n, SizeT k)
      : data{v}, n_obs{n}, k_dim{k} {}
};

// Factory function
template <typename PlateDim, typename VecDim>
auto indexedVectors(std::span<const double> data, SizeT n_obs, SizeT k_dim) {
  return IndexedVectorValues<PlateDim, VecDim>{data, n_obs, k_dim};
}

template <typename PlateDim, typename VecDim>
auto indexedVectors(const std::vector<double>& data, SizeT n_obs, SizeT k_dim) {
  return IndexedVectorValues<PlateDim, VecDim>{data, n_obs, k_dim};
}

// ============================================================================
// PlateVectorView - Runtime view of a single vector from a plate
// ============================================================================
//
// When evaluating an IndexedVectorSymbol at a specific plate index,
// we need to return something that acts like a vector for matrix operations.

class PlateVectorView {
 public:
  PlateVectorView(std::span<const double> data) : data_{data} {}

  auto size() const -> std::size_t { return data_.size(); }
  auto operator[](std::size_t i) const -> double { return data_[i]; }

  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }

 private:
  std::span<const double> data_;
};

// ============================================================================
// Vector arithmetic helpers for plate evaluation
// ============================================================================
//
// These operations are needed when combining indexed vectors with fixed vectors.
// For example: y[i] - mu where y[i] is PlateVectorView and mu is DynamicVector.

namespace plate_vector_ops {

// Vector subtraction: v1 - v2 -> DynamicVector
template <typename V1, typename V2>
auto vectorSub(const V1& v1, const V2& v2) -> DynamicVector {
  assert(v1.size() == v2.size());
  DynamicVector result(v1.size());
  for (std::size_t i = 0; i < v1.size(); ++i) {
    result[i] = v1[i] - v2[i];
  }
  return result;
}

// Vector addition: v1 + v2 -> DynamicVector
template <typename V1, typename V2>
auto vectorAdd(const V1& v1, const V2& v2) -> DynamicVector {
  assert(v1.size() == v2.size());
  DynamicVector result(v1.size());
  for (std::size_t i = 0; i < v1.size(); ++i) {
    result[i] = v1[i] + v2[i];
  }
  return result;
}

// Scalar-vector multiplication: s * v -> DynamicVector
template <typename V>
auto scalarMul(double s, const V& v) -> DynamicVector {
  DynamicVector result(v.size());
  for (std::size_t i = 0; i < v.size(); ++i) {
    result[i] = s * v[i];
  }
  return result;
}

// Vector negation: -v -> DynamicVector
template <typename V>
auto vectorNeg(const V& v) -> DynamicVector {
  DynamicVector result(v.size());
  for (std::size_t i = 0; i < v.size(); ++i) {
    result[i] = -v[i];
  }
  return result;
}

// Type traits to detect vector-like types
template <typename T>
constexpr bool is_vector_like_v =
    std::is_same_v<T, PlateVectorView> ||
    std::is_same_v<T, DynamicVector> ||
    std::is_same_v<T, const DynamicVector&>;

}  // namespace plate_vector_ops

// ============================================================================
// MatrixPlateEval - Extends MatrixEval for indexed vector lookups
// ============================================================================
//
// This interpreter handles both regular matrix bindings and indexed vector
// bindings. For indexed vectors, it uses the current plate index from
// DimIndexMap to select the appropriate row.
//
// Template parameters:
//   ScalarBindings  - BinderPack of scalar bindings
//   MatrixBindings  - Bindings for DimVectorSymbol and CholeskySymbol
//   IndexedBindings - Bindings for IndexedVectorSymbol (plates of vectors)

namespace matrix_dim_detail {

// Collect plate dims from IndexedVectorBindings in a BinderPack
template <typename B>
struct VecDimsOf {
  using Type = TypeList<>;
};

template <typename B>
  requires is_indexed_vector_binding_v<B>
struct VecDimsOf<B> {
  using Type = TypeList<typename B::plate_dim>;
};

template <typename IndexedBindings>
struct CollectVecDims;

template <typename... Binders>
struct CollectVecDims<BinderPack<Binders...>> {
  using Type = Unique_t<Concat_t<typename VecDimsOf<Binders>::Type...>>;
};

template <typename DimsList>
struct MakeVecDimMap;

template <typename... DimTags>
struct MakeVecDimMap<TypeList<DimTags...>> {
  using Type = StaticDimIndexMap<DimTags...>;
};

}  // namespace matrix_dim_detail

template <typename ScalarBindings, typename MatrixBindings, typename IndexedBindings>
struct MatrixPlateEval {
  using result_type = double;
  using DimMapType = typename matrix_dim_detail::MakeVecDimMap<
      typename matrix_dim_detail::CollectVecDims<IndexedBindings>::Type>::Type;

  struct context_type {
    ScalarBindings scalars;
    MatrixBindings matrices;
    IndexedBindings indexed_vecs;
    DimMapType dim_indices;
  };

  // Terminal handling - dispatch based on terminal type
  template <typename T>
  static auto terminal(T term, context_type& ctx) {
    if constexpr (is_constant_v<T>) {
      return static_cast<double>(T::value);
    } else if constexpr (is_fraction_v<T>) {
      return T::value;
    } else if constexpr (is_indexed_vector_symbol_v<T>) {
      // Look up plate index and return vector view
      return lookupIndexedVector<T>(term, ctx);
    } else if constexpr (is_dim_vector_symbol_v<T> || is_cholesky_symbol_v<T>) {
      return ctx.matrices[term];
    } else if constexpr (is_atom_v<T>) {
      return lookupScalar(term, ctx);
    } else {
      static_assert(is_atom_v<T>, "Unknown terminal type");
      return 0.0;
    }
  }

  // Combine child results using the operator functor
  // Note: Op is taken as a parameter (not template parameter) to match indexedFold calling convention
  template <typename Op, typename... Rs>
  static auto combine(context_type&, [[maybe_unused]] Op op, Rs... children) {
    // Matrix/vector operations
    if constexpr (std::is_same_v<Op, DotOp>) {
      return matrix_eval_detail::evalDot(children...);
    } else if constexpr (std::is_same_v<Op, LogDetCholOp>) {
      return matrix_eval_detail::evalLogDetChol(children...);
    } else if constexpr (std::is_same_v<Op, SolveTriangularOp>) {
      return matrix_eval_detail::evalSolveTriangular(children...);
    } else if constexpr (std::is_same_v<Op, DiagPreMultOp>) {
      return matrix_eval_detail::evalDiagPreMult(children...);
    } else if constexpr (std::is_same_v<Op, QuadFormCholOp>) {
      return matrix_eval_detail::evalQuadFormChol(children...);
    }
    // Vector arithmetic: check if we have vector-like operands
    else if constexpr (std::is_same_v<Op, SubOp> && sizeof...(Rs) == 2) {
      return combineVectorSub(children...);
    } else if constexpr (std::is_same_v<Op, AddOp> && sizeof...(Rs) == 2) {
      return combineVectorAdd(children...);
    } else if constexpr (std::is_same_v<Op, NegOp> && sizeof...(Rs) == 1) {
      return combineVectorNeg(children...);
    } else {
      // Fall back to standard operator evaluation for scalar ops
      return op(children...);
    }
  }

 private:
  // Vector subtraction: handles PlateVectorView - DynamicVector etc.
  template <typename V1, typename V2>
  static auto combineVectorSub(const V1& v1, const V2& v2) {
    constexpr bool v1_is_vec = plate_vector_ops::is_vector_like_v<std::decay_t<V1>>;
    constexpr bool v2_is_vec = plate_vector_ops::is_vector_like_v<std::decay_t<V2>>;

    if constexpr (v1_is_vec && v2_is_vec) {
      return plate_vector_ops::vectorSub(v1, v2);
    } else {
      // Scalar subtraction
      return v1 - v2;
    }
  }

  // Vector addition: handles PlateVectorView + DynamicVector etc.
  template <typename V1, typename V2>
  static auto combineVectorAdd(const V1& v1, const V2& v2) {
    constexpr bool v1_is_vec = plate_vector_ops::is_vector_like_v<std::decay_t<V1>>;
    constexpr bool v2_is_vec = plate_vector_ops::is_vector_like_v<std::decay_t<V2>>;

    if constexpr (v1_is_vec && v2_is_vec) {
      return plate_vector_ops::vectorAdd(v1, v2);
    } else {
      // Scalar addition
      return v1 + v2;
    }
  }

  // Vector negation
  template <typename V>
  static auto combineVectorNeg(const V& v) {
    if constexpr (plate_vector_ops::is_vector_like_v<std::decay_t<V>>) {
      return plate_vector_ops::vectorNeg(v);
    } else {
      return -v;
    }
  }

  // Look up indexed vector at current plate index
  template <typename Sym>
  static auto lookupIndexedVector([[maybe_unused]] Sym sym, context_type& ctx) -> PlateVectorView {
    using PlateDim = typename Sym::plate_dim;
    const auto& binding = ctx.indexed_vecs[sym];
    SizeT plate_idx = ctx.dim_indices.template get<PlateDim>();
    return PlateVectorView{binding.row(plate_idx)};
  }

  // Look up scalar — Sample atoms resolve via corresponding Free symbol
  template <typename T>
  static auto lookupScalar([[maybe_unused]] T term, context_type& ctx) -> double {
    if constexpr (is_random_var_atom_v<T>) {
      using FreeSymbol = Atom<get_id_t<T>, Free>;
      return ctx.scalars[FreeSymbol{}];
    } else {
      return ctx.scalars[term];
    }
  }
};

// ============================================================================
// Helper: Separate different types of bindings
// ============================================================================

namespace plate_detail {

template <typename T>
constexpr bool is_matrix_binding =
    is_dim_vector_binding_v<T> || is_cholesky_binding_v<T>;

template <typename... Binders>
struct SeparateMatrixBindings {
  template <typename B>
  static auto scalarPart(const B& b) {
    if constexpr (is_matrix_binding<B> || is_indexed_vector_binding_v<B>) {
      return std::tuple<>{};
    } else {
      return std::tuple<B>{b};
    }
  }

  template <typename B>
  static auto matrixPart(const B& b) {
    if constexpr (is_matrix_binding<B>) {
      return std::tuple<B>{b};
    } else {
      return std::tuple<>{};
    }
  }

  template <typename B>
  static auto indexedVecPart(const B& b) {
    if constexpr (is_indexed_vector_binding_v<B>) {
      return std::tuple<B>{b};
    } else {
      return std::tuple<>{};
    }
  }

  static auto separate(const BinderPack<Binders...>& pack) {
    auto scalars = std::tuple_cat(scalarPart(static_cast<const Binders&>(pack))...);
    auto matrices = std::tuple_cat(matrixPart(static_cast<const Binders&>(pack))...);
    auto indexed = std::tuple_cat(indexedVecPart(static_cast<const Binders&>(pack))...);
    return std::tuple{scalars, matrices, indexed};
  }
};

template <typename... Ts>
auto tupleToBinderPack(std::tuple<Ts...> t) {
  return std::apply([](auto... bs) { return BinderPack{bs...}; }, t);
}

inline auto tupleToBinderPack(std::tuple<>) { return BinderPack<>{}; }

template <typename... Ts>
auto tupleToMatrixBinderPack(std::tuple<Ts...> t) {
  return std::apply([](auto... bs) { return MatrixBinderPack{bs...}; }, t);
}

inline auto tupleToMatrixBinderPack(std::tuple<>) { return MatrixBinderPack<>{}; }

}  // namespace plate_detail

// ============================================================================
// Get plate dimension size from indexed vector bindings
// ============================================================================

template <typename DimTag, typename Bindings>
struct GetPlateSizeImpl;

template <typename DimTag>
struct GetPlateSizeImpl<DimTag, BinderPack<>> {
  static auto get(const BinderPack<>&) -> SizeT { return 0; }
};

template <typename DimTag, typename First, typename... Rest>
struct GetPlateSizeImpl<DimTag, BinderPack<First, Rest...>> {
  static auto get(const BinderPack<First, Rest...>& pack) -> SizeT {
    if constexpr (is_indexed_vector_binding_v<First>) {
      using Sym = typename First::symbol_type;
      if constexpr (std::is_same_v<DimTag, typename Sym::plate_dim>) {
        return static_cast<const First&>(pack).plateSize();
      } else {
        return GetPlateSizeImpl<DimTag, BinderPack<Rest...>>::get(
            static_cast<const BinderPack<Rest...>&>(pack));
      }
    } else {
      return GetPlateSizeImpl<DimTag, BinderPack<Rest...>>::get(
          static_cast<const BinderPack<Rest...>&>(pack));
    }
  }
};

template <typename DimTag, typename Bindings>
auto getPlateSizeFromBindings(const Bindings& bindings) -> SizeT {
  return GetPlateSizeImpl<DimTag, Bindings>::get(bindings);
}

// ============================================================================
// matrixPlateFold - Fold with SumOver support for matrix-valued plates
// ============================================================================

namespace plate_detail {

template <typename Interp, typename Op, typename Ctx, SizeT... Is, typename... Args>
auto matrixPlateFoldExpression([[maybe_unused]] Expression<Op, Args...> expr,
                                Ctx& ctx, IndexSequence<Is...>) {
  return Interp::combine(ctx, Op{},
      matrixPlateFold<Interp>(expr.template arg<Is>(), ctx)...);
}

}  // namespace plate_detail

template <typename Interp, Symbolic E, typename Ctx>
auto matrixPlateFold(E expr, Ctx& ctx);

template <typename Interp, Symbolic E, typename Ctx>
auto matrixPlateFold(E expr, Ctx& ctx) {
  if constexpr (is_sum_over_v<E>) {
    // Handle SumOver - loop over plate dimension and sum
    using DimTag = typename E::dim_tag;
    auto size = getPlateSizeFromBindings<DimTag>(ctx.indexed_vecs);

    double total = 0.0;
    for (SizeT i = 0; i < size; ++i) {
      ctx.dim_indices.template set<DimTag>(i, size);
      total += matrixPlateFold<Interp>(expr.expr_, ctx);
    }
    return total;
  } else if constexpr (is_expression_v<E>) {
    return plate_detail::matrixPlateFoldExpression<Interp>(
        expr, ctx, MakeIndexSequence<E::arity>{});
  } else {
    return Interp::terminal(expr, ctx);
  }
}

// ============================================================================
// evaluateMatrixPlate - Main entry point
// ============================================================================
//
// Evaluates an expression that may contain:
//   - Scalar symbols bound to double values
//   - DimVectorSymbol bound to DynamicVector
//   - CholeskyCovSymbol bound to DynamicMatrix
//   - IndexedVectorSymbol bound to N x K data matrix
//   - SumOver expressions for plate reduction
//
// Usage:
//   auto result = evaluateMatrixPlate(expr, scalar_binders..., matrix_binders..., indexed_vec_binders...);

template <Symbolic E, typename... Binders>
auto evaluateMatrixPlate(E expr, BinderPack<Binders...> bindings) -> double {
  auto [scalar_tuple, matrix_tuple, indexed_tuple] =
      plate_detail::SeparateMatrixBindings<Binders...>::separate(bindings);

  auto scalars = plate_detail::tupleToBinderPack(scalar_tuple);
  auto matrices = plate_detail::tupleToMatrixBinderPack(matrix_tuple);
  auto indexed_vecs = plate_detail::tupleToBinderPack(indexed_tuple);

  using ScalarBindings = decltype(scalars);
  using MatrixBindings = decltype(matrices);
  using IndexedBindings = decltype(indexed_vecs);
  using Interp = MatrixPlateEval<ScalarBindings, MatrixBindings, IndexedBindings>;

  typename Interp::context_type ctx{scalars, matrices, indexed_vecs, {}};
  return matrixPlateFold<Interp>(expr, ctx);
}

template <Symbolic E, typename... Args>
auto evaluateMatrixPlate(E expr, Args... binders) -> double {
  return evaluateMatrixPlate(expr, BinderPack{binders...});
}

// ============================================================================
// makeIndexedVectorBinding - Factory for creating indexed vector bindings
// ============================================================================
//
// Given an IndexedVectorSymbol and data dimensions, create the appropriate binding.
//
// Usage:
//   auto y_sym = IndexedVectorSymbol<YId, Obs, Dims>{};
//   auto binding = makeIndexedVectorBinding(y_sym, data, n_obs, k_dim);

template <typename Sym>
  requires is_indexed_vector_symbol_v<Sym>
auto makeIndexedVectorBinding(Sym, std::span<const double> data, SizeT n_obs, SizeT k_dim) {
  return IndexedVectorBinding<Sym>{data, n_obs, k_dim};
}

template <typename Sym>
  requires is_indexed_vector_symbol_v<Sym>
auto makeIndexedVectorBinding(Sym, const std::vector<double>& data, SizeT n_obs, SizeT k_dim) {
  return IndexedVectorBinding<Sym>{data, n_obs, k_dim};
}

// ============================================================================
// bindPlateVectors - Alias with clearer semantics
// ============================================================================

template <typename Sym>
  requires is_indexed_vector_symbol_v<Sym>
auto bindPlateVectors(Sym sym, std::span<const double> data, SizeT n_obs, SizeT k_dim) {
  return makeIndexedVectorBinding(sym, data, n_obs, k_dim);
}

template <typename Sym>
  requires is_indexed_vector_symbol_v<Sym>
auto bindPlateVectors(Sym sym, const std::vector<double>& data, SizeT n_obs, SizeT k_dim) {
  return makeIndexedVectorBinding(sym, data, n_obs, k_dim);
}

}  // namespace tempura::symbolic4
