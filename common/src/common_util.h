#ifndef COMMON_UTIL_H
#define COMMON_UTIL_H

#include <string>
#include <sstream>

using namespace std;

template<typename T>
std::enable_if_t<std::is_integral_v<T>, std::string>
UTIL_ToString(T value) {
    string result;
    stringstream SS;
    SS << value;
    SS >> result;
    return result;
}

#endif // COMMON_UTIL_H