#ifndef COMMON_UTIL_H
#define COMMON_UTIL_H

#include <string>
#include <vector>

using namespace std;

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template<Numeric T>
std::string UTIL_ToString(T value)
{
	return std::to_string(value);
}

vector<string> UTIL_Tokenize(string s, char delim);

#endif // COMMON_UTIL_H
