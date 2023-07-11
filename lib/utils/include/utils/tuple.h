#ifndef _FLEXFLOW_UTILS_TUPLE_H
#define _FLEXFLOW_UTILS_TUPLE_H

#include "utils/any.h"
#include "utils/exception.h"
#include <cstddef>
#include <tuple>
#include <type_traits>

// Adapted from
// https://github.com/bitwizeshift/BackportCpp/blob/4f33a7f9b219f169e60d8ed2fd5731a3a23288e4/include/bpstd/tuple.hpp

namespace FlexFlow {

namespace TupleUtils {

template <typename T, std::size_t Index, typename... Types>
struct index_of_impl;

template <typename T, std::size_t Index, typename Type0, typename... Types>
struct index_of_impl<T, Index, Type0, Types...>
    : index_of_impl<T, Index + 1, Types...> {};

template <typename T, std::size_t Index, typename... Types>
struct index_of_impl<T, Index, T, Types...>
    : std::integral_constant<std::size_t, Index> {};

template <typename T, typename... Types>
struct index_of : index_of_impl<T, 0, Types...> {};

} // namespace TupleUtils

template <typename T, typename... Types>
T &get(std::tuple<Types...> &t) noexcept {
  return std::get<TupleUtils::index_of<T, Types...>::value>(t);
}

template <typename T, typename... Types>
T &&get(std::tuple<Types...> &&t) noexcept {
  return move(std::get<TupleUtils::index_of<T, Types...>::value>(t));
}

template <typename T, typename... Types>
T const &get(std::tuple<Types...> const &t) noexcept {
  return std::get<TupleUtils::index_of<T, Types...>::value>(t);
}

template <typename T, typename... Types>
T const &&get(std::tuple<Types...> const &&t) noexcept {
  return move(std::get<TupleUtils::index_of<T, Types...>::value>(t));
}

template <int Idx, typename Visitor, typename... Types>
void visit_tuple_impl(Visitor &v, std::tuple<Types...> const &tup) {
  v(Idx, std::get<Idx>(tup));
  if (Idx >= std::tuple_size<decltype(tup)>::value) {
    return;
  } else {
    visit_tuple_impl<(Idx + 1)>(v, tup);
  }
}

template <typename Visitor, typename... Types>
void visit_tuple(Visitor &v, std::tuple<Types...> const &tup) {
  visit_tuple_impl<0>(v, tup);
}

struct tuple_get_visitor {
  tuple_get_visitor() = delete;
  tuple_get_visitor(int requested_idx, any &result)
      : requested_idx(requested_idx), result(result) {}

  int requested_idx;
  any &result;

  template <typename T>
  void operator()(int idx, T const &t) {
    if (idx == requested_idx) {
      result = t;
    }
  }
};

template <typename... Types>
any get(std::tuple<Types...> const &t, int idx) {
  size_t tuple_size = std::tuple_size<decltype(t)>::value;
  if (idx < 0 || idx >= tuple_size) {
    throw mk_runtime_error(
        "Error: idx {} out of bounds for tuple of size {}", idx, tuple_size);
  }
  any result;
  visit_tuple(t, tuple_get_visitor{idx, result});
  return result;
}

template <typename T, typename Tup>
struct tuple_prepend_type;

template <typename T, typename... Args>
struct tuple_prepend_type<T, std::tuple<Args...>> {
  using type = std::tuple<T, Args...>;
};

template <typename T, typename Tup>
using tuple_prepend_type_t = typename tuple_prepend_type<T, Tup>::type;

template <typename T, typename... Args>
auto tuple_prepend(T const &t, std::tuple<Args...> const &tup)
    -> std::tuple<T, Args...> {
  return std::tuple_cat(std::make_tuple(t), tup);
}

template <typename T, typename Tup>
struct lazy_tuple_prepend {
  using type =
      typename tuple_prepend_type<typename T::type, typename Tup::type>::type;
};

template <int IDX, typename T>
struct normalize_idx
    : std::integral_constant<int,
                             ((IDX < 0) ? (std::tuple_size<T>::value + IDX)
                                        : IDX)> {};

template <int start, int end, int cur, typename T>
struct tuple_slice_impl
    : conditional_t<
          (cur < start),
          tuple_slice_impl<start, end, (cur + 1), T>,
          conditional_t<
              (cur < end),
              lazy_tuple_prepend<std::tuple_element<cur, T>,
                                 tuple_slice_impl<start, end, (cur + 1), T>>,
              type_identity<std::tuple<>>>> {};

template <int start, int end, typename T>
using tuple_slice_t = typename tuple_slice_impl<normalize_idx<start, T>::value,
                                                normalize_idx<end, T>::value,
                                                0,
                                                T>::type;

template <int start, typename T>
using tuple_tail_t = tuple_slice_t<start, std::tuple_size<T>::value, T>;

template <int end, typename T>
using tuple_head_t = tuple_slice_t<0, end, T>;

/* DEBUG_PRINT_TYPE(tuple_tail_t<0, std::tuple<int>>); */

static_assert(
    std::is_same<tuple_tail_t<1, std::tuple<int>>, std::tuple<>>::value, "");

} // namespace FlexFlow

#endif