#pragma once

#include "../serde.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <sstream>
#include <string>
#include <memory>

namespace nicxlive::core::fmt {

// Attribute aliases for parity with D
using Ignore = int;
using Optional = int;
using Name = int;

template <typename T>
inline std::shared_ptr<T> inLoadJsonDataFromMemory(const std::string& data);

template <typename T>
inline std::shared_ptr<T> inLoadJsonData(const std::string& file) {
    std::ifstream ifs(file);
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return inLoadJsonDataFromMemory<T>(buffer.str());
}

template <typename T>
inline std::shared_ptr<T> inLoadJsonDataFromMemory(const std::string& data) {
    std::stringstream ss(data);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);
    auto obj = std::make_shared<T>();
    if constexpr (requires(T t) { t.deserializeFromFghj(pt); }) {
        obj->deserializeFromFghj(pt);
    }
    return obj;
}

template <typename T>
inline std::string inToJson(const T& item) {
    std::stringstream ss;
    if constexpr (requires(const T& t, nicxlive::core::serde::InochiSerializer& s) { t.serialize(s, true, {}); }) {
        nicxlive::core::serde::InochiSerializer serializer;
        const_cast<T&>(item).serialize(serializer, true, {});
        boost::property_tree::write_json(ss, serializer.root, false);
    }
    return ss.str();
}

template <typename T>
inline std::string inToJsonPretty(const T& item) {
    std::stringstream ss;
    if constexpr (requires(const T& t, nicxlive::core::serde::InochiSerializer& s) { t.serialize(s, true, {}); }) {
        nicxlive::core::serde::InochiSerializer serializer;
        const_cast<T&>(item).serialize(serializer, true, {});
        boost::property_tree::write_json(ss, serializer.root, true);
    }
    return ss.str();
}

} // namespace nicxlive::core::fmt
