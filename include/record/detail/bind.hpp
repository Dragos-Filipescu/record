#ifndef RECORD_INCLUDE_RECORD_DETAIL_BIND_HPP
#define RECORD_INCLUDE_RECORD_DETAIL_BIND_HPP

#include <concepts>
#include <tuple>
#include <utility>

#include <record/field.hpp>


namespace record {

    /// @brief Controls argument storage in the intermediate Binding.
    enum class BindStrategy {
        Copy,    ///< Copies the arguments within the intermediate Binding type.
        Forward, ///< Preserves reference semantics. @warning: Does <b>NOT</b> extend reference lifetimes. Exercise caution.
    };

    namespace detail {

        template<typename F, typename... Args>
        concept ArgsFor = field::concepts::Field<F>
            && std::constructible_from<
                typename F::StoredType,
                Args...
            >;

        template<typename F, typename... Args>
        concept NoThrowArgsFor = field::concepts::Field<F>
            && std::is_nothrow_constructible_v<
                typename F::StoredType,
                Args...
            >;

        template<
            BindStrategy Strategy,
            field::concepts::Field F,
            typename... Args
        >
            requires(ArgsFor<F, Args...>)
        struct FieldBindingT final {
            using FieldT = F;
            static constexpr bool kNoThrow = NoThrowArgsFor<F, Args...>;

            std::tuple<
                std::conditional_t<
                    Strategy == BindStrategy::Copy,
                    // Use std::decay_t instead os std::remove_cvref_t here, because tuples
                    // can't store arrays. For example, with Bind<F>("hello"), Args = const
                    // char(&)[6], which becomes const char[6] under std::remove_cvref_t,
                    // but std::tuple cannot store arrays.
                    //
                    // std::tuple cannot construct an array member from a forwarded array
                    // reference (arrays are not copy-initializable), so this branch would
                    // be ill-formed.
                    std::decay_t<Args>,
                    Args
                >...
            > args;

            explicit constexpr FieldBindingT(Args&&... a) noexcept(kNoThrow)
                : args(std::forward<Args>(a)...) {}
        };

        template<typename>
        constexpr bool FieldBindingSpec = false;

        template<BindStrategy S, field::concepts::Field F, typename... Args>
        constexpr bool FieldBindingSpec<FieldBindingT<S, F, Args...>> = true;

        template<typename T>
        concept FieldBinding = FieldBindingSpec<T>;

        template<typename F, typename B>
        concept BindFor = field::concepts::Field<F>
            && FieldBinding<B>
            && std::same_as<typename B::FieldT, F>;

        template<typename F, typename B>
        concept NoThrowBindFor = BindFor<F, B>
            && B::kNoThrow;
    }

    namespace detail::contracts {

        template<BindStrategy Strategy, field::concepts::Field F, typename... Args>
        class BindContract final {

            static constexpr bool kValidBinds = ArgsFor<F, Args...>;

        public:

            static constexpr bool kIsValid = kValidBinds;

            static constexpr bool kIsNoexcept = std::is_nothrow_constructible_v<
                typename F::StoredType,
                Args...
            >;

            static constexpr void diagnose() noexcept {
                static_assert(
                    kValidBinds,
                    "[record::Bind] The field must be constructible from the given args."
                );
            }
        };
    }

#pragma region Bind Helper

    /**
     * @brief Creates a field binding for use in <c>Record</c>'s parameterised constructor.
     *
     * Captures <c>args...</c> by value or forwards them for later in-place construction
     * of <c>F::StoredType</c>. Binds are order agnostic and dispatched by field type.
     *
     * @tparam Strategy Copy (default) or Forward. See <c>BindStrategy</c>.
     * @tparam F A type satisfying the concept <c>Field</c>.
     * @tparam Args Constructor argument types for <c>F::StoredType</c>.
     *
     * Example:
     *
     * @code{.cpp}
     * auto r = Record<FieldA, FieldB>{ Bind<FieldB>(2), Bind<FieldA>(1) };
     * @endcode
     */
    template<BindStrategy Strategy, field::concepts::Field F, typename... Args>
    [[nodiscard]] constexpr auto Bind(Args&&... args)
        noexcept(detail::contracts::BindContract<Strategy, F, Args...>::kIsNoexcept)
        requires(detail::contracts::BindContract<Strategy, F, Args...>::kIsValid)
    {
        return detail::FieldBindingT<Strategy, F, Args...>{ std::forward<Args>(args)... };
    }

    /// @brief Trap overload of Bind<BindStrategy Strategy, Field F, typename... Args>
    template<BindStrategy Strategy, field::concepts::Field F, typename... Args>
    constexpr void Bind(Args&&...)
        noexcept
        requires(!detail::contracts::BindContract<Strategy, F, Args...>::kIsValid)
    {
        detail::contracts::BindContract<Strategy, F, Args...>::diagnose();
    }

    /**
     * @brief Creates a field binding for use in <c>Record</c>'s parameterised constructor.
     *
     * Captures <c>args...</c> by value for later in-place construction of <c>F::StoredType</c>.
     * Binds are order agnostic and dispatched by field type.
     *
     * @tparam F A type satisfying the concept <c>Field</c>.
     * @tparam Args Constructor argument types for <c>F::StoredType</c>.
     *
     * Example:
     *
     * @code{.cpp}
     * auto r = Record<FieldA, FieldB>{ Bind<FieldB>(2), Bind<FieldA>(1) };
     * @endcode
     */
    template<field::concepts::Field F, typename... Args>
    [[nodiscard]] constexpr auto Bind(Args&&... args)
        noexcept(detail::contracts::BindContract<BindStrategy::Copy, F, Args...>::kIsNoexcept)
        requires(detail::contracts::BindContract<BindStrategy::Copy, F, Args...>::kIsValid)
    {
        return detail::FieldBindingT<BindStrategy::Copy, F, Args...>{ std::forward<Args>(args)... };
    }

    /// @brief Trap overload of Bind<Field F, typename... Args>
    template<field::concepts::Field F, typename... Args>
    constexpr void Bind(Args&&...)
        noexcept
        requires(!detail::contracts::BindContract<BindStrategy::Copy, F, Args...>::kIsValid)
    {
        detail::contracts::BindContract<BindStrategy::Copy, F, Args...>::diagnose();
    }

#pragma endregion Bind Helper
}

#endif //RECORD_INCLUDE_RECORD_DETAIL_BIND_HPP
