#ifndef RECORD_TESTS_TEST_GET_HPP
#define RECORD_TESTS_TEST_GET_HPP

#include <cassert>
#include <iostream>
#include <string>
#include <utility>

#include <record/record.hpp>

namespace record::tests {

    namespace fixtures {
        struct Name   : field::RecordField<std::string> {};
        struct Age    : field::RecordField<int> {};
        struct Height : field::RecordField<double> {};
        struct Flag   : field::RecordField<bool> {};
    }

    namespace fields {

        void test_traits() {
            using Traits = field::detail::traits::FieldTraits<Name, Age>;

            static_assert(Traits::kTriviallyCopyable == false); // std::string is not trivially copyable
            static_assert(Traits::CopyConstruction::kTrivial == false);
            static_assert(Traits::MoveConstruction::kTrivial == false);
            static_assert(Traits::Destruction::kTrivial == false);

            static_assert(Traits::template Has<Name> == true);
            static_assert(Traits::template Has<Height> == false);

            std::cout << "  [OK] fields::test_traits\n";
        }

        void test_layout() {

        }
    }

    namespace record {

        namespace special_member_functions {
            void test_defaultCtor() {

            }

            void test_BindCtor() {

            }

            void test_CopyCtor() {

            }

            void test_CopyAssignmentOp() {

            }

            void test_MoveCtor() {

            }

            void test_MoveAssingmentOp() {

            }
        }

        namespace helper_friends {
            void test_get() {

            }

            void test_equals() {

            }

            void test_compare() {

            }

            void test_project() {

            }

            void test_extract() {

            }
        }
    }
}

#endif //RECORD_TESTS_TEST_GET_HPP
