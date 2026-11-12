#ifndef RECORD_INCLUDE_RECORD_DETAIL_CONTRACTS_HPP
#define RECORD_INCLUDE_RECORD_DETAIL_CONTRACTS_HPP

#include <record/field.hpp>
#include <record/detail/bind.hpp>
#include <record/detail/utils.hpp>

namespace record::detail::contracts {

    template<field::concepts::Field... Fields>
    class RecordContracts final {

        using Traits = field::detail::traits::FieldTraits<Fields...>;
        using Layout = field::detail::layout::FieldLayout<Fields...>;

        template<
            typename QueryTraits,
            utils::string::FixedString Msg
        >
        class Contract final {

            static constexpr bool kAvailable = QueryTraits::kAvailable;

        public:

            static constexpr bool kIsTrivial = QueryTraits::kTrivial;
            static constexpr bool kIsValid = kAvailable;
            static constexpr bool kIsNoexcept = QueryTraits::kNoThrow;

            static constexpr void diagnose() noexcept {
                static_assert(kAvailable, Msg);
            }
        };

        template<FieldBinding... Binds>
        class BindConstructorContract final {

            static constexpr bool kMatchingBinds = detail::utils::set::SetEqual<
                utils::set::Set<Fields...>,
                utils::set::Set<typename std::remove_cvref_t<Binds>::FieldT...>
            >;

        public:

            static constexpr bool kIsValid = kMatchingBinds;
            static constexpr bool kIsNoexcept = (
                ...
                && detail::NoThrowBindFor<
                    typename std::remove_cvref_t<Binds>::FieldT,
                    std::remove_cvref_t<Binds>
                >
            );

            static constexpr void diagnose() noexcept {
                static_assert(
                    kMatchingBinds,
                    "[record::Record::Record(Binds&&...)] "
                    "The Bind constructor is unavailable "
                    "unless each Field is bound exactly once."
                );
            }
        };

    public:

        using DefaultConstructor = Contract<
            typename Traits::DefaultConstruction,
            utils::string::FixedString{
                "[record::Record::Record()] "
                "The default constructor is unavailable "
                "unless all Fields are default constructible"
            }
        >;

        template<FieldBinding... Binds>
        using BindConstructor = BindConstructorContract<Binds...>;

        using CopyConstructor = Contract<
            typename Traits::CopyConstruction,
            utils::string::FixedString{
                "[record::Record::Record(const Record&)] "
                "The copy constructor is unavailable "
                "unless all Fields are copy constructible"
            }
        >;

        using CopyAssignment = Contract<
            typename Traits::CopyAssignment,
            utils::string::FixedString{
                "[record::Record::operator=(const Record&)] "
                "The copy assignment operator is unavailable "
                "unless all Fields are copy assignable"
            }
        >;

        using MoveConstructor = Contract<
            typename Traits::MoveConstruction,
            utils::string::FixedString{
                "[record::Record::Record(Record&&)] "
                "The move constructor is unavailable "
                "unless all Fields are move constructible"
            }
        >;

        using MoveAssignment = Contract<
            typename Traits::MoveAssignment,
            utils::string::FixedString{
                "[record::Record::operator=(Record&&)] "
                "The move assignment operator is unavailable "
                "unless all Fields are move assignable"
            }
        >;

        using Destructor = Contract<
            typename Traits::Destruction,
            utils::string::FixedString{
                "[record::Record::~Record()] "
                "The destructor is unavailable "
                "unless all Fields are destructible"
            }
        >;
    };

    template<field::concepts::Field... Fields>
    class HelperContracts final {

        using Traits = field::detail::traits::FieldTraits<Fields...>;
        using Layout = field::detail::layout::FieldLayout<Fields...>;

        template<std::size_t I>
        class GetIContract final {

            static constexpr bool kFieldsExist = Layout::kFieldCount > 0;
            static constexpr bool kFieldInRange = I < Layout::kFieldCount;

        public:

            static constexpr bool kIsValid = kFieldsExist && kFieldInRange;
            static constexpr bool kIsNoexcept = true;

            static constexpr void diagnose() noexcept {
                static_assert(
                    kFieldsExist,
                    "[record::get<I>] Helper is unavailable "
                    "unless Record has at least one Field."
                );
                static_assert(
                    kFieldInRange,
                    "[record::get<I>] Helper is unavailable "
                    "unless I < sizeof...(Fields)."
                );
            }
        };

        template<field::concepts::Field F>
        class GetFContract final {

            static constexpr bool kFieldsExist = Layout::kFieldCount > 0;
            static constexpr bool kHasField = Traits::template Has<F>;

        public:

            static constexpr bool kIsValid = kFieldsExist && kHasField;
            static constexpr bool kIsNoexcept = true;

            static constexpr void diagnose() noexcept {
                static_assert(
                    kFieldsExist,
                    "[record::get<F>] Helper is unavailable "
                    "unless Record has at least one Field."
                );
                static_assert(
                    kHasField,
                    "[record::get<F>] Helper is unavailable "
                    "unless the Field exists on the Record."
                );
            }
        };

        template<field::concepts::Field... OtherFields>
        class EqualsContract final {

            static constexpr bool kMatchingFields = utils::set::SetEqual<
                utils::set::Set<Fields...>,
                utils::set::Set<OtherFields...>
            >;

            static constexpr bool kAvailable = Traits::Equality::kAvailable;

        public:

            static constexpr bool kIsValid = kMatchingFields && kAvailable;
            static constexpr bool kIsNoexcept = Traits::Equality::kNoThrow;

            static constexpr void diagnose() noexcept {
                static_assert(
                    kAvailable,
                    "[record::equals] Helper is unavailable "
                    "unless all Fields are equality comparable"
                );
                static_assert(
                    kMatchingFields,
                    "[record::equals] Helper is unavailable "
                    "unless both Records have matching Field sets"
                );
            }
        };

        class CompareContract final {

            static constexpr bool kAvailable = Traits::ThreeWayComparison::kAvailable;

        public:

            static constexpr bool kIsValid = kAvailable;
            static constexpr bool kIsNoexcept = Traits::ThreeWayComparison::kNoThrow;

            static constexpr void diagnose() noexcept {
                static_assert(
                    kAvailable,
                    "[record::compare] Helper is unavailable "
                    "unless all Fields are three-way comparable"
                );
            }
        };

        template<field::concepts::Field... Selected>
        class ProjectContract final {

            static constexpr bool kAvailable = field::detail::traits::FieldTraits<
                Selected...
            >::CopyConstruction::kAvailable;
            static constexpr bool kHasSelected = Traits::template HasAll<Selected...>;

        public:

            static constexpr bool kIsValid = kAvailable && kHasSelected;
            static constexpr bool kIsNoexcept = Traits::CopyConstruction::kNoThrow;

            static constexpr void diagnose() noexcept {
                static_assert(
                    kAvailable,
                    "[record::project] Helper is unavailable "
                    "unless all Selected Fields are copy constructible"
                );
                static_assert(
                    kHasSelected,
                    "[record::project] Helper is unavailable "
                    "unless all Selected Fields are in the original Record"
                );
            }
        };

        template<field::concepts::Field... Selected>
        class ExtractContract final {

            static constexpr bool kAvailable = field::detail::traits::FieldTraits<
                Selected...
            >::MoveConstruction::kAvailable;
            static constexpr bool kHasSelected = Traits::template HasAll<Selected...>;

        public:

            static constexpr bool kIsValid = kAvailable && kHasSelected;
            static constexpr bool kIsNoexcept = Traits::MoveConstruction::kNoThrow;

            static constexpr void diagnose() noexcept {
                static_assert(
                    kAvailable,
                    "[record::extract] Helper is unavailable "
                    "unless all Selected Fields are move constructible"
                );
                static_assert(
                    kHasSelected,
                    "[record::extract] Helper is unavailable "
                    "unless all Selected Fields are in the original Record"
                );
            }
        };

    public:

        template<std::size_t I>
        using GetI = GetIContract<I>;

        template<field::concepts::Field F>
        using GetF = GetFContract<F>;

        template<field::concepts::Field... OtherFields>
        using Equals = EqualsContract<OtherFields...>;

        using Compare = CompareContract;

        template<field::concepts::Field... Selected>
        using Project = ProjectContract<Selected...>;

        template<field::concepts::Field... Selected>
        using Extract = ExtractContract<Selected...>;
    };
}

#endif //RECORD_INCLUDE_RECORD_DETAIL_CONTRACTS_HPP
