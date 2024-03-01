#include <cstddef>
#include <utility>
namespace common {

template <std::size_t I, typename T>
struct indexed {
    using type = T;
};

template <typename Is, typename... Ts>
struct indexer;

template <std::size_t... Is, typename... Ts>
struct indexer<std::index_sequence<Is...>, Ts...> : indexed<Is, Ts>... {};

template <std::size_t I, typename T>
static indexed<I, T> select(indexed<I, T>);

// get the index of specified parameter pack
// FIXME: constant index only
template <std::size_t I, typename... Ts>
using nth_element = typename decltype(select<I>(
    indexer<std::index_sequence_for<Ts...>, Ts...>{}))::type;

} // namespace common
