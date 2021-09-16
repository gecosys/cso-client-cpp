#ifndef CSO_UTILS_STRING_HPP
#define CSO_UTILS_STRING_HPP

#include <memory>
#include <string>
#include <stdexcept>

class UtilsString {
public:
    template<typename ... Args>
    static std::string format(const std::string& format, Args&& ... args) {
        size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1ULL; // Extra space for '\0'
        if (size <= 0) {
            throw "Error during formatting.";
        }
        std::unique_ptr<char> buf{ new char[size] };
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        return std::string{ buf.get(), buf.get() + size - 1 }; // We don't want the '\0' inside
    }
};

#endif // !CSO_UTILS_STRING_HPP