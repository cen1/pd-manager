#ifndef COMMON_UTIL_H
#define COMMON_UTIL_H

#include <string>

using namespace std;

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template<Numeric T>
std::string UTIL_ToString(T value)
{
	return std::to_string(value);
}

#endif // COMMON_UTIL_H