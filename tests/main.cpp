#include <record/record.hpp>
#include <cassert>
#include <iostream>
#include <string>
#include <utility>

namespace record::tests {

    // ==========================================================================
    // Shared Test Fixtures
    // ==========================================================================

    namespace fixtures {
        struct Name   : field::RecordField<std::string> {};
        struct Age    : field::RecordField<int> {};
        struct Height : field::RecordField<double> {};
        struct Flag   : field::RecordField<bool> {};
    }

    using namespace fixtures;

    // ==========================================================================
    // Fields
    // ==========================================================================

    namespace fields {

        void test_traits() {
            std::cout << "  fields::test_traits...[START]\n";
            using Traits = field::detail::traits::FieldTraits<Name, Age>;

            static_assert(Traits::kTriviallyCopyable == false); // std::string is not trivially copyable
            static_assert(Traits::CopyConstruction::kTrivial == false);
            static_assert(Traits::MoveConstruction::kTrivial == false);
            static_assert(Traits::Destruction::kTrivial == false);

            using TrivialTraits = field::detail::traits::FieldTraits<Age, Height, Flag>;

            static_assert(TrivialTraits::kTriviallyCopyable == true);
            static_assert(TrivialTraits::CopyConstruction::kTrivial == true);
            static_assert(TrivialTraits::MoveConstruction::kTrivial == true);
            static_assert(TrivialTraits::Destruction::kTrivial == true);

            static_assert(Traits::Has<Name> == true);
            static_assert(Traits::Has<Height> == false);

            static_assert(Traits::HasAll<Name, Age> == true);
            static_assert(Traits::HasAll<Name, Age, Height> == false);

            static_assert(Traits::HasAny<Name, Age, Height> == true);
            static_assert(Traits::HasAny<Height, Flag> == false);

            static_assert(Traits::HasNone<Height, Flag> == true);
            static_assert(Traits::HasNone<Name, Age, Height> == false);

            std::cout << "  fields::test_traits...[OK]\n";
        }

        void test_layout() {
            std::cout << "  fields::test_layout...[START]\n";
            // Record<Flag, Age, Height, Name>
            // Flag(1) < Age(4) < Height(8) <= Name(32, align 8)
            using Layout = field::detail::layout::FieldLayout<Flag, Age, Height, Name>;

            // Check that offsets are unique and within bounds
            for (std::size_t i = 0; i < Layout::kFieldCount; ++i) {
                assert(Layout::kOffsets[i] < Layout::kAlignedSize);
                for (std::size_t j = i + 1; j < Layout::kFieldCount; ++j) {
                    assert(Layout::kOffsets[i] != Layout::kOffsets[j]);
                }
            }

            // Aligned size must be a multiple of max alignment
            assert(Layout::kAlignedSize % alignof(std::string) == 0);

            // Packed size is <= aligned size
            assert(Layout::kPackedSize <= Layout::kAlignedSize);

            std::cout << "  fields::test_layout...[OK]\n";
        }
    }

    // ==========================================================================
    // Record
    // ==========================================================================

    namespace record {

        namespace special_member_functions {

            void test_defaultCtor() {
                std::cout << "  special_member_functions::test_defaultCtor...[START]\n";
                using R = Record<Age, Flag>;
                R r{};
                assert(get<Age>(r) == 0);
                assert(get<Flag>(r) == false);
                std::cout << "  special_member_functions::test_defaultCtor...[OK]\n";
            }

            void test_BindCtor() {
                std::cout << "  special_member_functions::test_BindCtor...[START]\n";
                using R = Record<Name, Age>;
                R r{
                    Bind<Age>(25),
                    Bind<Name>("Alice")
                };
                const auto& name = get<Name>(r);
                std::cout << "Name: " << name << '\n';
                const auto& age = get<Age>(r);
                std::cout << "Age: " << age << '\n';
                assert(name == "Alice");
                assert(age == 25);
                std::cout << "  special_member_functions::test_BindCtor...[OK]\n";
            }

            void test_CopyCtor() {
                std::cout << "  special_member_functions::test_CopyCtor...[START]\n";
                using R = Record<Name, Age>;
                R a{ Bind<Name>("Bob"), Bind<Age>(30) };
                R b = a;
                assert(get<Name>(a) == "Bob");
                assert(get<Name>(b) == "Bob");
                assert(get<Age>(b) == 30);
                std::cout << "  special_member_functions::test_CopyCtor...[OK]\n";
            }

            void test_CopyAssignmentOp() {
                std::cout << "  special_member_functions::test_CopyAssignmentOp...[START]\n";
                using R = Record<Name, Age>;
                R a{ Bind<Name>("Charlie"), Bind<Age>(40) };
                R b{ Bind<Name>("Dave"), Bind<Age>(50) };
                b = a;
                assert(get<Name>(b) == "Charlie");
                assert(get<Age>(b) == 40);
                std::cout << "  special_member_functions::test_CopyAssignmentOp...[OK]\n";
            }

            void test_MoveCtor() {
                std::cout << "  special_member_functions::test_MoveCtor...[START]\n";
                using R = Record<Name, Age>;
                R a{ Bind<Name>("Eve"), Bind<Age>(60) };
                R b = std::move(a);
                assert(get<Name>(b) == "Eve");
                assert(get<Age>(b) == 60);
                std::cout << "  special_member_functions::test_MoveCtor...[OK]\n";
            }

            void test_MoveAssingmentOp() {
                std::cout << "  special_member_functions::test_MoveAssingmentOp...[START]\n";
                using R = Record<Name, Age>;
                R a{ Bind<Name>("Frank"), Bind<Age>(70) };
                R b{ Bind<Name>("Grace"), Bind<Age>(80) };
                b = std::move(a);
                assert(get<Name>(b) == "Frank");
                assert(get<Age>(b) == 70);
                std::cout << "  special_member_functions::test_MoveAssingmentOp...[OK]\n";
            }
        }

        namespace helper_friends {

            void test_get() {
                std::cout << "  helper_friends::test_get...[START]\n";
                using R = Record<Name, Age, Height>;
                R r{
                    Bind<Name>("Alice"),
                    Bind<Age>(25),
                    Bind<Height>(168.5)
                };
                assert(get<Name>(r) == "Alice");
                assert(get<0>(r) == "Alice"); // declaration index 0
                assert(get<2>(r) == 168.5);  // declaration index 2

                get<Age>(r) = 26;
                assert(get<Age>(r) == 26);

                // Const get
                const R& cr = r;
                assert(get<Name>(cr) == "Alice");
                std::cout << "  helper_friends::test_get...[OK]\n";
            }

            void test_equals() {
                std::cout << "  helper_friends::test_equals...[START]\n";
                using R = Record<Name, Age>;
                R a{ Bind<Name>("Alice"), Bind<Age>(25) };
                R b{ Bind<Name>("Alice"), Bind<Age>(25) };
                R c{ Bind<Name>("Alice"), Bind<Age>(26) };

                assert(equals(a, a) == true);
                assert(equals(a, b) == true);
                assert(equals(a, c) == false);

                // Cross-type comparison (same fields, different declaration order)
                Record<Age, Name> reversed{
                    Bind<Age>(25),
                    Bind<Name>("Alice")
                };
                assert(equals(a, reversed) == true);
                std::cout << "  helper_friends::test_equals...[OK]\n";
            }

            void test_compare() {
                std::cout << "  helper_friends::test_compare...[START]\n";
                using R = Record<Name, Age>;
                R a{ Bind<Name>("Alice"), Bind<Age>(25) };
                R b{ Bind<Name>("Alice"), Bind<Age>(26) };

                assert(compare(a, a) == 0);
                assert(compare(a, b) < 0);
                assert(compare(b, a) > 0);
                std::cout << "  helper_friends::test_compare...[OK]\n";
            }

            void test_project() {
                std::cout << "  helper_friends::test_project...[START]\n";
                using Full = Record<Name, Age, Height>;
                Full full{
                    Bind<Name>("Alice"),
                    Bind<Age>(25),
                    Bind<Height>(168.5)
                };

                auto projected = project<Name, Age>(full);
                static_assert(std::is_same_v<decltype(projected), Record<Name, Age>>);
                assert(get<Name>(projected) == "Alice");
                assert(get<Age>(projected) == 25);

                // Verify independence: modifying projected doesn't affect full
                get<Age>(projected) = 26;
                assert(get<Age>(full) == 25);
                std::cout << "  helper_friends::test_project...[OK]\n";
            }

            void test_extract() {
                std::cout << "  helper_friends::test_extract...[START]\n";
                using Full = Record<Name, Age, Height>;
                Full full{
                    Bind<Name>("Alice"),
                    Bind<Age>(25),
                    Bind<Height>(168.5)
                };

                auto extracted = extract<Name>(std::move(full));
                static_assert(std::is_same_v<decltype(extracted), Record<Name>>);
                assert(get<Name>(extracted) == "Alice");

                // The un-extracted fields of `full` should be intact
                assert(get<Age>(full) == 25);
                assert(get<Height>(full) == 168.5);
                std::cout << "  helper_friends::test_extract...[OK]\n";
            }
        }
    }
}

int main() {
    std::cout << "Running Record tests...\n";

    std::cout << "\n[fields]\n";
    record::tests::fields::test_traits();
    record::tests::fields::test_layout();

    std::cout << "\n[special_member_functions]\n";
    record::tests::record::special_member_functions::test_defaultCtor();
    record::tests::record::special_member_functions::test_BindCtor();
    record::tests::record::special_member_functions::test_CopyCtor();
    record::tests::record::special_member_functions::test_CopyAssignmentOp();
    record::tests::record::special_member_functions::test_MoveCtor();
    record::tests::record::special_member_functions::test_MoveAssingmentOp();

    std::cout << "\n[helper_friends]\n";
    record::tests::record::helper_friends::test_get();
    record::tests::record::helper_friends::test_equals();
    record::tests::record::helper_friends::test_compare();
    record::tests::record::helper_friends::test_project();
    record::tests::record::helper_friends::test_extract();

    std::cout << "\nAll tests passed.\n";
    return 0;
}