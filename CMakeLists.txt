cmake_minimum_required(VERSION 3.30)
project(record LANGUAGES CXX)

# ==============================================================================
# Build Type Configuration
# ==============================================================================

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g3 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# Header-Only Library
# ==============================================================================

add_library(${PROJECT_NAME} INTERFACE
        include/record/detail/contracts.hpp
        include/record/detail/utils.hpp
        include/record/field.hpp
        include/record/record.hpp
        include/record/detail/bind.hpp
        include/record/detail/enumerate.hpp
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_26)

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ==============================================================================
# Tests
# ==============================================================================

option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
    enable_testing()

    add_executable(${PROJECT_NAME}_tests tests/main.cpp
            tests/test_get.hpp)

    target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${PROJECT_NAME})

    # Apply the same warning flags to both GCC and Clang
    target_compile_options(${PROJECT_NAME}_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wcast-align
    )

    # Clang-specific stdlib configuration
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -stdlib=libstdc++)
        target_link_options(${PROJECT_NAME}_tests PRIVATE -stdlib=libstdc++)
    endif()

    add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
    doxygen_add_docs(
            docs
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            COMMENT "Generating API documentation with Doxygen"
    )
endif()
