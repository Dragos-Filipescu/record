#ifndef RECORD_INCLUDE_RECORD_FIELD_HPP
#define RECORD_INCLUDE_RECORD_FIELD_HPP

#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <record/detail/utils.hpp>

namespace record::field {

    namespace concepts {

        namespace detail {

            template <typename T>
            concept ValidStoredType = std::is_object_v<T>
                && !std::is_abstract_v<T>
                && std::destructible<T>;

            template <typename T>
            concept Storable = requires {
                typename T::StoredType;
                requires ValidStoredType<typename T::StoredType>;
            };

            template <typename T>
            concept HasAlignment = requires {
                { T::Alignment } -> std::convertible_to<std::size_t>;
            };

            template <typename T>
            concept HasEqualTo = requires {
                typename T::EqualTo;
                requires std::invocable<typename T::EqualTo, const T&, const T&>;
                requires std::convertible_to<
                    std::invoke_result_t<typename T::EqualTo, const T&, const T&>,
                    bool
                >;
            };

            template <typename T>
            concept IsCommonComparisonCategory = std::same_as<T, std::partial_ordering>
                || std::same_as<T, std::weak_ordering>
                || std::same_as<T, std::strong_ordering>;

            template <typename T>
            concept HasCompareThreeWay = requires {
                typename T::CompareThreeWay;
                requires std::invocable<typename T::CompareThreeWay, const T&, const T&>;
                requires IsCommonComparisonCategory<
                    std::invoke_result_t<typename T::CompareThreeWay, const T&, const T&>
                >;
            };
        }

        /**
         * @brief Models a field descriptor with semantic operations.
         *
         * In order for a type <c>T</c> to satisfy the concept <c>Field</c>, it <b>must</b> provide:
         *
         * - <c>T::stored_type</c>: A typedef of the underlying data type.
         * It must be a non-abstract object type, with a non-throwing destructor.
         *
         * The type <c>T</c> <b>may</b> additionally provide:
         *
         * - <c>T::alignment</c>: A static member of type <c>std::size_t</c>.
         * This enforces memory alignment for the field.
         *
         * - <c>T::equals</c>: A functor for equality comparison, <b>and</b>
         *
         * - <c>T::compare_three_way</c>: A functor for three-way comparison.
         *
         * It is intended for users to inherit <c>RecordField</c>, which already
         * provides defaults for all 4, and which may be overridden as desired.
         */
        template<typename T>
        concept Field = detail::Storable<T>;
    }

    namespace detail {

        template<concepts::detail::ValidStoredType T>
        using DefaultEqualTo = std::equal_to<T>;

        template<concepts::detail::ValidStoredType T>
        using DefaultCompareThreeWay = std::compare_three_way;
    }

    /**
     * @brief Helper base class for declaring a Field.
     *
     * @tparam StoredT The concrete type stored by the Field.
     * It must be a non-abstract object type, with a non-throwing destructor.
     * The same StoredT may be shared between different Fields,
     * so long as inheritance is used.
     *
     * @warning This class should be inherited, <b>NOT</b> aliased.
     * Inheritance allows semantic differentiation because two classes/structs
     * with the same template arguments are considered two different Field types.
     * However, two aliases to the same template are considered the same type,
     * <b>NOT</b> two different Field types.
     *
     * <b>Correct usage:</b>
     * @code{.cpp}
     * struct FirstName : RecordField<std::string> {};
     * struct LastName : RecordField<std::string> {};
     *
     * // this will compile:
     * auto x = Record<FirstName, LastName>{};
     * // FirstName and LastName are different types here,
     * // and thus Record will allow both in the parameter pack
     * @endcode
     *
     * <b>Incorrect usage:</b>
     * @code{.cpp}
     * using FirstName = RecordField<std::string>;
     * using LastName = RecordField<std::string>;
     *
     * // this will NOT compile:
     * auto x = Record<FirstName, LastName>{};
     * // FirstName and LastName are the same type here,
     * // and thus Record will NOT allow both in the
     * // parameter pack, even though the user may have
     * // intended for them to have different semantic meaning
     * @endcode
     */
    template<concepts::detail::ValidStoredType StoredT>
    struct RecordField {
        using StoredType = StoredT;
        static constexpr std::size_t Alignment = alignof(StoredT);
        using EqualTo = detail::DefaultEqualTo<StoredT>;
        using CompareThreeWay = detail::DefaultCompareThreeWay<StoredT>;
    };

    namespace detail::traits {

        /**
         * @brief Type traits for a collection of <c>Field</c>s.
         *
         * Contains conjunctions over various STL type traits, such as:
         *
         * - <c>std::is_trivially_copyable</c>
         * - Destruction traits
         * - Default Construction traits
         * - Move/Copy Semantics traits
         * - Equality and Three Way Comparison traits
         *
         * @tparam Fields A type pack, each satisfying the concept <c>Field</c>
         */
        template<concepts::Field... Fields>
        struct FieldTraits final {

        private:

            template<template<typename> class Pred, typename... Args>
            static constexpr bool AllOf = record::detail::utils::predicate_folds::AllOf<Pred, Args...>;

        public:

            /**
             * @brief Member constant which evaluates to <c>true</c>
             * if all fields describe trivially copyable types.
             *
             * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
             * <c>std::is_trivially_copyable<T>::value</c> evaluates to <c>true</c>.
             *
             * Otherwise, it evaluates to <c>false</c>.
             *
             * If <c>true</c>:
             *
             * - The record is effectively trivially copyable, meaning instances
             * can be safely copied using <c>memcpy</c>/<c>memmove</c>, <b>and</b>
             *
             * - The copy and move constructors and assignment operators are trivial, <b>and</b>
             *
             * - The exception specifications of the copy and move
             * constructors and assignment operators are <c>noexcept</c>.
             */
            static constexpr bool kTriviallyCopyable = AllOf<
                std::is_trivially_copyable,
                typename Fields::StoredType...
            >;

            struct DefaultConstruction final {

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe trivially default constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_trivially_default_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise, it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The default constructor participates in overload resolution, <b>and</b>
                 *
                 * - The default constructor is trivial, <b>and</b>
                 *
                 * - The exception specification of the default constructor is <c>noexcept</c>.
                 */
                static constexpr bool kTrivial = AllOf<
                    std::is_trivially_default_constructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe default constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_default_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise, it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The default constructor participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    std::is_default_constructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow default constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_nothrow_default_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise, it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specification of the default constructor is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    std::is_nothrow_default_constructible,
                    typename Fields::StoredType...
                >;
            };

            struct CopyConstruction final {

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe trivially copy constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_trivially_copy_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The copy constructor participates in overload resolution, <b>and</b>
                 *
                 * - The copy constructor is trivial, <b>and</b>
                 *
                 * - The exception specification of the copy constructor is <c>noexcept</c>.
                 */
                static constexpr bool kTrivial = AllOf<
                    std::is_trivially_copy_constructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe copy constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_copy_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The copy constructor participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    std::is_copy_constructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow copy constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_nothrow_copy_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specification of the copy constructor is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    std::is_nothrow_copy_constructible,
                    typename Fields::StoredType...
                >;
            };

            struct CopyAssignment final {

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe trivially copy assignable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_trivially_copy_assignable<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The copy assignment operator participates in overload resolution, <b>and</b>
                 *
                 * - The copy assignment operator is trivial, <b>and</b>
                 *
                 * - The exception specification of the copy assignment operator is <c>noexcept</c>.
                 */
                static constexpr bool kTrivial = AllOf<
                    std::is_trivially_copy_assignable,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe copy assignable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_copy_assignable<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The copy assignment operator participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    std::is_copy_assignable,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow copy assignable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_nothrow_copy_assignable<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specification of the copy assignment operator is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    std::is_nothrow_copy_assignable,
                    typename Fields::StoredType...
                >;
            };

            struct MoveConstruction final {

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe trivially move constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_trivially_move_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The move constructor participates in overload resolution, <b>and</b>
                 *
                 * - The move constructor is trivial, <b>and</b>
                 *
                 * - The exception specification of the move constructor is <c>noexcept</c>.
                 */
                static constexpr bool kTrivial = AllOf<
                    std::is_trivially_move_constructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe move constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_move_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The move constructor participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    std::is_move_constructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow move constructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_nothrow_move_constructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specification of the move constructor is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    std::is_nothrow_move_constructible,
                    typename Fields::StoredType...
                >;
            };

            struct MoveAssignment final {

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe trivially move assignable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_trivially_move_assignable<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The move assignment operator participates in overload resolution, <b>and</b>
                 *
                 * - The move assignment operator is trivial, <b>and</b>
                 *
                 * - The exception specification of the move assignment operator is <c>noexcept</c>.
                 */
                static constexpr bool kTrivial = AllOf<
                    std::is_trivially_move_assignable,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe move assignable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_move_assignable<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The move assignment operator participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    std::is_move_assignable,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow move assignable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_nothrow_move_assignable<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specification of the move assignment operator is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    std::is_nothrow_move_assignable,
                    typename Fields::StoredType...
                >;
            };

            struct Destruction final {

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe trivially destructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_trivially_destructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise, it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The destructor participates in overload resolution, <b>and</b>
                 *
                 * - The destructor is trivial, <b>and</b>
                 *
                 * - The exception specification of the destructor is <c>noexcept</c>.
                 */
                static constexpr bool kTrivial = AllOf<
                    std::is_trivially_destructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe destructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_destructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise, it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The destructor participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    std::is_destructible,
                    typename Fields::StoredType...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow destructible types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::is_nothrow_destructible<T>::value</c> evaluates to <c>true</c>.
                 *
                 * Otherwise, it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specification of the destructor is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    std::is_nothrow_destructible,
                    typename Fields::StoredType...
                >;
            };

            struct Equality final {

#pragma region Predicate Adaptors

            private:

                template<concepts::Field F>
                using Functor = std::conditional_t<
                    concepts::detail::HasEqualTo<F>,
                    typename F::EqualTo,
                    DefaultEqualTo<typename F::StoredType>
                >;

                template<concepts::Field F>
                struct IsInvocable : std::bool_constant<
                    std::invocable<
                        Functor<F>,
                        const typename F::StoredType&,
                        const typename F::StoredType&
                    >
                > {};

                template<concepts::Field F>
                struct IsGoodResult : std::bool_constant<
                    std::convertible_to<
                        std::invoke_result_t<
                            Functor<F>,
                            const typename F::StoredType&,
                            const typename F::StoredType&
                        >,
                        bool
                    >
                > {};

                template<concepts::Field F>
                struct IsNoThrow : std::bool_constant<
                    noexcept(
                        std::declval<Functor<F>>()(
                            std::declval<const typename F::StoredType&>(),
                            std::declval<const typename F::StoredType&>()
                        )
                    )
                > {};

                template<concepts::Field F>
                struct EqComparable : std::conjunction<
                    IsInvocable<F>,
                    IsGoodResult<F>
                > {};

                template<concepts::Field F>
                struct NoThrowEqComparable : std::conjunction<
                    IsInvocable<F>,
                    IsNoThrow<F>
                > {};

            public:

#pragma endregion Predicate Adaptors

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe equality comparable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredTypeF::StoredType</c>, then
                 * <c>std::equality_comparable<T></c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - An ADL helper friend for equality comparison
                 * (<c>equals</c>), participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    EqComparable,
                    Fields...
                >;

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow equality comparable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>noexcept(std::declval<const T&>()
                 * == std::declval<const T&>())</c>
                 * evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specifications of the equality
                 * comparison helper (<c>equals</c>) is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    NoThrowEqComparable,
                    Fields...
                >;
            };

            struct ThreeWayComparison final {

                template<concepts::Field F>
                using Functor = std::conditional_t<
                    concepts::detail::HasCompareThreeWay<F>,
                    typename F::CompareThreeWay,
                    DefaultCompareThreeWay<typename F::StoredType>
                >;

#pragma region Predicate Adaptors

            private:

                template<concepts::Field F>
                struct IsInvocable : std::bool_constant<
                    std::invocable<
                        Functor<F>,
                        const typename F::StoredType&,
                        const typename F::StoredType&
                    >
                > {};

                template<concepts::Field F>
                struct IsGoodResult : std::bool_constant<
                    concepts::detail::IsCommonComparisonCategory<
                        std::invoke_result_t<
                            Functor<F>,
                            const typename F::StoredType&,
                            const typename F::StoredType&
                        >
                    >
                > {};

                template<concepts::Field F>
                struct IsNoThrow : std::bool_constant<
                    noexcept(
                        std::declval<Functor<F>>()(
                            std::declval<const typename F::StoredType&>(),
                            std::declval<const typename F::StoredType&>()
                        )
                    )
                > {};

                template<concepts::Field F>
                struct IsThreeWayComparable : std::conjunction<
                    IsInvocable<F>,
                    IsGoodResult<F>
                > {};

                template<concepts::Field F>
                struct IsNoThrowThreeWayComparable : std::conjunction<
                    IsInvocable<F>,
                    IsNoThrow<F>
                > {};

                template<concepts::Field F>
                using ResultT = std::invoke_result_t<
                    std::conditional_t<
                        concepts::detail::HasCompareThreeWay<F>,
                        typename F::CompareThreeWay,
                        std::compare_three_way
                    >,
                    const typename F::StoredType&,
                    const typename F::StoredType&
                >;

            public:

#pragma endregion Predicate Adaptors

                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe three way comparable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>std::three_way_comparable<T></c> evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - An ADL helper friend for three way comparison
                 * (<c>compare</c>), participates in overload resolution.
                 */
                static constexpr bool kAvailable = AllOf<
                    IsThreeWayComparable,
                    Fields...
                >;


                /**
                 * @brief Member constant which evaluates to <c>true</c>
                 * if all fields describe nothrow three way comparable types.
                 *
                 * That is, for each field <c>F</c>, if <c>T</c> is <c>F::StoredType</c>, then
                 * <c>noexcept(std::declval<const T&>()
                 * <=> std::declval<const T&>())</c>
                 * evaluates to <c>true</c>.
                 *
                 * Otherwise it evaluates to <c>false</c>.
                 *
                 * If <c>true</c>:
                 *
                 * - The exception specifications of the three way
                 * comparison helper (<c>compare</c>) is <c>noexcept</c>.
                 */
                static constexpr bool kNoThrow = AllOf<
                    IsNoThrowThreeWayComparable,
                    Fields...
                >;
                /**
                 * @brief The common comparison category of all fields.
                 *
                 * That is, the weakest category that can represent the result of comparing
                 * any two fields of this type.
                 *
                 * If the pack is empty, defaults to <c>std::strong_ordering</c>.
                 */
                using CommonCompareResultT = std::common_comparison_category_t<
                    std::strong_ordering,
                    ResultT<Fields>...
                >;
            };

#pragma region Field Membership Check

        /**
         * @brief Evaluates to <c>true</c> if <c>F</c> is one of the Fields of this Record type.
         *
         * @tparam F A type satisfying the <c>Field</c> concept.
         *
         * @code{.cpp}
         * static_assert(Record<FieldA, FieldB, FieldC>::Has<FieldA>);
         * static_assert(!Record<FieldA, FieldB, FieldC>::Has<FieldD>);
         * @endcode
         */
        template<concepts::Field F>
        static constexpr bool Has = (... || std::same_as<F, Fields>);

        /**
         * @brief Evaluates to <c>true</c> if any of <c>Fs</c> are Fields of this Record type.
         *
         * @tparam Fs A type pack, each satisfying the <c>Field</c> concept.
         *
         * @code{.cpp}
         * static_assert(Record<FieldA, FieldB, FieldC>::HasAny<FieldA, FieldD, FieldE>);
         * static_assert(!Record<FieldA, FieldB, FieldC>::HasAny<FieldD, FieldE, FieldF>);
         * @endcode
         */
        template<concepts::Field... Fs>
        static constexpr bool HasAny = (... || Has<Fs>);

        /**
         * @brief Evaluates to <c>true</c> if all of <c>Fs</c> are Fields of this Record type.
         *
         * @tparam Fs A type pack, each satisfying the <c>Field</c> concept.
         *
         * @code{.cpp}
         * static_assert(Record<FieldA, FieldB, FieldC>::HasAll<FieldA, FieldB>);
         * static_assert(!Record<FieldA, FieldB, FieldC>::HasAll<FieldA, FieldB, FieldD>);
         * @endcode
         */
        template<concepts::Field... Fs>
        static constexpr bool HasAll = (... && Has<Fs>);

        /**
         * @brief Evaluates to <c>true</c> if none of <c>Fs</c> are Fields of this Record type.
         *
         * @tparam Fs A type pack, each satisfying the <c>Field</c> concept.
         *
         * @code{.cpp}
         * static_assert(Record<FieldA, FieldB, FieldC>::HasNone<FieldD, FieldE, FieldF>);
         * static_assert(!Record<FieldA, FieldB, FieldC>::HasNone<FieldA, FieldD, FieldE>);
         * @endcode
         */
        template<concepts::Field... Fs>
        static constexpr bool HasNone = !HasAny<Fs...>;

#pragma endregion Field Membership Check
        };
    }

    namespace detail::layout {

        template<concepts::Field... Fields>
        struct FieldLayout final {

            [[nodiscard]] static constexpr std::size_t alignUp(
                std::size_t value,
                std::size_t alignment
            ) {
                return (value + alignment - 1) / alignment * alignment;
            }

            template<concepts::Field F>
            static constexpr std::size_t getAlignment() {
                if constexpr (concepts::detail::HasAlignment<F>) {
                    return F::Alignment;
                } else {
                    // ReSharper disable once CppRedundantTypenameKeyword
                    return alignof(typename F::StoredType);
                }
            }

            static consteval auto getLayoutInfo()
            {
                constexpr std::size_t N = sizeof...(Fields);

                if constexpr (N == 0) {
                    return std::tuple{
                        std::array<std::size_t, N>{},
                        std::array<std::size_t, N>{},
                        0ULL,
                        0ULL,
                    };
                }

                std::array<std::size_t, N> offs{};

                std::array<std::size_t, N> alignments{ getAlignment<Fields>()... };
                std::array sizes = { sizeof(typename Fields::StoredType)... };

                // 1. Create a list of indices [0, 1, 2, ...]
                std::array<std::size_t, N> order{};

                for (std::size_t i = 0; i < N; ++i) {
                    order[i] = i;
                }

                // 2. Sort indices by alignment (descending)
                // Use Insertion Sort as it's fast enough for the common case,
                // and easy to reason about.
                auto should_swap = [&](std::size_t a_idx, std::size_t b_idx) {
                    return (
                        std::tie(alignments[a_idx], sizes[a_idx])
                        < std::tie(alignments[b_idx], sizes[b_idx])
                    );
                };

                for (std::size_t i = 1; i < order.size(); ++i) {
                    std::size_t j = i;
                    const auto current = order[i];
                    while (j > 0 && should_swap(order[j - 1], current)) {
                        order[j] = order[j - 1];
                        --j;
                    }
                    order[j] = current;
                }

                // 4. Assign offsets based on sorted order
                std::size_t cursor = 0;
                for (std::size_t idx : order) {
                    const auto align = alignments[idx];
                    const auto size = sizes[idx];

                    cursor = alignUp(cursor, align);
                    offs[idx] = cursor;
                    cursor += size;
                }

                constexpr std::size_t maxAlign = std::max({ std::size_t{1}, Fields::Alignment... });

                return std::tuple{
                    offs,
                    order,
                    cursor,
                    alignUp(cursor, maxAlign),
                };
            }

            static constexpr auto kLayoutInfo = getLayoutInfo();

            static constexpr std::size_t kFieldCount = sizeof...(Fields);

            /**
             * @brief Byte offsets of each field's stored value within the underlying buffer,
             * indexed by declaration order.
             *
             * That is, <c>kOffsets[I]</c> is the byte offset of the <c>I</c>-th field in <c>Fields...</c>.
             * Note that fields are reordered internally to minimise padding, so offsets are
             * not necessarily monotonically increasing.
             */
            static constexpr std::array<std::size_t, kFieldCount> kOffsets = std::get<0>(kLayoutInfo);

            /**
             * @brief The permutation mapping storage positions to declaration indices,
             * indexed by storage position.
             *
             * That is, <c>kStorageOrder[I]</c> is the declaration index of the field occupying
             * the <c>I</c>-th position in storage.
             */
            static constexpr std::array<std::size_t, kFieldCount> kStorageOrder = std::get<1>(kLayoutInfo);

            /**
             * @brief The same permutation as <c>kStorageOrder</c>, but the order of indices is reversed.
             *
             * That is, <c>kReversedStorageOrder[I]</c> is the declaration index of the field occupying
             * the <c>(N - 1 - I)</c>-th position in storage.
             *
             * @note This is strictly different from attempting to use <c>kStorageOrder[N - 1 - I]</c>,
             * as the indices inside it are not guaranteed to be monotonically increasing.
             */
            static constexpr auto kReversedStorageOrder = [] {
                std::array<std::size_t, kFieldCount> arr{};
                std::copy(kStorageOrder.rbegin(), kStorageOrder.rend(), arr.begin());
                return arr;
            }();

            /**
             * @brief The packed size of this Field set, in bytes.
             *
             * That is, the sum of all <c>sizeof(T)</c>, where <c>T</c> is <c>F::StoredType</c>
             * and <c>F</c> is each of the <c>Fields...</c>, including internal padding between
             * <c>Ts</c>, but excluding tail padding.
             *
             * This is the size that should be used when tail padding is irrelevant,
             * such as writing to a network buffer.
             */
            static constexpr std::size_t kPackedSize = std::get<2>(kLayoutInfo);

            /**
             * @brief The aligned size of this Field set, in bytes, including tail padding.
             *
             * That is, the sum of all <c>sizeof(T)</c>, where <c>T</c> is <c>F::StoredType</c>
             * and <c>F</c> is each of the <c>Fields...</c>, including internal padding between
             * <c>T</c>s, as well as tail padding.
             *
             * This is the size that should be used when embedding Records in arrays
             * or other contiguous structures.
             */
            static constexpr std::size_t kAlignedSize = std::get<3>(kLayoutInfo);
        };
    }
}

#endif  // !RECORD_INCLUDE_RECORD_FIELD_HPP
