#pragma once

#include <serde.hpp>
#include <serialization_interface.hpp>
#include <serde_json.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

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

template<class T>
inline std::string inToJson(const common::StringKeyMap<T>& item) {
    serde::TypedJsonWriter serializer;
    serializer.serializeValue(item);
    serializer.flush();
    return std::move(serializer).takeString();
}

template<class T, class Hash, class Equal>
inline std::string inToJson(const common::ResourceKeyMap<d_runtime::RetainedString, T, Hash, Equal>& item) {
    serde::TypedJsonWriter serializer;
    serializer.serializeValue(item);
    serializer.flush();
    return std::move(serializer).takeString();
}

template<CustomSerializable T>
inline std::string inToJson(T& item) {
    serde::TypedJsonWriter serializer;
    serializer.serializeValue(item);
    serializer.flush();
    return std::move(serializer).takeString();
}

template<class T> requires CustomSerializable<std::remove_cv_t<T>>
inline std::string inToJson(const std::shared_ptr<T>& item) {
    serde::TypedJsonWriter serializer;
    serializer.serializeValue(item);
    serializer.flush();
    return std::move(serializer).takeString();
}

template<class T> requires (!CustomSerializable<std::remove_cv_t<T>> &&
    d_runtime::detail::sourceNativeObjectOrigin<std::remove_cv_t<T>> &&
    serde::DeclaredJsonSerializable<T, serde::TypedJsonWriter>)
inline std::string inToJson(const std::shared_ptr<T>& item) {
    serde::TypedJsonWriter serializer;
    serializer.serializeValue(item);
    serializer.flush();
    return std::move(serializer).takeString();
}

template<typename T> requires (!CustomSerializable<T>)
inline std::string inToJson(const T& item) {
    using Value = std::remove_cvref_t<T>;
    if constexpr (serde::DeclaredSourcePointer<T>) {
        serde::TypedJsonWriter serializer;
        serializer.serializeValue(item);
        serializer.flush();
        return std::move(serializer).takeString();
    } else if constexpr (CustomSerializable<Value> ||
        (serde::DeclaredJsonSerializable<T, serde::TypedJsonWriter> &&
            d_runtime::detail::sourceNativeObjectOrigin<Value>)) {
        serde::TypedJsonWriter serializer;
        serializer.serializeValue(item);
        serializer.flush();
        return std::move(serializer).takeString();
    } else if constexpr (std::same_as<Value, std::string>) {
        // Source strings share immutable bytes. This synchronous primitive
        // call does not need an extra native string copy or retained owner.
        serde::TypedJsonWriter serializer;
        serializer.serializeValue(item);
        serializer.flush();
        return std::move(serializer).takeString();
    } else if constexpr (serde::DeclaredJsonSerializable<T, serde::TypedJsonWriter> ||
        serde::DeclaredJsonArray<T, serde::TypedJsonWriter> ||
        serde::DeclaredJsonEnum<T, serde::TypedJsonWriter> ||
        serde::DeclaredJsonEnumMap<T, serde::TypedJsonWriter> ||
        serde::DeclaredJsonString<T> || std::same_as<Value, std::nullptr_t> ||
        std::same_as<Value, d_runtime::RetainedString> ||
        std::same_as<Value, common::NullableString> || std::same_as<Value, float> || std::same_as<Value, double> ||
        (std::integral<Value> && !std::same_as<Value, char> && !std::same_as<Value, wchar_t> &&
            !std::same_as<Value, char8_t> && !std::same_as<Value, char16_t> && !std::same_as<Value, char32_t>)) {
        auto value = item;
        serde::TypedJsonWriter serializer;
        serializer.serializeValue(value);
        serializer.flush();
        return std::move(serializer).takeString();
    } else {
        std::stringstream ss;
        if constexpr (requires(const T& t, nicxlive::core::serde::InochiSerializer& s) { t.serialize(s, true, {}); }) {
            nicxlive::core::serde::InochiSerializer serializer;
            const_cast<T&>(item).serialize(serializer, true, {});
            boost::property_tree::write_json(ss, serializer.root, false);
        }
        return ss.str();
    }
}

template<CustomSerializable T>
inline std::string inToJsonPretty(T& item) {
    std::stringstream ss;
    serde::InochiSerializer serializer;
    ngSerializeCustom(item, serializer);
    boost::property_tree::write_json(ss, serializer.root, true);
    return ss.str();
}

template<typename T> requires (!CustomSerializable<T>)
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
