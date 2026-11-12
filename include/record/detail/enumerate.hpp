#ifndef RECORD_INCLUDE_RECORD_DETAIL_ENUMERATE_HPP
#define RECORD_INCLUDE_RECORD_DETAIL_ENUMERATE_HPP

#include <cstddef>
#include <tuple>
#include <utility>

namespace record::detail::enumerate {

    template<typename... Ts>
    class Enumerate final {

        enum class FoldEval {
            Left,  ///< Processes from the first element to the last (left-associative).
            Right, ///< Processes from the last element to the first (right-associative).
        };

        static constexpr std::size_t N = sizeof...(Ts);

        template<
            typename Func,
            typename... Args
        >
        static constexpr void applyImpl(Func&& func, Args&&... args)
        {
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (
                    ...,
                    [&]<std::size_t I>() {
                        using T = std::tuple_element_t<I, std::tuple<Ts...>>;
                        func.template operator()<I, T>(std::forward<Args>(args)...);
                    }.template operator()<Is>()
                );
            }(std::make_index_sequence<N>{});
        }

        template<
            typename Func,
            typename... Args
        >
        static constexpr bool allOfImpl(Func&& func, Args&&... args)
        {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return (
                    ... &&
                    [&]<std::size_t I>() {
                        using U = std::tuple_element_t<I, std::tuple<Ts...>>;
                        return func.template operator()<I, U>(std::forward<Args>(args)...);
                    }.template operator()<Is>()
                );
            }(std::make_index_sequence<N>{});
        }

        template<
            typename Func,
            typename... Args
        >
        static constexpr bool anyOfImpl(Func&& func, Args&&... args)
        {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return (
                    ... ||
                    [&]<std::size_t I>() {
                        using U = std::tuple_element_t<I, std::tuple<Ts...>>;
                        return func.template operator()<I, U>(std::forward<Args>(args)...);
                    }.template operator()<Is>()
                );
            }(std::make_index_sequence<N>{});
        }

    public:

        template<typename Func, typename... Args>
        static constexpr void apply(Func&& func, Args&&... args)
        {
            applyImpl(
                std::forward<Func>(func),
                std::forward<Args>(args)...
            );
        }

        template<typename Func, typename... Args>
        static constexpr bool allOf(Func&& func, Args&&... args)
        {
            return allOfImpl(
                std::forward<Func>(func),
                std::forward<Args>(args)...
            );
        }

        template<typename Func, typename... Args>
        static constexpr bool anyOf(Func&& func, Args&&... args)
        {
            return anyOfImpl(
                std::forward<Func>(func),
                std::forward<Args>(args)...
            );
        }

        template<typename Func, typename... Args>
        static constexpr bool noneOf(Func&& func, Args&&... args)
        {
            return !anyOf(
                std::forward<Func>(func),
                std::forward<Args>(args)...
            );
        }
    };
}

#endif //RECORD_INCLUDE_RECORD_DETAIL_ENUMERATE_HPP
