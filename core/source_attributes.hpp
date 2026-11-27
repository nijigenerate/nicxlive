#pragma once

#include <tuple>
#include <type_traits>

namespace nicxlive::core {

// Source UDAs can denote a type declaration without constructing its value.
template<class T> struct SourceTypeAttribute { using Type = T; };
template<class Class, auto Member> struct SourceMemberDeclaration {};
template<class Class> struct MemberDefinedAttributes {
    template<auto Member> static auto get() { return std::tuple{}; }
};
namespace detail {
template<class Member, class Lookup> struct SourceMemberDeclaringClass {
    using Type = std::remove_cv_t<Lookup>;
};
template<class Member, class Owner, class Lookup>
struct SourceMemberDeclaringClass<Member Owner::*, Lookup> { using Type = Owner; };
} // namespace detail
template<auto Left, auto Right> inline constexpr bool ngSameSourceDeclaration = [] {
    if constexpr (std::is_same_v<decltype(Left), decltype(Right)>) return Left == Right;
    else return false;
}();

// Specializations contain the actual Source UDA values, independently of class
// names, factories, or runtime RTTI. Unannotated declarations have no UDAs.
template<class T> struct UserDefinedAttributes {
    static auto get() { return std::tuple{}; }
};
template<class Class, auto Member>
struct UserDefinedAttributes<SourceMemberDeclaration<Class, Member>> {
    static auto get() {
        using Lookup = std::remove_cv_t<Class>;
        using Owner = typename detail::SourceMemberDeclaringClass<decltype(Member), Lookup>::Type;
        // Inherited lookup still denotes the original member declaration.
        if constexpr (std::is_same_v<Owner, Lookup> || std::is_base_of_v<Owner, Lookup>)
            return MemberDefinedAttributes<Owner>::template get<Member>();
        else return std::tuple{};
    }
};

template<class T, class Attribute> auto ngGetUDAs() {
    return std::apply([](const auto&... values) {
        return std::tuple_cat([](const auto& value) {
            using Value = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Attribute> ||
                    std::is_same_v<Value, SourceTypeAttribute<Attribute>>)
                return std::tuple<Value>{value};
            else return std::tuple{};
        }(values)...);
    }, UserDefinedAttributes<std::remove_cv_t<T>>::get());
}

template<class T, class Attribute>
inline constexpr bool ngHasUDA = std::tuple_size_v<decltype(ngGetUDAs<T, Attribute>())> != 0;

} // namespace nicxlive::core
