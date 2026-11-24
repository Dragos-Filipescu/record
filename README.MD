# Record

A heterogeneous record container for C++26 with compile-time layout optimization.

`record::Record<Fields...>` stores one value per field. Storage is reordered by alignment to reduce padding, and fields are constructed, accessed, compared and projected by field type or declaration index.

## Example

```cpp
#include <record/record.hpp>

#include <string>

struct Name : record::RecordField<std::string> {};
struct Age  : record::RecordField<int> {};

// Bindings may be supplied in any order
auto user = record::Record<Name, Age>{
    record::Bind<Age>(25),
    record::Bind<Name>("Alice"),
};

auto& name = record::get<Name>(user); // "Alice"
auto& age  = record::get<1>(user);    // 25 (declaration index)
```

## Defining fields

A field is a type with a `StoredType` alias. Inherit from `RecordField` to get the defaults:

```cpp
struct Name : record::RecordField<std::string> {};
```

Fields must be distinct types, so users should inherit rather than alias. `using A = RecordField<int>;` and `using B = RecordField<int>;` name the same type, and a `Record` rejects them as duplicates.

Optionally, a field may provide `Alignment`, `EqualTo` and `CompareThreeWay` to override the defaults. `RecordField` supplies all three.

## Features

- **Layout optimization:** Fields are stored in alignment-descending order to reduce padding.
- **Order-agnostic construction:** `Bind<F>(args...)` pairs a field with its constructor arguments. Each field must be bound exactly once.
- **Explicit comparison semantics:** `equals` tests whether all fields are equal and does not depend on field order. `compare` is lexicographic in declaration order, so `Record<A, B>` and `Record<B, A>` are distinct types that can order the same values differently.
- **Readable diagnostics:** Invalid use, such as a missing binding, an absent field, or an operation the field types don't support, fails with a `static_assert` naming the problem.
- **Projection and extraction:** `project<Fields...>(r)` copies a subset into a new `Record`. `extract<Fields...>(std::move(r))` moves a subset out, leaving the selected fields in an unspecified state.
- **Type reflection:** `Record<...>::Rebind<Target>::AsDeclared` and `::AsStored` rebind the field pack to any template, such as `std::tuple`.

## Requirements

- C++26 or later
- CMake 3.x

## Building

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

## Compiler support

Tested with GCC 15 and 16, and Clang 20 on Linux.
