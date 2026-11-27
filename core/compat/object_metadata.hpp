#pragma once
#include "retained_string.hpp"
#include "system_error.hpp"
#include <cstddef>
#include <optional>
#include <memory>
#include <new>
#include <source_location>
#include <string>
#include <string_view>
#include <stdexcept>
#include <typeinfo>
#include <type_traits>
#include <utility>

namespace nicxlive::core {
class Puppet;
namespace nodes {
class Node;
class Part;
class PathDeformer;
class SimplePhysicsDriver;
class Composite;
class Drawable;
class GridDeformer;
class MeshGroup;
}
namespace param {
class Parameter;
class ParameterBinding;
class ValueParameterBinding;
class DeformationParameterBinding;
class ParameterParameterBinding;
struct DeformSlot;
}
namespace common {
template<class T> class SharedArray;
template<class K, class V, class Hash, class Equal> class ResourceKeyMap;
}
}

namespace nicxlive::core::d_runtime {
// Process-wide source ClassInfo names. RTTI identifies a native type internally;
// its implementation-dependent spelling is never exposed as a source name.
void ngRegisterObjectClassName(const std::type_info& type, RetainedString name);
std::optional<RetainedString> ngFindObjectClassName(const std::type_info& type);
RetainedString ngObjectClassName(const std::type_info& type);

// Register both independent spellings from the original source declaration.
// A template type argument can name an eponymous alias instead of the class
// leaf in ClassInfo.name. Its spelling includes the selected type printer's
// argument syntax; never infer it from RTTI or the registered ClassInfo name.
// Existing names are immutable. Conflicts reject the whole registration.
void ngRegisterObjectTypeNames(const std::type_info& type, RetainedString className,
                               RetainedString templateArgumentName);
std::optional<RetainedString> ngFindObjectTemplateArgumentName(const std::type_info& type);
RetainedString ngObjectTemplateArgumentName(const std::type_info& type);

// Value types have a Source type-argument spelling without an Object ClassInfo.
// Register that independent origin in the same metadata row, never a fake class.
void ngRegisterSourceTypeArgumentName(const std::type_info& type, RetainedString name);

// These are distinct selected-printer contexts, not interchangeable strings.
enum class SourceTypeSpelling {
    QualifiedArgument,
    UnqualifiedArgument,
    QualifiedElement,
    UnqualifiedElement,
};
struct SourceTypeSpellingNames {
    RetainedString qualifiedArgument;
    RetainedString unqualifiedArgument;
    RetainedString qualifiedElement;
    RetainedString unqualifiedElement;
};
void ngRegisterSourceTypeSpellings(const std::type_info& type, SourceTypeSpellingNames names);
std::optional<RetainedString> ngFindSourceTypeSpelling(
    const std::type_info& type, SourceTypeSpelling context);
RetainedString ngSourceTypeSpelling(const std::type_info& type, SourceTypeSpelling context);
template<class T> struct SourceTypeSpellings;

// Source declarations supply independent literal arrays or metadata functions
// returning retained names. Functions support genuine class-template records.
// This open trait also supports class templates and eponymous Source aliases;
// no record is synthesized from an unqualified class or a native type name.
template<class T> struct SourceObjectTypeNames;

// Open declaration metadata for non-Object types. argumentName can be an exact
// literal array or a function returning the retained selected-printer spelling.
// Object declarations continue to supply their independent existing name pair.
template<class T> struct SourceTypeArgumentName;

// A counted char StringExp value, not a type spelling or a native type name.
// Preserve explicit lexical-origin flags when the defining declaration has them.
// DMD v2.111.0 hdrgen.visitString / tokens.writeCharLiteral, D Language
// Foundation 1999-2025, BSL-1.0; selected compiler's initial C print locale.
inline RetainedString ngSourceStringValueArgument(std::string_view bytes,
        char postfix = '\0', bool hexadecimal = false) {
    try {
        constexpr char lowerHex[] = "0123456789abcdef";
        constexpr char upperHex[] = "0123456789ABCDEF";
        std::string result;
        if (hexadecimal) result += 'x';
        result += '"';
        for (const unsigned char byte : bytes) {
            if (hexadecimal) {
                // hdrgen uses minimum width sz (one for char), not 2 * sz.
                if (byte >= 0x10) result += upperHex[byte >> 4];
                result += upperHex[byte & 15];
                continue;
            }
            switch (byte) {
            case 0: result += "\\0"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            default:
                if (byte >= 0x20 && byte <= 0x7e) result += static_cast<char>(byte);
                else {
                    result += "\\x";
                    result += lowerHex[byte >> 4];
                    result += lowerHex[byte & 15];
                }
            }
        }
        result += '"';
        if (postfix) result += postfix;
        return RetainedString(std::move(result));
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}

// Actual Source primitive and immutable-string carrier origins, not a list of
// valid template arguments. Other declarations use the same open trait/API.
template<> struct SourceTypeArgumentName<float> {
    inline static constexpr char argumentName[] = "float";
};
template<> struct SourceTypeArgumentName<bool> {
    inline static constexpr char argumentName[] = "bool";
};
template<> struct SourceTypeArgumentName<RetainedString> {
    inline static constexpr char argumentName[] = "string";
};
template<> struct SourceTypeArgumentName<common::NullableString> {
    inline static constexpr char argumentName[] = "string";
};
template<> struct SourceTypeArgumentName<std::string> {
    inline static constexpr char argumentName[] = "string";
};

// A metadata type key has no instance. It preserves T's qualifiers, which
// typeid(T) alone erases, without changing dynamic Object/ClassInfo identities.
template<class T> struct SourceObjectTypeIdentity {};

template<class T> void ngRegisterSourceTypeSpellings(SourceTypeSpellingNames names) {
    ngRegisterSourceTypeSpellings(typeid(SourceObjectTypeIdentity<T>), std::move(names));
}
template<class T> void ngPrepareSourceTypeSpellings() {
    if constexpr (requires { SourceTypeSpellings<T>::get(); })
        ngRegisterSourceTypeSpellings<T>(SourceTypeSpellings<T>::get());
    else if constexpr (requires {
        SourceTypeSpellings<T>::qualifiedArgument;
        SourceTypeSpellings<T>::unqualifiedArgument;
        SourceTypeSpellings<T>::qualifiedElement;
        SourceTypeSpellings<T>::unqualifiedElement;
    })
        ngRegisterSourceTypeSpellings<T>({
            RetainedString::fromStaticLiteral(SourceTypeSpellings<T>::qualifiedArgument),
            RetainedString::fromStaticLiteral(SourceTypeSpellings<T>::unqualifiedArgument),
            RetainedString::fromStaticLiteral(SourceTypeSpellings<T>::qualifiedElement),
            RetainedString::fromStaticLiteral(SourceTypeSpellings<T>::unqualifiedElement)});
}

template<class T> void ngRegisterSourceTypeArgumentName(RetainedString name) {
    ngRegisterSourceTypeArgumentName(typeid(SourceObjectTypeIdentity<T>), std::move(name));
}

template<class T> void ngRegisterObjectTypeNames(RetainedString className,
                                               RetainedString templateArgumentName) {
    ngRegisterObjectTypeNames(typeid(SourceObjectTypeIdentity<T>), className, templateArgumentName);
}

template<class T> RetainedString ngObjectTemplateArgumentName() {
    ngPrepareSourceTypeSpellings<T>();
    if constexpr (requires {
        SourceObjectTypeNames<T>::className();
        SourceObjectTypeNames<T>::templateArgumentName();
    }) {
        const auto className = SourceObjectTypeNames<T>::className();
        const auto argumentName = SourceObjectTypeNames<T>::templateArgumentName();
        ngRegisterObjectTypeNames<T>(className, argumentName);
    } else if constexpr (requires {
        SourceObjectTypeNames<T>::className;
        SourceObjectTypeNames<T>::templateArgumentName;
    }) {
        ngRegisterObjectTypeNames<T>(
            RetainedString::fromStaticLiteral(SourceObjectTypeNames<T>::className),
            RetainedString::fromStaticLiteral(SourceObjectTypeNames<T>::templateArgumentName));
    } else if constexpr (requires { SourceTypeArgumentName<T>::argumentName(); }) {
        ngRegisterSourceTypeArgumentName<T>(SourceTypeArgumentName<T>::argumentName());
    } else if constexpr (requires { SourceTypeArgumentName<T>::argumentName; }) {
        ngRegisterSourceTypeArgumentName<T>(
            RetainedString::fromStaticLiteral(SourceTypeArgumentName<T>::argumentName));
    }
    // Explicit typed registration also supports declarations without a trait.
    // Missing exact metadata remains an error; there is no ClassInfo fallback.
    return ngObjectTemplateArgumentName(typeid(SourceObjectTypeIdentity<T>));
}

template<class T> RetainedString ngSourceTypeSpelling(SourceTypeSpelling context) {
    if (context != SourceTypeSpelling::QualifiedArgument && context != SourceTypeSpelling::UnqualifiedArgument &&
        context != SourceTypeSpelling::QualifiedElement && context != SourceTypeSpelling::UnqualifiedElement)
        throw std::invalid_argument("Invalid Source type spelling context");
    if (context == SourceTypeSpelling::QualifiedArgument) return ngObjectTemplateArgumentName<T>();
    ngPrepareSourceTypeSpellings<T>();
    return ngSourceTypeSpelling(typeid(SourceObjectTypeIdentity<T>), context);
}

namespace detail {
inline SourceTypeSpellingNames sourcePrimitiveTypeSpellings(RetainedString name) {
    return {name, name, name, name};
}
template<std::size_t N> struct SourceBasicNameLiteral {
    char bytes[N];
    consteval SourceBasicNameLiteral(const char (&text)[N]) {
        for (std::size_t i = 0; i < N; ++i) bytes[i] = text[i];
    }
};

template<class T> consteval auto sourceNativeScalarLiteral() {
    if constexpr (std::is_void_v<T>) return SourceBasicNameLiteral{"void"};
    else if constexpr (std::is_same_v<T, bool>) return SourceBasicNameLiteral{"bool"};
    else if constexpr (std::is_same_v<T, char> || std::is_same_v<T, char8_t>)
        return SourceBasicNameLiteral{"char"};
    else if constexpr (std::is_same_v<T, char16_t> || (std::is_same_v<T, wchar_t> && sizeof(T) == 2))
        return SourceBasicNameLiteral{"wchar"};
    else if constexpr (std::is_same_v<T, char32_t> || std::is_same_v<T, wchar_t>)
        return SourceBasicNameLiteral{"dchar"};
    else if constexpr (std::is_same_v<T, float>) return SourceBasicNameLiteral{"float"};
    else if constexpr (std::is_same_v<T, double>) return SourceBasicNameLiteral{"double"};
    else if constexpr (std::is_same_v<T, long double>) return SourceBasicNameLiteral{"real"};
    else if constexpr (sizeof(T) == 1) {
        if constexpr (std::is_signed_v<T>) return SourceBasicNameLiteral{"byte"};
        else return SourceBasicNameLiteral{"ubyte"};
    } else if constexpr (sizeof(T) == 2) {
        if constexpr (std::is_signed_v<T>) return SourceBasicNameLiteral{"short"};
        else return SourceBasicNameLiteral{"ushort"};
    } else if constexpr (sizeof(T) == 4) {
        if constexpr (std::is_signed_v<T>) return SourceBasicNameLiteral{"int"};
        else return SourceBasicNameLiteral{"uint"};
    } else if constexpr (sizeof(T) == 8) {
        if constexpr (std::is_signed_v<T>) return SourceBasicNameLiteral{"long"};
        else return SourceBasicNameLiteral{"ulong"};
    } else {
        static_assert(sizeof(T) == 16, "A native scalar needs its genuine Source primitive mapping");
        if constexpr (std::is_signed_v<T>) return SourceBasicNameLiteral{"cent"};
        else return SourceBasicNameLiteral{"ucent"};
    }
}

template<class T> RetainedString sourceNativeScalarName() {
    static constexpr auto name = sourceNativeScalarLiteral<T>();
    return RetainedString::fromStaticLiteral(name.bytes);
}

// DMD hdrgen's single-argument shortcut excludes qualified TypeBasic. Raw
// element contexts have no surrounding template-argument parentheses.
template<auto Name, class Indices = std::make_index_sequence<sizeof(Name.bytes) - 1>>
struct SourceConstBasicSpellings;

template<auto Name, std::size_t... I>
struct SourceConstBasicSpellings<Name, std::index_sequence<I...>> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = {
        '(', 'c', 'o', 'n', 's', 't', '(', Name.bytes[I]..., ')', ')', '\0'};
    inline static constexpr char unqualifiedArgument[] = {
        '(', 'c', 'o', 'n', 's', 't', '(', Name.bytes[I]..., ')', ')', '\0'};
    inline static constexpr char qualifiedElement[] = {
        'c', 'o', 'n', 's', 't', '(', Name.bytes[I]..., ')', '\0'};
    inline static constexpr char unqualifiedElement[] = {
        'c', 'o', 'n', 's', 't', '(', Name.bytes[I]..., ')', '\0'};
};
template<class T> inline constexpr bool sourceNativeObjectOrigin = [] {
    if constexpr (requires { SourceTypeSpellings<T>::isNativeObject; })
        return SourceTypeSpellings<T>::isNativeObject;
    else if constexpr (requires {
        SourceObjectTypeNames<T>::className(); SourceObjectTypeNames<T>::templateArgumentName();
    } || requires { SourceObjectTypeNames<T>::className; SourceObjectTypeNames<T>::templateArgumentName; })
        return true;
    else return false;
}();
}

// The selected backend's unqualified native scalar representations correspond
// to Source TypeBasic names. Qualifiers and non-basic types need their own data.
template<class T> requires (std::is_same_v<T, std::remove_cv_t<T>> &&
    (std::is_integral_v<T> || std::is_same_v<T, float> || std::is_same_v<T, double> ||
        std::is_same_v<T, long double>))
struct SourceTypeSpellings<T> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return detail::sourcePrimitiveTypeSpellings(detail::sourceNativeScalarName<T>());
    }
};

// Only the existing basic scalar carrier mapping establishes this origin.
// Native const on a general value holder is not Source transitive const.
template<class T> requires (std::is_same_v<T, std::remove_cv_t<T>> &&
    (std::is_void_v<T> || std::is_integral_v<T> || std::is_same_v<T, float> || std::is_same_v<T, double> ||
        std::is_same_v<T, long double>))
struct SourceTypeSpellings<const T>
    : detail::SourceConstBasicSpellings<detail::sourceNativeScalarLiteral<T>()> {};

#define NC_SOURCE_PRIMITIVE_SPELLINGS(Type, Name) \
    template<> struct SourceTypeSpellings<Type> { \
        inline static constexpr bool isNativeObject = false; \
        static SourceTypeSpellingNames get() { \
            return detail::sourcePrimitiveTypeSpellings(RetainedString::fromStaticLiteral(Name)); \
        } \
    }
NC_SOURCE_PRIMITIVE_SPELLINGS(void, "void");
NC_SOURCE_PRIMITIVE_SPELLINGS(RetainedString, "string");
NC_SOURCE_PRIMITIVE_SPELLINGS(common::NullableString, "string");
NC_SOURCE_PRIMITIVE_SPELLINGS(std::string, "string");
#undef NC_SOURCE_PRIMITIVE_SPELLINGS

// Independent nongeneric declaration origins read from the defining D modules.
// Parentheses here implement the TypeClass/TypeStruct single-argument rule;
// no name is obtained from ClassInfo or stripped from another spelling.
#define NC_SOURCE_NOMINAL_SPELLINGS(Type, Qualified, Unqualified, Object) \
    template<> struct SourceTypeSpellings<Type> { \
        inline static constexpr bool isNativeObject = Object; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified), RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NC_SOURCE_NOMINAL_SPELLINGS(nodes::Node, "nijilive.core.nodes.node.Node", "Node", true);
NC_SOURCE_NOMINAL_SPELLINGS(nodes::Part, "nijilive.core.nodes.part.Part", "Part", true);
NC_SOURCE_NOMINAL_SPELLINGS(nodes::PathDeformer,
    "nijilive.core.nodes.deformer.path.PathDeformer", "PathDeformer", true);
NC_SOURCE_NOMINAL_SPELLINGS(nodes::SimplePhysicsDriver,
    "nijilive.core.nodes.drivers.simplephysics.SimplePhysics", "SimplePhysics", true);
NC_SOURCE_NOMINAL_SPELLINGS(nodes::Composite,
    "nijilive.core.nodes.composite.composite.Composite", "Composite", true);
NC_SOURCE_NOMINAL_SPELLINGS(nodes::Drawable, "nijilive.core.nodes.drawable.Drawable", "Drawable", true);
NC_SOURCE_NOMINAL_SPELLINGS(nodes::GridDeformer,
    "nijilive.core.nodes.deformer.grid.GridDeformer", "GridDeformer", true);
NC_SOURCE_NOMINAL_SPELLINGS(nodes::MeshGroup, "nijilive.core.nodes.meshgroup.MeshGroup", "MeshGroup", true);
NC_SOURCE_NOMINAL_SPELLINGS(Puppet, "nijilive.core.puppet.Puppet", "Puppet", true);
NC_SOURCE_NOMINAL_SPELLINGS(param::Parameter, "nijilive.core.param.Parameter", "Parameter", true);
NC_SOURCE_NOMINAL_SPELLINGS(param::ParameterBinding,
    "nijilive.core.param.binding.ParameterBinding", "ParameterBinding", true);
NC_SOURCE_NOMINAL_SPELLINGS(param::ValueParameterBinding,
    "nijilive.core.param.binding.ValueParameterBinding", "ValueParameterBinding", true);
NC_SOURCE_NOMINAL_SPELLINGS(param::DeformationParameterBinding,
    "nijilive.core.param.binding.DeformationParameterBinding", "DeformationParameterBinding", true);
NC_SOURCE_NOMINAL_SPELLINGS(param::ParameterParameterBinding,
    "nijilive.core.param.binding.ParameterParameterBinding", "ParameterParameterBinding", true);
NC_SOURCE_NOMINAL_SPELLINGS(param::DeformSlot,
    "nijilive.core.nodes.defstack.Deformation", "Deformation", false);
#undef NC_SOURCE_NOMINAL_SPELLINGS

// A readonly Source class keeps its defining nominal origins. Native const on
// an arbitrary value holder does not establish a Source class declaration.
template<class T> requires (std::is_same_v<T, std::remove_cv_t<T>> &&
    detail::sourceNativeObjectOrigin<T>)
struct SourceTypeSpellings<const T> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        const auto qualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedElement);
        const auto unqualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedElement);
        // Genuine declaration callbacks execute before allocation translation.
        try {
            auto q = std::string("const(") + std::string(qualified.view()) + ')';
            auto u = std::string("const(") + std::string(unqualified.view()) + ')';
            return {RetainedString("(" + q + ")"), RetainedString("(" + u + ")"),
                RetainedString(std::move(q)), RetainedString(std::move(u))};
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
};

// A source class reference keeps the exact existing declaration origins. Value
// holders and arbitrary shared_ptr payloads are not treated as Source classes.
template<class T> requires detail::sourceNativeObjectOrigin<T>
struct SourceTypeSpellings<std::shared_ptr<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument),
            ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument),
            ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedElement),
            ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedElement)};
    }
};

// Other actual D-array carriers can use this same producer beside their own
// declarations. Element syntax is never inferred from a ClassInfo name.
template<class T> SourceTypeSpellingNames ngSourceDynamicArrayTypeSpellings() {
    const auto qualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedElement);
    const auto unqualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedElement);
    // Declaration providers can execute user code; translate only the pure
    // byte/retained-owner construction after both genuine origins are ready.
    try {
        auto q = std::string(qualified.view()) + "[]";
        auto u = std::string(unqualified.view()) + "[]";
        return {RetainedString("(" + q + ")"), RetainedString("(" + u + ")"),
            RetainedString(std::move(q)), RetainedString(std::move(u))};
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}
template<class T> struct SourceTypeSpellings<common::SharedArray<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() { return ngSourceDynamicArrayTypeSpellings<T>(); }
};

template<class K, class V> SourceTypeSpellingNames ngSourceAssociativeArrayTypeSpellings() {
    // The selected type printer emits value[key], each in its own context.
    // Resolve genuine declaration providers before pure byte construction.
    const auto qualifiedValue = ngSourceTypeSpelling<V>(SourceTypeSpelling::QualifiedElement);
    const auto qualifiedKey = ngSourceTypeSpelling<K>(SourceTypeSpelling::QualifiedElement);
    const auto unqualifiedValue = ngSourceTypeSpelling<V>(SourceTypeSpelling::UnqualifiedElement);
    const auto unqualifiedKey = ngSourceTypeSpelling<K>(SourceTypeSpelling::UnqualifiedElement);
    try {
        auto q = std::string(qualifiedValue.view()) + "[" + std::string(qualifiedKey.view()) + "]";
        auto u = std::string(unqualifiedValue.view()) + "[" + std::string(unqualifiedKey.view()) + "]";
        return {RetainedString("(" + q + ")"), RetainedString("(" + u + ")"),
            RetainedString(std::move(q)), RetainedString(std::move(u))};
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}

template<class K, class V, class Hash, class Equal>
struct SourceTypeSpellings<common::ResourceKeyMap<K, V, Hash, Equal>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() { return ngSourceAssociativeArrayTypeSpellings<K, V>(); }
};
}
