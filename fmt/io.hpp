#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace nicxlive::core::fmt {

// Minimal big-endian file helpers mirroring fmt/io.d
template <typename T>
inline T readValue(std::ifstream& file) {
    std::vector<uint8_t> buf(sizeof(T));
    file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    T value = 0;
    for (auto b : buf) value = static_cast<T>((value << 8) | b);
    return value;
}

template <typename T>
inline T peekValue(std::ifstream& file) {
    auto pos = file.tellg();
    T v = readValue<T>(file);
    file.seekg(pos);
    return v;
}

inline std::string readStr(std::ifstream& file, std::size_t length) {
    std::string s(length, '\0');
    file.read(s.data(), static_cast<std::streamsize>(length));
    return s;
}

inline std::string peekStr(std::ifstream& file, std::size_t length) {
    auto pos = file.tellg();
    auto s = readStr(file, length);
    file.seekg(pos);
    return s;
}

inline std::vector<uint8_t> read(std::ifstream& file, std::size_t length) {
    std::vector<uint8_t> buf(length);
    file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(length));
    return buf;
}

inline std::vector<uint8_t> peek(std::ifstream& file, std::size_t length) {
    auto pos = file.tellg();
    auto buf = read(file, length);
    file.seekg(pos);
    return buf;
}

inline void skip(std::ifstream& file, std::ptrdiff_t length) {
    file.seekg(length, std::ios::cur);
}

} // namespace nicxlive::core::fmt
