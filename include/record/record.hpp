#ifndef RECORD_INCLUDE_RECORD_RECORD_HPP
#define RECORD_INCLUDE_RECORD_RECORD_HPP

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <record/field.hpp>
#include <record/detail/bind.hpp>
#include <record/detail/contracts.hpp>
#include <record/detail/utils.hpp>

#include "detail/enumerate.hpp"

#ifdef __cpp_deleted_function
    #define DELETE(msg) delete(msg)
#else
    #define DELETE(msg) delete
#endif

namespace record {

    /**
     * @brief A heterogeneous record container with compile-time layout optimization.
     *
     * Fields are reordered by alignment (descending) to minimize internal padding.
     * The class maintains a distinction between Declaration Order (logical) and
     * Storage Order (physical).
     *
     * Initialization is performed via order-agnostic type-safe bindings.
     *
     * Multiple Fields may share the same <c>storage_type</c> but the
     * field types themselves must be unique to avoid ambiguity.
     *
     * @tparam Fields A pack of unique types.
     *
     * Example:
     * @code{.cpp}
     * struct Name {
     *  using StoredType = std::string;
     *  static constexpr size_t Alignment = alignof(std::string);
     * };
     * struct Age  {
     *  using StoredType = int;
     *  static constexpr size_t Alignment = alignof(float);
     * };
     * // Fields are bound by type, making the constructor order-agnostic
     * Record<Name, Age> user{
     *  Bind<Age>(25),
     *  Bind<Name>("Alice")
     * };
     * auto& name = get<Name>(user);
     * auto&& [name, age] = user;
     */
    template<field::concepts::Field... Fields>
        requires(detail::utils::utils::AllUnique<Fields...>)
    class Record final {

        /// @brief Layout info used internally by Record.
        using Layout = field::detail::layout::FieldLayout<Fields...>;

        /// @brief Special function contract info used internally by Record.
        using Contracts = detail::contracts::RecordContracts<Fields...>;

        /// @brief Friend helper contract info used by ADL helpers.
        using HelperContracts = detail::contracts::HelperContracts<Fields...>;

        template<field::concepts::Field F, detail::FieldBinding Bind>
        struct IsBindFor final : std::bool_constant<
            detail::BindFor<F, std::remove_cvref_t<Bind>>
        > {};

        /// @brief Helper used internally by Record for binding dispatch.
        template<field::concepts::Field F, detail::FieldBinding... Binds>
        static constexpr std::size_t kFindBindIdx =
            detail::utils::utils::FindIf<
                detail::utils::utils::BindLeft<IsBindFor, F>::template Func,
                Binds...
            >;

    public:

        /**
         * @brief Rebinds the <c>Fields...</c> pack of this Record to another template.
         *
         * @tparam Target A template which accepts the same
         * number of type arguments as there are Fields.
         *
         * Provides two aliases for rebinding the <c>Fields...</c> pack:
         * - <c>AsDeclared</c>: Keeps the Fields in their original declaration order.
         * - <c>AsStored</c>: Reorders the Fields to match their internal storage layout.
         * - <c>AsStoredReversed</c>: Reorders the Fields to match their internal storage layout,
         * in reverse order.
         *
         * @warning The order of <c>Fields...</c> for <c>AsStored</c> will, in most cases,
         * <b>NOT</b> match the order of <c>Fields...</c> in the Record. This is because
         * Record may reorder its Fields in the internal storage to minimise its memory
         * footprint. Use <c>AsDeclared</c> when the declaration order is desired.
         *
         * @code{.cpp}
         * struct Name : RecordField<std::string> {};
         * struct Age : RecordField<int> {};
         * struct Height : RecordField<double> {};
         *
         * using t1 = Record<Name, Age, Height>::Rebind<std::tuple>::AsDeclared;
         * // t1 is std::tuple<Name, Age, Height>
         *
         * using t2 = Record<Name, Age, Height>::Rebind<std::tuple>::AsStored;
         * // assuming the internal ordering is Name > Height > Age,
         * // t2 is std::tuple<Name, Height, Age>
         * @endcode
         */
        template<template<typename...> typename Target>
        struct Rebind final {
            /// @brief The target template instantiated with the Fields in declaration order.
            using AsDeclared = Target<Fields...>;

            /// @brief The target template instantiated with the Fields in storage order.
            using AsStored = detail::utils::utils::Reorder<
                Layout::kStorageOrder,
                Fields...
            >::template Rebind<Target>;

            /// @brief The target template instantiated with the Fields in reverse storage order.
            using AsStoredReversed = detail::utils::utils::Reorder<
                Layout::kReversedStorageOrder,
                Fields...
            >::template Rebind<Target>;
        };

        /// @brief Type-trait queries for the Fields of this Record.
        using Traits = field::detail::traits::FieldTraits<Fields...>;

        /// @brief The number of fields in this Record.
        static constexpr std::size_t kFieldCount = sizeof...(Fields);

    private:

        /// @brief Helper used internally by Record for obtaining storage addresses of Fields.
        template<field::concepts::Field F>
            requires(kFieldCount > 0 && Traits::template Has<F>)
        [[nodiscard]] auto* getLoc() noexcept {
            using T = F::StoredType;
            constexpr std::size_t I = detail::utils::utils::Find<F, Fields...>;
            // We use static_cast here because reinterpret_cast
            // doesn't work in a constant expression.
            //
            // And we use byte* -> void* -> T* cast here because the standard
            // is a pedantic bitch that only allows static_cast between void* and T*,
            // not byte* and T*, so we must use void* as intermediary when accessing.
            return static_cast<T*>(static_cast<void*>(mData.data() + Layout::kOffsets[I]));
        }

        /// @brief Helper used internally by Record for obtaining storage addresses of Fields. Const version
        template<field::concepts::Field F>
            requires(kFieldCount > 0 && Traits::template Has<F>)
        [[nodiscard]] const auto* getLoc() const noexcept {
            using T = F::StoredType;
            constexpr std::size_t I = detail::utils::utils::Find<F, Fields...>;
            // We use static_cast here because reinterpret_cast
            // doesn't work in a constant expression.
            //
            // And we use byte* -> void* -> T* cast here because the standard
            // is a pedantic bitch that only allows static_cast between void* and T*,
            // not byte* and T*, so we must use void* as intermediary when accessing.
            return static_cast<const T*>(static_cast<const void*>(mData.data() + Layout::kOffsets[I]));
        }

    public:

#pragma region Special Member Functions

#pragma region Default & Parameterised Constructors

        Record()
            noexcept
            requires(Contracts::DefaultConstructor::kIsTrivial)
        = default;

        Record()
            noexcept(Contracts::DefaultConstructor::kIsNoexcept)
            requires(
                Contracts::DefaultConstructor::kIsValid
                && !Contracts::DefaultConstructor::kIsTrivial
            )
            : mData{}
        {
            using Enumerator = Rebind<detail::enumerate::Enumerate>::AsStored;
            Enumerator::apply(
                [&]<std::size_t I, field::concepts::Field F>() mutable {
                    std::construct_at(getLoc<F>());
                }
            );
        }

        Record()
            noexcept
            requires(!Contracts::DefaultConstructor::kIsValid)
        {
            Contracts::DefaultConstructor::diagnose();
            std::unreachable();
        }

        /**
         * @brief Constructs a <c>Record</c> from a set of <c>Field</c> <c>Bind</c>ings.
         *
         * Each <c>Field</c> <c>F</c> in <c>Fields...</c>
         * must be bound exactly once via <c>Bind<F>(args...)</c>.
         * <c>Bind</c>ings may be supplied in any order. Each <c>Field</c>'s stored
         * value is constructed in-place from the arguments captured in its binding.
         *
         * @tparam Binds A pack of <c>FieldBinding</c> types, one per <c>Field</c>, in any order.
         * Such types are an implementation detail and not to be instantiated directly, but rather
         * via calling <c>record::Bind</c> and passing the respective arguments for the <c>Field</c>.
         *
         * @code{.cpp}
         * auto r = Record<FieldA, FieldB>{ Bind<FieldB>(2), Bind<FieldA>(1) };
         * @endcode
         */
        template<detail::FieldBinding... Binds>
        explicit Record(Binds&&... binds)
            noexcept(Contracts::template BindConstructor<Binds...>::kIsNoexcept)
            requires(Contracts::template BindConstructor<Binds...>::kIsValid)
        {
            auto allBinds = std::forward_as_tuple(std::forward<Binds>(binds)...);

            using Enumerator = Rebind<detail::enumerate::Enumerate>::AsStored;
            Enumerator::apply(
                [&]<std::size_t I, field::concepts::Field F>() mutable {
                    // Locate the Binding supplied for Field F
                    constexpr std::size_t targetBindIdx = kFindBindIdx<F, std::remove_cvref_t<Binds>...>;
                    static_assert(targetBindIdx < sizeof...(Binds));

                    // Construct F::StoredType in-place at its offset within the buffer
                    auto &&targetBind = std::get<targetBindIdx>(allBinds);
                    std::apply(
                        [&]<typename... Args>(Args&&...args) {
                            std::construct_at(getLoc<F>(), std::forward<Args>(args)...);
                        },
                        std::forward<decltype(targetBind.args)>(targetBind.args)
                    );
                }
            );
        }

        template<detail::FieldBinding... Binds>
        explicit Record(Binds&&...)
            noexcept
            requires(!Contracts::template BindConstructor<Binds...>::kIsValid)
        {
            Contracts::template BindConstructor<Binds...>::diagnose();
            std::unreachable();
        }

#pragma endregion Default & Parameterised Constructors


#pragma region Copy/Move Constructors & Assignment Operators

#pragma region Copy Constructor

        Record(const Record&)
            noexcept
            requires(Contracts::CopyConstructor::kIsTrivial)
        = default;

        Record(const Record& other)
            noexcept(Contracts::CopyConstructor::kIsNoexcept)
            requires(
                Contracts::CopyConstructor::kIsValid
                && !Contracts::CopyConstructor::kIsTrivial
            )
        {
            using Enumerator = Rebind<detail::enumerate::Enumerate>::AsStored;
            Enumerator::apply(
                [&]<std::size_t I, field::concepts::Field F>() mutable {
                    std::construct_at(getLoc<F>(), get<F>(other));
                }
            );
        }

        Record(const Record&)
            noexcept
            requires(!Contracts::CopyConstructor::kIsValid)
        {
            Contracts::CopyConstructor::diagnose();
            std::unreachable();
        }

#pragma endregion Copy Constructor

#pragma region Copy Assignment Operator

        Record& operator=(const Record&)
            noexcept
            requires(Contracts::CopyAssignment::kIsTrivial)
        = default;

        Record& operator=(const Record& other)
            noexcept(Contracts::CopyAssignment::kIsNoexcept)
            requires(
                Contracts::CopyAssignment::kIsValid
                && !Contracts::CopyAssignment::kIsTrivial
            )
        {
            if (this == &other) [[unlikely]] {
                return *this;
            }

            using Enumerator = Rebind<detail::enumerate::Enumerate>::AsStored;
            Enumerator::apply(
                [&]<std::size_t I, field::concepts::Field F>() mutable {
                    get<F>(*this) = get<F>(other);
                }
            );
            return *this;
        }

        Record& operator=(const Record&)
            noexcept
            requires(!Contracts::CopyAssignment::kIsValid)
        {
            Contracts::CopyAssignment::diagnose();
            std::unreachable();
        }

#pragma endregion Copy Assignment Operator

#pragma region Move Constructor

        Record(Record&&)
            noexcept
            requires(Contracts::MoveConstructor::kIsTrivial)
        = default;

        Record(Record&& other)
            noexcept(Contracts::MoveConstructor::kIsNoexcept)
            requires(
                Contracts::MoveConstructor::kIsValid
                && !Contracts::MoveConstructor::kIsTrivial
            )
        {
            using Enumerator = Rebind<detail::enumerate::Enumerate>::AsStored;
            Enumerator::apply(
                [&]<std::size_t I, field::concepts::Field F>() mutable {
                    std::construct_at(getLoc<F>(), std::move(get<F>(other)));
                }
            );
        }

        Record(Record&&)
            noexcept
            requires(!Contracts::MoveConstructor::kAvailable)
        {
            Contracts::MoveConstructor::diagnose();
            std::unreachable();
        }

#pragma endregion Move Constructor

#pragma region Move Assignment Operator

        Record &operator=(Record&&)
            noexcept
            requires(Contracts::MoveAssignment::kIsTrivial)
        = default;

        Record &operator=(Record&& other)
            noexcept(Contracts::MoveAssignment::kIsNoexcept)
            requires(
                Contracts::MoveAssignment::kIsValid
                && !Contracts::MoveAssignment::kIsTrivial
            )
        {
            if (this == &other) [[unlikely]] {
                return *this;
            }
            using Enumerator = Rebind<detail::enumerate::Enumerate>::AsStored;
            Enumerator::apply(
                [&]<std::size_t I, field::concepts::Field F>() mutable {
                    get<F>(*this) = std::move(get<F>(other));
                }
            );
            return *this;
        }

        Record &operator=(Record&&)
            noexcept
            requires(!Contracts::MoveAssignment::kIsValid)
        {
            Contracts::MoveAssignment::diagnose();
            std::unreachable();
        }

#pragma endregion Move Assignment Operator

#pragma endregion Copy/Move Constructors & Assignment Operators


#pragma region Destructor

        ~Record()
            noexcept
            requires(Contracts::Destructor::kIsTrivial)
        = default;

        ~Record()
            noexcept(Contracts::Destructor::kIsNoexcept)
            requires(
                Contracts::Destructor::kIsValid
                && !Contracts::Destructor::kIsTrivial
            )
        {
            using Enumerator = Rebind<detail::enumerate::Enumerate>::AsStoredReversed;
            Enumerator::apply(
                [&]<std::size_t I, field::concepts::Field F>() mutable {
                    std::destroy_at(getLoc<F>());
                }
            );
        }

        ~Record()
            noexcept
            requires(!Contracts::Destructor::kIsValid)
        {
            Contracts::Destructor::diagnose();
            std::unreachable();
        }

#pragma endregion Destructor


#pragma region Equality & Comparison Operators (Deleted)

        /**
         * @brief Equality comparison operator, member version.
         *
         * @deprecated See record::equals.
         *
         * @return Nothing, because it's deleted.
         */
        [[nodiscard]] bool operator==(const Record&) const = DELETE(
            "Standard equality is disabled. Use equals<ComparisonOrder>(lhs, rhs) "
            "to specify Storage or Declaration order."
        );

        /**
         * @brief Equality comparison operator, free function version.
         *
         * @deprecated See record::equals.
         *
         * @return Nothing, because it's deleted.
         */
        [[nodiscard]] friend bool operator==(const Record&, const Record&) = DELETE(
            "Standard equality is disabled. Use equals<ComparisonOrder>(lhs, rhs) "
            "to specify Storage or Declaration order."
        );

        /**
         * @brief Three way comparison operator, member version.
         *
         * @deprecated See record::compare.
         *
         * @return Nothing, because it's deleted.
         */
        [[nodiscard]] auto operator<=>(const Record&) const = DELETE(
            "Standard comparison is disabled. Use compare<ComparisonOrder>(lhs, rhs) "
            "to specify Storage or Declaration order."
        );

        /**
         * @brief Three way comparison operator, free function version.
         *
         * @deprecated See record::compare.
         *
         * @return Nothing, because it's deleted.
         */
        [[nodiscard]] friend auto operator<=>(const Record&, const Record&) = DELETE(
            "Standard comparison is disabled. Use compare<ComparisonOrder>(lhs, rhs) "
            "to specify Storage or Declaration order."
        );

#pragma endregion Equality & Comparison Operators (Deleted)

#pragma endregion Special Member Functions

    private:

#pragma region Friend Helpers

#pragma region Getter Friends

#pragma region Nominal Getter Friends

        /**
         * @brief Retrieves a field by its type.
         *
         * @tparam F The field to retrieve.
         * @param record The source Record.
         * @return <c>T&</c>, where <c>T</c> is <c>F::StoredType</c>.
         *
         * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>.
         * @pre <c>Record<FriendFields...>::Traits::Has<F></c> is <c>true</c>.
         *
         * Example:
         * @code{.cpp}
         * struct Name : RecordField<std::string> {};
         * struct Age  : RecordField<int> {};
         *
         * auto r = Record<Name, Age>{
         *     Bind<Age>(25),
         *     Bind<Name>("Alice")
         * };
         *
         * auto& name = get<Name>(r);  // "Alice"
         * auto& age  = get<Age>(r);   // 25
         * @endcode
         */
        template<field::concepts::Field F, field::concepts::Field... FriendFields>
            requires(
                Record<FriendFields...>::HelperContracts
                ::template GetF<F>::kIsValid
            )
        friend auto& get(
            Record<FriendFields...>& record
        ) noexcept;

        /// @brief Trap overload of <c>record::get<F>(Record&)</c>.
        template<field::concepts::Field F, field::concepts::Field... FriendFields>
            requires(
                !Record<FriendFields...>::HelperContracts
                ::template GetF<F>::kIsValid
            )
        friend void get(
            Record<FriendFields...>&
        ) noexcept;

        /**
         * @brief Retrieves a field by its type.
         *
         * @tparam F The field to retrieve.
         * @param record The source Record.
         * @return <c>const T&</c>, where <c>T</c> is <c>F::StoredType</c>.
         *
         * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>.
         * @pre <c>Record<FriendFields...>::Traits::Has<F></c> is <c>true</c>.
         *
         * Example:
         * @code{.cpp}
         * struct Name : RecordField<std::string> {};
         * struct Age  : RecordField<int> {};
         *
         * auto r = Record<Name, Age>{
         *     Bind<Age>(25),
         *     Bind<Name>("Alice")
         * };
         *
         * auto& name = get<Name>(r);  // "Alice"
         * auto& age  = get<Age>(r);   // 25
         * @endcode
         */
        template<field::concepts::Field F, field::concepts::Field... FriendFields>
            requires(
                Record<FriendFields...>::HelperContracts
                ::template GetF<F>::kIsValid
            )
        friend const auto& get(
            const Record<FriendFields...>& record
        ) noexcept;

        /// @brief Trap overload of <c>record::get<F>(const Record&)</c>.
        template<field::concepts::Field F, field::concepts::Field... FriendFields>
            requires(
                !Record<FriendFields...>::HelperContracts
                ::template GetF<F>::kIsValid
            )
        friend void get(
            const Record<FriendFields...>&
        ) noexcept;

#pragma endregion Nominal Getter Friends

#pragma region Positional Getter Friends

        /**
         * @brief Retrieves a field by its declaration index.
         *
         * @tparam I Declaration index (0-based) into <c>Fields...</c>.
         * @param record The source Record.
         * @return <c>T&</c>, where <c>T</c> is <c>F::StoredType</c>, and <c>F</c>
         * is the Field at declaration position <c>I</c> in the <c>Fields...</c> pack.
         *
         * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>
         * @pre <c>I < Record<FriendFields...>::kFieldCount</c>.
         *
         * @warning Index <c>I</c> refers to <b>declaration order</b>
         * (the order in which fields were specified as template arguments),
         * <b>not storage order</b>.
         * For iteration over physically contiguous fields (e.g., for serialization), use
         * <c>kStorageOrder</c> with <c>get<I>()</c>.
         *
         * @code{.cpp}
         * struct Name : RecordField<std::string> {};
         * struct Age : RecordField<int> {};
         * struct Height : RecordField<float> {};
         *
         * auto r = Record<Name, Age, Height> {
         *     Bind<Name>("John"),
         *     Bind<Age>(22),
         *     Bind<Height>(175.3)
         * };
         * auto& name   = get<0>(r);    // "John"
         * auto& age    = get<1>(r);    // 22
         * auto& height = get<2>(r);    // 175.3
         * @endcode
         */
        template<std::size_t I, field::concepts::Field... FriendFields>
            requires(
                Record<FriendFields...>::HelperContracts
                ::template GetI<I>::kIsValid
            )
        friend auto& get(
            Record<FriendFields...>& record
        ) noexcept;

        /// @brief Trap overload of <c>record::get<I>(Record&)</c>.
        template<std::size_t I, field::concepts::Field... FriendFields>
            requires(
                !Record<FriendFields...>::HelperContracts
                ::template GetI<I>::kIsValid
            )
        friend void get(
            Record<FriendFields...>&
        ) noexcept;

        /**
         * @brief Retrieves a field by its declaration index.
         *
         * @tparam I Declaration index (0-based) into <c>Fields...</c>.
         * @param record The source Record.
         * @return <c>T&</c>, where <c>T</c> is <c>F::StoredType</c>, and <c>F</c>
         * is the Field at declaration position <c>I</c> in the <c>Fields...</c> pack.
         *
         * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>
         * @pre <c>I < Record<FriendFields...>::kFieldCount</c>.
         *
         * @warning Index <c>I</c> refers to <b>declaration order</b>
         * (the order in which fields were specified as template arguments), <b>not storage order</b>.
         * For iteration over physically contiguous fields (e.g., for serialization), use
         * <c>kStorageOrder</c> with <c>get<I>()</c>.
         *
         * @code{.cpp}
         * struct Name : RecordField<std::string> {};
         * struct Age : RecordField<int> {};
         * struct Height : RecordField<float> {};
         *
         * auto r = Record<Name, Age, Height> {
         *     Bind<Name>("John"),
         *     Bind<Age>(22),
         *     Bind<Height>(175.3)
         * };
         * auto& name   = get<0>(r);    // "John"
         * auto& age    = get<1>(r);    // 22
         * auto& height = get<2>(r);    // 175.3
         * @endcode
         */
        template<std::size_t I, field::concepts::Field... FriendFields>
            requires(
                Record<FriendFields...>::HelperContracts
                ::template GetI<I>::kIsValid
            )
        friend const auto& get(
            const Record<FriendFields...>& record
        ) noexcept;

        /// @brief Trap overload of <c>record::get<I>(const Record&)</c>.
        template<std::size_t I, field::concepts::Field... FriendFields>
            requires(
                !Record<FriendFields...>::HelperContracts
                ::template GetI<I>::kIsValid
            )
        friend void get(
            const Record<FriendFields...>&
        ) noexcept;

#pragma endregion Positional Getter Friends

#pragma endregion Getter Friends

#pragma region Equality & Comparison Friends

        /**
         * @brief Compares all <c>Field</c>s according to their respective
         * equality comparison functors, short-circuiting on the first mismatch.
         *
         * @param lhs A <c>Record</c> to compare.
         * @param rhs Another <c>Record</c> to compare against.
         *
         * @return <c>true</c> if all corresponding <c>Field</c>s compare equal,
         * <c>false</c> otherwise.
         *
         * @pre For each <c>Field</c> <c>F</c>, if <c>T</c> is <c>F::StoredType</c>,
         * then each <c>T</c> <b>must</b> satisfy <c>std::equality_comparable</c>.
         *
         * @note For each <c>Field</c> <c>F</c>, if the typedef <c>F::EqualTo</c> exists, then it
         * will be used for the comparison. Otherwise, <c>std::equal_to</c> will be used instead.
         *
         * @note Comparison order is the storage order of the <c>Field</c>s, <b>NOT</b> the declaration order.
         * Thus, the order in which <c>Field</c>s are declared in the pack has no effect on the order of comparisons.
         *
         * @code{.cpp}
         * struct Name   : RecordField<std::string> {};
         * struct Age    : RecordField<int> {};
         * struct Height : RecordField<double> {};
         *
         * auto a = Record<Name, Age, Height>{
         *     Bind<Name>("Alice"),
         *     Bind<Age>(25),
         *     Bind<Height>(168.5),
         * };
         * auto b = Record<Name, Age, Height>{
         *     Bind<Name>("Alice"),
         *     Bind<Age>(25),
         *     Bind<Height>(173.9)
         * };
         *
         * equals(a, a); // true: every field compares equal
         * equals(a, b); // false: Height differs, Age is never compared
         * @endcode
         */
        template<
            field::concepts::Field... FriendFields,
            field::concepts::Field... OtherFriendFields
        >
            requires(
                Record<FriendFields...>::HelperContracts
                ::template Equals<OtherFriendFields...>::kIsValid
            )
        friend bool equals(
            const Record<FriendFields...>& lhs,
            const Record<OtherFriendFields...>& rhs
        ) noexcept(
            Record<FriendFields...>::HelperContracts
            ::template Equals<OtherFriendFields...>::kIsNoexcept
        );

        template<
            field::concepts::Field... FriendFields,
            field::concepts::Field... OtherFriendFields
        >
            requires(
                !Record<FriendFields...>::HelperContracts
                ::template Equals<OtherFriendFields...>::kIsValid
            )
        friend void equals(
            const Record<FriendFields...>&,
            const Record<OtherFriendFields...>&
        ) noexcept;

        /**
         * @brief Compares all <c>Field</c>s according to their respective
         * three-way comparison functors, short-circuiting on the first mismatch.
         *
         * If all fields compare equivalent, returns the weakest common comparison
         * category among fields, as determined by <c>std::common_comparison_category_t<
         * std::compare_three_way_result_t<typename Fields::StoredType>...
         * ></c>.
         *
         * @param lhs A <c>Record</c> to compare.
         * @param rhs Another <c>Record</c> to compare against.
         *
         * @return A result of type std::common_comparison_category_t<
         * std::compare_three_way_result_t<typename Fields::StoredType>...
         * >
         *
         * @pre For each <c>Field</c> <c>F</c>, if <c>T</c> is <c>F::StoredType</c>,
         * then each <c>T</c> <b>must</b> satisfy <c>std::three_way_comparable</c>.
         *
         * @note For each <c>Field</c> <c>F</c>, if the typedef <c>F::CompareThreeWay</c> exists, then it
         * will be used for the comparison. Otherwise, <c>std::compare_three_way</c> will be used instead.
         *
         * @note Comparison order is the declaration order of the <c>Field</c>s.
         * Thus, the order in which <c>Field</c>s are declared
         * in the pack will define the order of comparisons.
         *
         * @code{.cpp}
         * struct FirstName : RecordField<std::string> {};
         * struct LastName  : RecordField<std::string> {};
         * struct Age       : RecordField<int> {};
         * struct Height    : RecordField<double> {};
         *
         * auto alice = Record<FirstName, LastName, Age, Height>{
         *     Bind<FirstName>("Alice"),
         *     Bind<LastName>("Smith"),
         *     Bind<Age>(25),
         *     Bind<Height>(168.5)
         * };
         * auto bob = Record<FirstName, LastName, Age, Height>{
         *     Bind<FirstName>("Bob"),
         *     Bind<LastName>("Smith"),
         *     Bind<Age>(23),
         *     Bind<Height>(173.9)
         * };
         *
         * auto cmp1 = compare(alice, bob); // cmp1 < 0:
         * //     "Alice" < "Bob"    [evaluated, short circuits here]
         * //     "Smith" == "Smith" [not evaluated],
         * //     25 > 23            [not evaluated],
         * //     168.5 < 173.9      [not evaluated],
         *
         * auto cmp2 = compare(alice, alice); // cmp2 == 0:
         * //     "Alice" == "Alice", [evaluated]
         * //     "Smith" == "Smith", [evaluated]
         * //     25 == 25,           [evaluated]
         * //     168.5 == 168.5      [evaluated]
         * @endcode
         */
        template<field::concepts::Field... FriendFields>
            requires(Record<FriendFields...>::HelperContracts::Compare::kIsValid)
        friend auto compare(
            const Record<FriendFields...>& lhs,
            const Record<FriendFields...>& rhs
        ) noexcept(Record<FriendFields...>::HelperContracts::Compare::kIsNoexcept);

#pragma endregion Equality & Comparison Friends

#pragma region Projection & Extraction Friends

        /**
         * @brief Creates a new <c>Record</c> containing a subset
         * of the original's <c>Field</c>s, via copy semantics.
         *
         * @tparam Selected The subset of <c>Field</c>s to retain.
         *
         * @param record The source <c>Record</c>.
         *
         * @return A new <c>Record</c> containing copies of the specified <c>Field</c>s.
         *
         * @pre All <c>Selected</c> <c>Field</c> types must be present in the source
         * <c>Record</c>'s <c>Field</c> pack.
         *
         * @pre For each of the <c>Selected</c> <c>Field</c> types <c>F</c>,
         * <c>F::StoredType</c> must be copy constructible.
         *
         * @note The resulting <c>Record</c>'s type is different from that of the original
         * <c>Record</c>, and as such its physical layout may be re-optimized independently
         * of the source.
         *
         * @note This is a copy; modifications to the projected <c>Record</c> do not affect the original.
         *
         * @code{.cpp}
         * struct Name : RecordField<std::string> {};
         * struct Age  : RecordField<int> {};
         * struct City : RecordField<std::string> {};
         *
         * auto full = Record<Name, Age, City>{
         *     Bind<Name>("Alice"),
         *     Bind<Age>(25),
         *     Bind<City>("Paris")
         * };
         *
         * auto nameAndAge = project<Name, Age>(full);
         * // nameAndAge has type Record<Name, Age>
         * // nameAndAge contains copies of "Alice" and 25
         * @endcode
         */
        template<
            field::concepts::Field... Selected,
            field::concepts::Field... Original
        >
            requires(
                Record<Original...>::HelperContracts
                ::template Project<Selected...>::kIsValid
            )
        friend auto project(
            const Record<Original...>& record
        ) noexcept(
            Record<Original...>::HelperContracts
            ::template Project<Selected...>::kIsNoexcept
        );

        /// @brief Trap overload of <c>record::project</c>.
        template<
            field::concepts::Field... Selected,
            field::concepts::Field... Original
        >
            requires(
                !Record<Original...>::HelperContracts
                ::template Project<Selected...>::kIsValid
            )
        friend void project(
            const Record<Original...>&
        ) noexcept;

        /**
         * @brief Creates a new <c>Record</c> containing a subset
         * of the original's <c>Field</c>s, via move semantics.
         *
         * Destructively extracts the specified <c>Field</c>s from the source <c>Record</c>.
         * The original <c>Record</c> is left in a valid but <b>unspecified</b> state
         * (each extracted <c>Field</c> is moved-from).
         *
         * @tparam Selected The subset of <c>Field</c>s to retain.
         *
         * @param record The source <c>Record</c>.
         *
         * @return A new <c>Record</c> containing the extracted <c>Field</c>s.
         *
         * @pre All <c>Selected</c> <c>Field</c> types must be
         * present in the source <c>Record</c>'s <c>Field</c> pack.
         *
         * @pre For each of the <c>Selected</c> <c>Field</c> types <c>F</c>,
         * <c>F::StoredType</c> must be move constructible.
         *
         * @warning After extraction, the source <c>Selected</c>
         * <c>Field</c>s are in a valid but unspecified state.
         *
         * @note The resulting <c>Record</c>'s type is different from that of the original <c>Record</c>,
         * and as such its physical layout may be re-optimized independently of the source.
         *
         * @note Only the <c>Selected</c> <c>Field</c>s are affected by the move operation.
         * <c>Field</c>s not in the <c>Selected</c> pack are not modified or affected.
         *
         * @code{.cpp}
         * struct Name : RecordField<std::string> {};
         * struct Age  : RecordField<int> {};
         * struct City : RecordField<std::string> {};
         *
         * auto full = Record<Name, Age, City>{
         *     Bind<Name>("Alice"),
         *     Bind<Age>(25),
         *     Bind<City>("Paris"),
         * };
         *
         * auto extracted = extract<Name, Age>(std::move(full));
         * // extracted has type Record<Name, Age>
         * // The Fields `Name` and `Age` of the Record `full` are now in an unspecified state
         * // However, the Field `City` is left untouched.
         * // Thus, get<City>(full), for example, will reliably produce "Paris".
         * @endcode
         */
        template<
            field::concepts::Field... Selected,
            field::concepts::Field... Original
        >
        requires(
            Record<Original...>::HelperContracts
            ::template Extract<Selected...>::kIsValid
        )
        friend auto extract(
            Record<Original...>&& record
        ) noexcept(
            Record<Original...>::HelperContracts
            ::template Extract<Selected...>::kIsNoexcept
        );

        /// @brief Trap overload of <c>record::extract</c>.
        template<
            field::concepts::Field... Selected,
            field::concepts::Field... Original
        >
            requires(
                !Record<Original...>::HelperContracts
                ::template Extract<Selected...>::kIsValid
            )
        friend void extract(
            Record<Original...>&&
        ) noexcept;

#pragma endregion Projection & Extraction Friends

#pragma endregion Friend Helpers

        alignas(std::max({ std::size_t{1}, Fields::Alignment... }))
        std::array<std::byte, Layout::kAlignedSize> mData;
    };

#pragma region ADL Helpers

#pragma region Getter Helpers

#pragma region Nominal Getter Helpers

    /**
     * @brief Retrieves a field by its type.
     *
     * @tparam F The field to retrieve.
     * @param record The source Record.
     * @return <c>T&</c>, where <c>T</c> is <c>F::StoredType</c>.
     *
     * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>.
     * @pre <c>Record<FriendFields...>::Traits::Has<F></c> is <c>true</c>.
     *
     * Example:
     * @code{.cpp}
     * struct Name : RecordField<std::string> {};
     * struct Age  : RecordField<int> {};
     *
     * auto r = Record<Name, Age>{
     *     Bind<Age>(25),
     *     Bind<Name>("Alice")
     * };
     *
     * auto& name = get<Name>(r);  // "Alice"
     * auto& age  = get<Age>(r);   // 25
     * @endcode
     */
    template<field::concepts::Field F, field::concepts::Field... Fields>
        requires(
            Record<Fields...>::HelperContracts
            ::template GetF<F>::kIsValid
        )
    [[nodiscard]] auto& get(
        Record<Fields...>& record
    ) noexcept {
        return *std::launder(record.template getLoc<F>());
    }

    /// @brief Trap overload of <c>record::get<F>(Record&)</c>.
    template<field::concepts::Field F, field::concepts::Field... Fields>
        requires(
            !Record<Fields...>::HelperContracts
            ::template GetF<F>::kIsValid
        )
    void get(
        Record<Fields...>&
    ) noexcept {
        Record<Fields...>::HelperContracts
        ::template GetF<F>::diagnose();
    }

    /**
     * @brief Retrieves a field by its type.
     *
     * @tparam F The field to retrieve.
     * @param record The source Record.
     * @return <c>const T&</c>, where <c>T</c> is <c>F::StoredType</c>.
     *
     * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>.
     * @pre <c>Record<FriendFields...>::Traits::Has<F></c> is <c>true</c>.
     *
     * Example:
     * @code{.cpp}
     * struct Name : RecordField<std::string> {};
     * struct Age  : RecordField<int> {};
     *
     * auto r = Record<Name, Age>{
     *     Bind<Age>(25),
     *     Bind<Name>("Alice")
     * };
     *
     * auto& name = get<Name>(r);  // "Alice"
     * auto& age  = get<Age>(r);   // 25
     * @endcode
     */
    template<field::concepts::Field F, field::concepts::Field... Fields>
        requires(
            Record<Fields...>::HelperContracts
            ::template GetF<F>::kIsValid
        )
    [[nodiscard]] const auto& get(
        const Record<Fields...>& record
    ) noexcept {
        return *std::launder(record.template getLoc<F>());
    }

    /// @brief Trap overload of <c>record::get<F>(const Record&)</c>.
    template<field::concepts::Field F, field::concepts::Field... Fields>
        requires(
            !Record<Fields...>::HelperContracts
            ::template GetF<F>::kIsValid
        )
    void get(
        const Record<Fields...>&
    ) noexcept {
        Record<Fields...>::HelperContracts
            ::template GetF<F>::diagnose();
    }

#pragma endregion Nominal Getter Helpers

#pragma region Positional Getter Helpers

    /**
     * @brief Retrieves a field by its declaration index.
     *
     * @tparam I Declaration index (0-based) into <c>Fields...</c>.
     * @param record The source Record.
     * @return <c>T&</c>, where <c>T</c> is <c>F::StoredType</c>, and <c>F</c>
     * is the Field at declaration position <c>I</c> in the <c>Fields...</c> pack.
     *
     * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>
     * @pre <c>I < Record<FriendFields...>::kFieldCount</c>.
     *
     * @warning Index <c>I</c> refers to <b>declaration order</b>
     * (the order in which fields were specified as template arguments),
     * <b>not storage order</b>.
     * For iteration over physically contiguous fields (e.g., for serialization), use
     * <c>kStorageOrder</c> with <c>get<I>()</c>.
     *
     * @code{.cpp}
     * struct Name : RecordField<std::string> {};
     * struct Age : RecordField<int> {};
     * struct Height : RecordField<float> {};
     *
     * auto r = Record<Name, Age, Height> {
     *     Bind<Name>("John"),
     *     Bind<Age>(22),
     *     Bind<Height>(175.3)
     * };
     * auto& name   = get<0>(r);    // "John"
     * auto& age    = get<1>(r);    // 22
     * auto& height = get<2>(r);    // 175.3
     * @endcode
     */
    template<std::size_t I, field::concepts::Field... Fields>
        requires(
            Record<Fields...>::HelperContracts
            ::template GetI<I>::kIsValid
        )
    [[nodiscard]] auto& get(
        Record<Fields...>& record
    ) noexcept {
        using F = std::tuple_element_t<I, std::tuple<Fields...>>;
        return *std::launder(record.template getLoc<F>());
    }

    /// @brief Trap overload of <c>record::get<I>(Record&)</c>.
    template<std::size_t I, field::concepts::Field... Fields>
        requires(
            !Record<Fields...>::HelperContracts
            ::template GetI<I>::kIsValid
        )
    void get(
        Record<Fields...>&
    ) noexcept {
        Record<Fields...>::HelperContracts
        ::template GetI<I>::diagnose();
    }

    /**
     * @brief Retrieves a field by its declaration index.
     *
     * @tparam <c>I</c> Declaration index (0-based) into <c>Fields...</c>.
     * @param record The source Record.
     * @return <c>const T&</c>, where <c>T</c> is <c>F::stored_type</c>, and <c>F</c>
     * is the Field at declaration position <c>I</c> in the <c>Fields...</c> pack.
     *
     * @pre <c>Record<FriendFields...>::kFieldCount > 0</c>
     * @pre <c>I < Record<FriendFields...>::kFieldCount</c>.
     *
     * @warning Index <c>I</c> refers to <b>declaration order</b>
     * (the order in which fields were specified as template arguments),
     * <b>not storage order</b>.
     * For iteration over physically contiguous fields (e.g., for serialization), use
     * <c>kStorageOrder</c> with <c>get<I>()</c>.
     *
     * @code{.cpp}
     * struct Name : RecordField<std::string> {};
     * struct Age : RecordField<int> {};
     * struct Height : RecordField<float> {};
     *
     * auto r = Record<Name, Age, Height> {
     *     Bind<Name>("John"),
     *     Bind<Age>(22),
     *     Bind<Height>(175.3)
     * };
     * auto& name   = get<0>(r);    // "John"
     * auto& age    = get<1>(r);    // 22
     * auto& height = get<2>(r);    // 175.3
     * @endcode
     */
    template<std::size_t I, field::concepts::Field... Fields>
        requires(
            Record<Fields...>::HelperContracts
            ::template GetI<I>::kIsValid
        )
    [[nodiscard]] const auto& get(
        const Record<Fields...>& record
    ) noexcept {
        using F = std::tuple_element_t<I, std::tuple<Fields...>>;
        return *std::launder(record.template getLoc<F>());
    }

    /// @brief Trap overload of <c>record::get<I>(const Record&)</c>.
    template<std::size_t I, field::concepts::Field... Fields>
        requires(
            !Record<Fields...>::HelperContracts
            ::template GetI<I>::kIsValid
        )
    void get(
        const Record<Fields...>&
    ) noexcept {
        Record<Fields...>::HelperContracts
        ::template GetI<I>::diagnose();
    }

#pragma endregion Positional Getter Helpers

#pragma endregion Getter Helpers

#pragma region Equality & Comparison Helpers

    /**
     * @brief Compares all <c>Field</c>s according to their respective
     * equality comparison functors, short-circuiting on the first mismatch.
     *
     * @param lhs A <c>Record</c> to compare.
     * @param rhs Another <c>Record</c> to compare against.
     *
     * @return <c>true</c> if all corresponding <c>Field</c>s compare equal,
     * <c>false</c> otherwise.
     *
     * @pre For each <c>Field</c> <c>F</c>, if <c>T</c> is <c>F::StoredType</c>,
     * then each <c>T</c> <b>must</b> satisfy <c>std::equality_comparable</c>.
     *
     * @note For each <c>Field</c> <c>F</c>, if the typedef <c>F::EqualTo</c> exists, then it
     * will be used for the comparison. Otherwise, <c>std::equal_to</c> will be used instead.
     *
     * @note Comparison order is the storage order of the <c>Field</c>s, <b>NOT</b> the declaration order.
     * Thus, the order in which <c>Field</c>s are declared in the pack has no effect on the order of comparisons.
     *
     * @code{.cpp}
     * struct Name   : RecordField<std::string> {};
     * struct Age    : RecordField<int> {};
     * struct Height : RecordField<double> {};
     *
     * auto a = Record<Name, Age, Height>{
     *     Bind<Name>("Alice"),
     *     Bind<Age>(25),
     *     Bind<Height>(168.5),
     * };
     * auto b = Record<Name, Age, Height>{
     *     Bind<Name>("Alice"),
     *     Bind<Age>(25),
     *     Bind<Height>(173.9)
     * };
     *
     * equals(a, a); // true: every field compares equal
     * equals(a, b); // false: Height differs, Age is never compared
     * @endcode
     */
    template<
        field::concepts::Field... Fields,
        field::concepts::Field... OtherFields
    >
        requires(
            Record<Fields...>::HelperContracts
            ::template Equals<OtherFields...>::kIsValid
        )
    [[nodiscard]] bool equals(
        const Record<Fields...>& lhs,
        const Record<OtherFields...>& rhs
    ) noexcept(
        Record<Fields...>::HelperContracts
        ::template Equals<OtherFields...>::kIsNoexcept
    ) {
        using Enumerator = Record<Fields...>
        ::template Rebind<detail::enumerate::Enumerate>::AsStored;

        if constexpr (Record<Fields...>::kFieldCount == 0) {
            return true;
        }

        return Enumerator::allOf(
            [&]<std::size_t I, field::concepts::Field F>() {
                using EqualsT = std::conditional_t<
                    field::concepts::detail::HasEqualTo<F>,
                    typename F::EqualTo,
                    std::equal_to<typename F::StoredType>
                >;
                return EqualsT{}(
                    get<F>(lhs),
                    get<F>(rhs)
                );
            }
        );
    }

    /// @brief Trap overload of <c>record::equals</c>.
    template<
        field::concepts::Field... Fields,
        field::concepts::Field... OtherFields
    >
        requires(
            !Record<Fields...>::HelperContracts
            ::template Equals<OtherFields...>::kIsValid
        )
    void equals(
        const Record<Fields...>&,
        const Record<OtherFields...>&
    ) noexcept {
        Record<Fields...>::HelperContracts
        ::template Equals<OtherFields...>::diagnose();
    }

    /**
     * @brief Compares all <c>Field</c>s according to their respective
     * three-way comparison functors, short-circuiting on the first mismatch.
     *
     * If all fields compare equivalent, returns the weakest common comparison
     * category among fields, as determined by <c>std::common_comparison_category_t<
     * std::compare_three_way_result_t<typename Fields::StoredType>...
     * ></c>.
     *
     * @param lhs A <c>Record</c> to compare.
     * @param rhs Another <c>Record</c> to compare against.
     *
     * @return A result of type std::common_comparison_category_t<
     * std::compare_three_way_result_t<typename Fields::StoredType>...
     * >
     *
     * @pre For each <c>Field</c> <c>F</c>, if <c>T</c> is <c>F::StoredType</c>,
     * then each <c>T</c> <b>must</b> satisfy <c>std::three_way_comparable</c>.
     *
     * @note For each <c>Field</c> <c>F</c>, if the typedef <c>F::CompareThreeWay</c> exists, then it
     * will be used for the comparison. Otherwise, <c>std::compare_three_way</c> will be used instead.
     *
     * @note Comparison order is the declaration order of the <c>Field</c>s.
     * Thus, the order in which <c>Field</c>s are declared
     * in the pack will define the order of comparisons.
     *
     * @code{.cpp}
     * struct FirstName : RecordField<std::string> {};
     * struct LastName  : RecordField<std::string> {};
     * struct Age       : RecordField<int> {};
     * struct Height    : RecordField<double> {};
     *
     * auto alice = Record<FirstName, LastName, Age, Height>{
     *     Bind<FirstName>("Alice"),
     *     Bind<LastName>("Smith"),
     *     Bind<Age>(25),
     *     Bind<Height>(168.5)
     * };
     * auto bob = Record<FirstName, LastName, Age, Height>{
     *     Bind<FirstName>("Bob"),
     *     Bind<LastName>("Smith"),
     *     Bind<Age>(23),
     *     Bind<Height>(173.9)
     * };
     *
     * auto cmp1 = compare(alice, bob); // cmp1 < 0:
     * //     "Alice" < "Bob"    [evaluated, short circuits here]
     * //     "Smith" == "Smith" [not evaluated],
     * //     25 > 23            [not evaluated],
     * //     168.5 < 173.9      [not evaluated],
     *
     * auto cmp2 = compare(alice, alice); // cmp2 == 0:
     * //     "Alice" == "Alice", [evaluated]
     * //     "Smith" == "Smith", [evaluated]
     * //     25 == 25,           [evaluated]
     * //     168.5 == 168.5      [evaluated]
     * @endcode
     */
    template<field::concepts::Field... Fields>
        requires(
            Record<Fields...>::HelperContracts
            ::Compare::kIsValid
        )
    [[nodiscard]] auto compare(
        const Record<Fields...>& lhs,
        const Record<Fields...>& rhs
    ) noexcept(
        Record<Fields...>::HelperContracts
        ::Compare::kIsNoexcept
    ) {
        using ResultT = Record<Fields...>::Traits
            ::ThreeWayComparison::CommonCompareResultT;

        auto finalResult = ResultT::equivalent;

        if constexpr (Record<Fields...>::kFieldCount == 0) {
            return finalResult;
        }

        using Enumerator = Record<Fields...>
        ::template Rebind<detail::enumerate::Enumerate>::AsDeclared;

        Enumerator::anyOf(
            [&]<std::size_t I, field::concepts::Field F>() {
                using CompareT = std::conditional_t<
                    field::concepts::detail::HasCompareThreeWay<F>,
                    typename F::CompareThreeWay,
                    std::compare_three_way
                >;
                auto res = CompareT{}(
                    get<F>(lhs),
                    get<F>(rhs)
                );
                if (res != 0) {
                    finalResult = res;
                    return true;  // Short-circuit
                }
                return false;
            }
        );

        return finalResult;
    }

    /// @brief Trap overload of <c>record::compare</c>.
    template<field::concepts::Field... Fields>
        requires(
            !Record<Fields...>::HelperContracts
            ::Compare::kIsValid
        )
    void compare(
        const Record<Fields...>&,
        const Record<Fields...>&
    ) noexcept {
        Record<Fields...>::HelperContracts
            ::Compare::diagnose();
    }

#pragma endregion Equality & Comparison Helpers

#pragma region Projection & Extraction Helpers

    /**
     * @brief Creates a new <c>Record</c> containing a subset
     * of the original's <c>Field</c>s, via copy semantics.
     *
     * @tparam Selected The subset of <c>Field</c>s to retain.
     *
     * @param record The source <c>Record</c>.
     *
     * @return A new <c>Record</c> containing copies of the specified <c>Field</c>s.
     *
     * @pre All <c>Selected</c> <c>Field</c> types must be present in the source
     * <c>Record</c>'s <c>Field</c> pack.
     *
     * @pre For each of the <c>Selected</c> <c>Field</c> types <c>F</c>,
     * <c>F::StoredType</c> must be copy constructible.
     *
     * @note The resulting <c>Record</c>'s type is different from that of the original
     * <c>Record</c>, and as such its physical layout may be re-optimized independently
     * of the source.
     *
     * @note This is a copy; modifications to the projected <c>Record</c> do not affect the original.
     *
     * @code{.cpp}
     * struct Name : RecordField<std::string> {};
     * struct Age  : RecordField<int> {};
     * struct City : RecordField<std::string> {};
     *
     * auto full = Record<Name, Age, City>{
     *     Bind<Name>("Alice"),
     *     Bind<Age>(25),
     *     Bind<City>("Paris")
     * };
     *
     * auto nameAndAge = project<Name, Age>(full);
     * // nameAndAge has type Record<Name, Age>
     * // nameAndAge contains copies of "Alice" and 25
     * @endcode
     */
    template<
        field::concepts::Field... Selected,
        field::concepts::Field... Original
    >
        requires(
            Record<Original...>::HelperContracts
            ::template Project<Selected...>::kIsValid
        )
    [[nodiscard]] auto project(
        const Record<Original...>& record
    ) noexcept(
        Record<Original...>::HelperContracts
        ::template Project<Selected...>::kIsNoexcept
    ) {
        return Record<Selected...>{
            Bind<BindStrategy::Forward, Selected>(
                get<Selected>(record)
            )...
        };
    }

    /// @brief Trap overload of <c>record::project</c>.
    template<
        field::concepts::Field... Selected,
        field::concepts::Field... Original
    >
        requires(
            !Record<Original...>::HelperContracts
            ::template Project<Selected...>::kIsValid
        )
    void project(
        const Record<Original...>&
    ) noexcept {
        Record<Original...>::HelperContracts
        ::template Project<Selected...>::diagnose();
    }

    /**
     * @brief Creates a new <c>Record</c> containing a subset
     * of the original's <c>Field</c>s, via move semantics.
     *
     * Destructively extracts the specified <c>Field</c>s from the source <c>Record</c>.
     * The original <c>Record</c> is left in a valid but <b>unspecified</b> state
     * (each extracted <c>Field</c> is moved-from).
     *
     * @tparam Selected The subset of <c>Field</c>s to retain.
     *
     * @param record The source <c>Record</c>.
     *
     * @return A new <c>Record</c> containing the extracted <c>Field</c>s.
     *
     * @pre All <c>Selected</c> <c>Field</c> types must be
     * present in the source <c>Record</c>'s <c>Field</c> pack.
     *
     * @pre For each of the <c>Selected</c> <c>Field</c> types <c>F</c>,
     * <c>F::StoredType</c> must be move constructible.
     *
     * @warning After extraction, the source <c>Selected</c>
     * <c>Field</c>s are in a valid but unspecified state.
     *
     * @note The resulting <c>Record</c>'s type is different from that of the original <c>Record</c>,
     * and as such its physical layout may be re-optimized independently of the source.
     *
     * @note Only the <c>Selected</c> <c>Field</c>s are affected by the move operation.
     * <c>Field</c>s not in the <c>Selected</c> pack are not modified or affected.
     *
     * @code{.cpp}
     * struct Name : RecordField<std::string> {};
     * struct Age  : RecordField<int> {};
     * struct City : RecordField<std::string> {};
     *
     * auto full = Record<Name, Age, City>{
     *     Bind<Name>("Alice"),
     *     Bind<Age>(25),
     *     Bind<City>("Paris"),
     * };
     *
     * auto extracted = extract<Name, Age>(std::move(full));
     * // extracted has type Record<Name, Age>
     * // The Fields `Name` and `Age` of the Record `full` are now in an unspecified state
     * // However, the Field `City` is left untouched.
     * // Thus, get<City>(full), for example, will reliably produce "Paris".
     * @endcode
     */
    template<
        field::concepts::Field... Selected,
        field::concepts::Field... Original
    >
        requires(
            Record<Original...>::HelperContracts
            ::template Extract<Selected...>::kIsValid
        )
    [[nodiscard]] auto extract(
        Record<Original...>&& record
    ) noexcept(
        Record<Original...>::HelperContracts
        ::template Extract<Selected...>::kIsNoexcept
    ) {
        // Do NOT use forward/move here, record::get
        // will NOT work with an rvalue reference, as
        // it'd bind to the const getter, which would
        // then produce const T&&, and result in a
        // copy ctor call instead of a move ctor call.
        //
        // Moving the resulting reference is
        // sufficient for our purposes here.
        return Record<Selected...>{
            Bind<BindStrategy::Forward, Selected>(
                std::move(get<Selected>(record))
            )...
        };
    }

    /// @brief Trap overload of <c>record::extract</c>.
    template<
        field::concepts::Field... Selected,
        field::concepts::Field... Original
    >
        requires(
            !Record<Original...>::HelperContracts
            ::template Extract<Selected...>::kIsValid
        )
    void extract(
        Record<Original...>&&
    ) noexcept {
        Record<Original...>::HelperContracts
        ::template Extract<Selected...>::diagnose();
    }

#pragma endregion Projection & Extraction Helpers

#pragma endregion ADL Helpers

}

#pragma region Tuple-like Interface Helpers

namespace std {

    template<record::field::concepts::Field... Fields>
    struct tuple_size<record::Record<Fields...>>
    : std::integral_constant<std::size_t, record::Record<Fields...>::kFieldCount>
    {};

    template<std::size_t I, record::field::concepts::Field... Fields>
    struct tuple_element<I, record::Record<Fields...>> {
        using type = tuple_element_t<
            I,
            std::tuple<typename Fields::StoredType...>
        >;
    };
}

#pragma endregion Tuple-like Interface Helpers

#endif  // !RECORD_INCLUDE_RECORD_RECORD_HPP
