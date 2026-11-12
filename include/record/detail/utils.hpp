#ifndef RECORD_INCLUDE_RECORD_DETAIL_UTILS_HPP
#define RECORD_INCLUDE_RECORD_DETAIL_UTILS_HPP

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace record::detail::utils {

    namespace concepts {

        template<typename T>
        concept ValuePredicate = requires {
            { T::value } -> std::convertible_to<bool>;
        };
    }

    namespace string {

        template<std::size_t N>
        struct FixedString final {

            char mData[N]{};

            explicit constexpr FixedString(const char (&str)[N]) noexcept {
                for (std::size_t i = 0; i < N; ++i) {
                    mData[i] = str[i];
                }
            }

            [[nodiscard]] constexpr std::size_t size() const noexcept { return N - 1; }
            [[nodiscard]] constexpr const char* data() const noexcept { return mData; }
        };

        namespace detail {

            template<std::size_t... Ns>
            constexpr std::size_t ConcatN = ((Ns - 1) + ...) + 1;

            template<std::uintmax_t V>
            constexpr std::size_t DigitCount = [] {
                std::size_t n = 1;
                for (std::uintmax_t v = V; v >= 10; v /= 10) {
                    ++n;
                }
                return n;
            }();
        }

        template<std::size_t... Ns>
        [[nodiscard]] constexpr FixedString<detail::ConcatN<Ns...>> concat(
            const FixedString<Ns>&... parts
        ) noexcept
        {
            char out[detail::ConcatN<Ns...>]{};
            std::size_t pos = 0;
            (
                ...,
                (
                    std::copy_n(parts.data(), parts.size(), out + pos),
                    pos += parts.size()
                )
            );
            return FixedString<detail::ConcatN<Ns...>>{out};
        }

        template<std::integral auto V>
        constexpr auto StringifyInt = [] {
            constexpr std::size_t N = detail::DigitCount<static_cast<std::uintmax_t>(V)>;
            char out[N + 1]{};
            auto v = static_cast<std::uintmax_t>(V);
            for (std::size_t i = 0; i < N; ++i) {
                out[N - i - 1] = static_cast<char>('0' + v % 10);
                v /= 10;
            }
            return FixedString{out};
        }();
    }

    namespace predicate_folds {

        template<template<typename> typename Pred, typename... Args>
            requires(... && concepts::ValuePredicate<Pred<Args>>)
        constexpr bool AllOf = (... && Pred<Args>::value);
    }

    namespace utils {

        template<template<typename, typename> typename Pred, typename LeftArg>
        struct BindLeft final {
            template<typename RightArg>
            using Func = Pred<LeftArg, RightArg>;
        };

        template<template<typename, typename> typename Pred, typename RightArg>
        struct BindRight final {
            template<typename LeftArg>
            using Func = Pred<LeftArg, RightArg>;
        };

        template<typename T, typename... Ts>
        constexpr bool IsAnyOf = (... || std::is_same_v<T, Ts>);

#pragma region Type Pack Lookup

        namespace detail {

            template<template<typename> typename Pred, typename... Ts>
                requires(... && concepts::ValuePredicate<Pred<Ts>>)
            struct FindIfImpl final {
                template<std::size_t... Is>
                constexpr std::size_t operator()(std::index_sequence<Is...>) {
                    std::size_t result = sizeof...(Ts);
                    bool found = false;
                    (..., (Pred<Ts>::value && !found ? (result = Is, found = true) : 0));
                    return result;
                }
            };
        }

        template<typename T, typename... Ts>
        constexpr std::size_t Find = detail::FindIfImpl<
            BindLeft<std::is_same, T>::template Func, Ts...
        >{}(std::index_sequence_for<Ts...>{});

        template<template<typename> typename Pred, typename... Ts>
            requires(... && concepts::ValuePredicate<Pred<Ts>>)
        constexpr std::size_t FindIf = detail::FindIfImpl<
            Pred, Ts...
        >{}(std::index_sequence_for<Ts...>{});

#pragma endregion Type Pack Lookup

#pragma region AllUnique

        namespace detail {

            template<typename...>
            struct AllUniqueImpl;

            template<>
            struct AllUniqueImpl<> : std::true_type {};

            template<typename T, typename... Ts>
            struct AllUniqueImpl<T, Ts...> : std::bool_constant<
                !IsAnyOf<T, Ts...>
                && AllUniqueImpl<Ts...>::value
            > {};
        }

        template<typename... Ts>
        constexpr bool AllUnique = detail::AllUniqueImpl<Ts...>::value;

#pragma endregion AllUnique

        namespace detail {

            template<typename T>
            concept HasTupleSize = requires {
                { std::tuple_size_v<T> } -> std::convertible_to<std::size_t>;
            };

            template<typename T, typename... Ts>
            concept SameSize = HasTupleSize<T> && std::tuple_size_v<T> == sizeof...(Ts);

            template<auto Order, typename... Ts>
            concept ValidIndices = []<std::size_t... Is>(std::index_sequence<Is...>) {
                return ((std::get<Is>(Order) < sizeof...(Ts)) && ...);
            }(std::make_index_sequence<std::tuple_size_v<decltype(Order)>>{});

            template<typename... Ts>
            struct Pack final {

                template<template<typename...> typename Target>
                using Rebind = Target<Ts...>;
            };
        }

        template<auto Order, typename... Ts>
            requires(
                detail::SameSize<decltype(Order), Ts...>
                && detail::ValidIndices<Order, Ts...>
            )
        struct Reorder final {
            // We use an index_sequence just to expand the array indices [0, 1, 2...]
            template<std::size_t... Is>
            static auto makeType(std::index_sequence<Is...>) {
                // std::get<Is>(Order) returns Order[Is] at compile time
                return detail::Pack<std::tuple_element_t<std::get<Is>(Order), std::tuple<Ts...>>...>{};
            }

            using type = decltype(makeType(std::make_index_sequence<sizeof...(Ts)>{}));

            template<template<typename...> typename Target>
            using Rebind = type::template Rebind<Target>;
        };
    }

    namespace set {

        template<typename... Ts>
        struct Set final {
            static_assert(
                utils::AllUnique<Ts...>,
                "[ ERR | record::detail::utils::utils::detail::Set] elements of a Set must be unique"
            );
        };

        namespace detail {

            template<typename T, typename S>
            constexpr bool Contains = false;

            template<typename T, typename... Ts>
            constexpr bool Contains<T, Set<Ts...>> = utils::IsAnyOf<T, Ts...>;

            template<typename S1, typename S2>
            constexpr bool Subset = false;

            template<typename... As, typename... Bs>
            constexpr bool Subset<Set<As...>, Set<Bs...>> = (... && Contains<As, Set<Bs...>>);
        }

        template<typename S1, typename S2>
        constexpr bool SetEqual = false;

        template<typename... As, typename... Bs>
        constexpr bool SetEqual<Set<As...>, Set<Bs...>> = (
            sizeof...(As) == sizeof...(Bs)
            && detail::Subset<Set<As...>, Set<Bs...>>
        );
    }
}

#endif //RECORD_INCLUDE_RECORD_DETAIL_UTILS_HPP
