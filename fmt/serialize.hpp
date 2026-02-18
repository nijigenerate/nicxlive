#pragma once

#include "../serde.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <sstream>
#include <string>
#include <memory>
#include <type_traits>

namespace nicxlive::core::fmt {

// D parity markers (no reflection behavior in C++).
struct Ignore final {};
struct Optional final {};
struct Name final {
    std::string jsonKey{};
    std::string displayName{};
    Name() = default;
    explicit Name(std::string key) : jsonKey(std::move(key)), displayName(jsonKey) {}
    Name(std::string key, std::string display) : jsonKey(std::move(key)), displayName(std::move(display)) {}
};

class ISerializable {
public:
    virtual ~ISerializable() = default;
    virtual void serialize(serde::InochiSerializer& serializer) const = 0;
};

template <typename T>
struct IDeserializable {
    static std::shared_ptr<T> deserialize(const serde::Fghj& data) {
        if constexpr (requires(const serde::Fghj& pt) { T::deserialize(pt); }) {
            return T::deserialize(data);
        } else {
            auto obj = std::make_shared<T>();
            if constexpr (requires(T t) { t.deserializeFromFghj(data); }) {
                obj->deserializeFromFghj(data);
            }
            return obj;
        }
    }
};

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
    return IDeserializable<T>::deserialize(pt);
}

template <typename T>
inline std::string inToJson(const T& item) {
    std::stringstream ss;
    if constexpr (std::is_base_of_v<ISerializable, T>) {
        nicxlive::core::serde::InochiSerializer serializer;
        static_cast<const ISerializable&>(item).serialize(serializer);
        boost::property_tree::write_json(ss, serializer.root, false);
    } else if constexpr (requires(const T& t, nicxlive::core::serde::InochiSerializer& s) { t.serialize(s, true, {}); }) {
        nicxlive::core::serde::InochiSerializer serializer;
        const_cast<T&>(item).serialize(serializer, true, {});
        boost::property_tree::write_json(ss, serializer.root, false);
    }
    return ss.str();
}

template <typename T>
inline std::string inToJsonPretty(const T& item) {
    std::stringstream ss;
    if constexpr (std::is_base_of_v<ISerializable, T>) {
        nicxlive::core::serde::InochiSerializer serializer;
        static_cast<const ISerializable&>(item).serialize(serializer);
        boost::property_tree::write_json(ss, serializer.root, true);
    } else if constexpr (requires(const T& t, nicxlive::core::serde::InochiSerializer& s) { t.serialize(s, true, {}); }) {
        nicxlive::core::serde::InochiSerializer serializer;
        const_cast<T&>(item).serialize(serializer, true, {});
        boost::property_tree::write_json(ss, serializer.root, true);
    }
    return ss.str();
}

} // namespace nicxlive::core::fmt
