#pragma once
#include <array>
#include <cstddef>

// Generic helper to map enum values to string literals safely.
// Usage:
//   enum class MyEnum { A, B, MAX };
//   ENUM_TO_STRING(MyEnum, "A", "B");
// This defines `const char* ToString(MyEnum)` that returns the
// corresponding string or "Unknown" if the value is out of range.


template <typename Enum, typename Array>
constexpr const char* EnumToString(Enum value, const Array& names) {
    std::size_t index = static_cast<std::size_t>(value);
    if (index < names.size()) {
        return names[index];
    }
    return "Unknown";
}

#define ENUM_TO_STRING(EnumType, ...)                                       \
    inline const char* ToString(EnumType value) {                           \
        static constexpr std::array names{ __VA_ARGS__ };                   \
        return EnumToString(value, names);                                  \
    }