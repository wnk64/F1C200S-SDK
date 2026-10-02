#pragma once
#include <algorithm>
#if defined(__GNUC__) && __GNUC__ < 7
#include <experimental/string_view>
#else
#include <string_view>
#endif

namespace wiliwili {
#if defined(__GNUC__) && __GNUC__ < 7
using string_view = std::experimental::string_view;
#else
using string_view = std::string_view;
#endif
template <typename T> const T& clamp(const T& value, const T& low, const T& high) {
    return value < low ? low : high < value ? high : value;
}
}
