#include "meta/containers.h"
#include "meta/type_id.h"
#include "meta/utility.h"

namespace tempura {

template <SizeT N, typename... Args>
using TypeAt = TypeOf<(MinimalArray{kMeta<Args>...}[N])>;

template <SizeT N, typename T>
struct TupleLeaf {
  using ValueType = T;

  ValueType value;
};

template <typename Sequence, typename... Ts>
class TupleImpl;

template <SizeT... Is, typename... Ts>
class TupleImpl<IndexSequence<Is...>, Ts...> : public TupleLeaf<Is, Ts>... {
 public:
  template <typename... Args>
  constexpr TupleImpl(Args&&... args)
      : TupleLeaf<Is, Ts>{static_cast<Args&&>(args)}... {}

  template <SizeT I>
  constexpr auto get() -> TypeAt<I, Ts...>& {
    return static_cast<TupleLeaf<I, TypeAt<I, Ts...>>*>(this)->value;
  }

  template <SizeT I>
  constexpr auto get() const -> const TypeAt<I, Ts...>& {
    return static_cast<const TupleLeaf<I, TypeAt<I, Ts...>>*>(this)->value;
  }
};

template <typename... Ts>
class Tuple : public TupleImpl<MakeIndexSequence<sizeof...(Ts)>, Ts...> {
 public:
  using Base = TupleImpl<MakeIndexSequence<sizeof...(Ts)>, Ts...>;

  template <typename... Args>
  constexpr Tuple(Args&&... args) : Base{static_cast<Args&&>(args)...} {}

  template <SizeT I>
  constexpr auto get() -> TypeAt<I, Ts...>& {
    return Base::template get<I>();
  }

  template <SizeT I>
  constexpr auto get() const -> const TypeAt<I, Ts...>& {
    return Base::template get<I>();
  }
};

template <typename... Ts, typename... Us>
auto operator==(const Tuple<Ts...>& lhs, const Tuple<Us...>& rhs) -> bool {
  return ((lhs.template get<Ts>() == rhs.template get<Us>()) && ...);
}

// TODO Implement std::decay
template <typename... Ts>
Tuple(Ts...) -> Tuple<Ts...>;

template <SizeT N>
auto get(auto&& tup) {
  return static_cast<decltype(tup)>(tup).template get<N>();
}

template <typename Func, typename... Args>
constexpr auto apply(Func&& func, const Tuple<Args...>& tup) {
  return [&]<SizeT... Is>(IndexSequence<Is...>) {
    return static_cast<Func&&>(func)(tup.template get<Is>()...);
  }(MakeIndexSequence<sizeof...(Args)>());
}

template <typename Func, typename... Args>
constexpr auto apply(Func&& func, Tuple<Args...>&& tup) {
  return [&]<SizeT... Is>(IndexSequence<Is...>) {
    return static_cast<Func&&>(func)(
        static_cast<Args&&>(tup.template get<Is>())...);
  }(MakeIndexSequence<sizeof...(Args)>());
}

}  // namespace tempura
