#pragma once
#include <concepts>
#include <cstddef>
#include <type_traits>
#include "compat/retained_string.hpp"
#include "common/shared_array.hpp"
#include "common/resource_key_map.hpp"

namespace nicxlive::core::serde {

// A producer belongs to an actual defining declaration and implements its real
// selected Source serialization branch. Missing fields or UDAs are not inferred
// from native layout, member counts, RTTI, or a successful serialization probe.
template<class T> struct SourceJsonFields;

// A defining Source string carrier supplies the real retained byte descriptor.
// Native view-like types without this declaration are not inferred as strings.
template<class T> struct SourceJsonString;
template<class T> struct SourceJsonArray;
template<class T> struct SourceJsonEnum;
template<class T> struct SourceJsonEnumKey;
template<class T> struct SourceJsonEnumMap;

template<class T>
concept DeclaredJsonEnumKey = std::is_enum_v<std::remove_cvref_t<T>> && requires(T value) {
    { SourceJsonEnumKey<std::remove_cvref_t<T>>::get(value) } -> std::same_as<d_runtime::RetainedString>;
};

template<DeclaredJsonEnumKey Key, class Value, class Hash, class Equal>
struct SourceJsonEnumMap<common::ResourceKeyMap<Key, Value, Hash, Equal>> {
    template<class Serializer>
    static void serialize(const common::ResourceKeyMap<Key, Value, Hash, Equal>& value, Serializer& serializer) {
        auto retained = value;
        if (retained.isNull()) { serializer.putValue(nullptr); return; }
        auto state = serializer.structBegin();
        for (auto iterator = retained.begin(); iterator != retained.end(); ++iterator) {
            auto& entry = *iterator;
            auto key = entry.first;
            auto text = SourceJsonEnumKey<std::remove_cv_t<Key>>::get(key);
            serializer.putEscapedKey(text.view());
            serializer.putValue(entry.second);
        }
        serializer.structEnd(state);
    }
};

template<class T, class Serializer>
concept DeclaredJsonEnumMap = requires(const T& value, Serializer& serializer) {
    { SourceJsonEnumMap<std::remove_cvref_t<T>>::serialize(value, serializer) } -> std::same_as<void>;
};

// Exact Source pointer carriers declare their real pointee. They must not use
// an ownership wrapper's unrelated native class-reference serialization path.
template<class T> struct SourceJsonPointer;

template<class T>
concept DeclaredSourcePointer = requires {
    typename SourceJsonPointer<std::remove_cvref_t<T>>::Pointee;
};

template<class T, class Serializer>
concept DeclaredJsonEnum = requires(T value, Serializer& serializer) {
    { SourceJsonEnum<std::remove_cvref_t<T>>::serialize(value, serializer) } -> std::same_as<void>;
};

template<class T>
concept JsonCharacter = std::same_as<std::remove_cv_t<T>, char> ||
    std::same_as<std::remove_cv_t<T>, wchar_t> || std::same_as<std::remove_cv_t<T>, char8_t> ||
    std::same_as<std::remove_cv_t<T>, char16_t> || std::same_as<std::remove_cv_t<T>, char32_t>;

template<class T>
concept JsonArrayElement = !std::is_void_v<std::remove_cv_t<T>> && !JsonCharacter<T>;

template<JsonArrayElement T> struct SourceJsonArray<common::SharedArray<T>> {
    template<class Serializer>
    static void serialize(const common::SharedArray<T>& value, Serializer& serializer) {
        auto retained = value;
        if (retained.isNull()) { serializer.putValue(nullptr); return; }
        auto state = serializer.listBegin();
        const auto length = retained.size();
        for (std::size_t index = 0; index < length; ++index) {
            auto& element = retained[index];
            serializer.elemBegin();
            serializer.serializeValue(element);
        }
        serializer.listEnd(state);
    }
};

template<class T, class Serializer>
concept DeclaredJsonArray = requires(const T& value, Serializer& serializer) {
    { SourceJsonArray<std::remove_cvref_t<T>>::serialize(value, serializer) } -> std::same_as<void>;
};

template<class T>
concept DeclaredJsonString = requires(const T& value) {
    { SourceJsonString<std::remove_cvref_t<T>>::read(value) } -> std::same_as<d_runtime::RetainedString>;
};

template<class T, class Serializer>
concept DeclaredJsonSerializable = requires(T& value, Serializer& serializer) {
    { SourceJsonFields<std::remove_cvref_t<T>>::serialize(value, serializer) } -> std::same_as<void>;
};

} // namespace nicxlive::core::serde
