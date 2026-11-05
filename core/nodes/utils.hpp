#pragma once
// Independent nijilive/core/nodes/utils.d Source adaptation; SDK BSD-2-Clause.
#include "node.hpp"
#include "../common/shared_array.hpp"
#include "../common/object_key_policy.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nicxlive::core::nodes {

// Ported value types may specialize this open equality policy. A value's own
// operator== remains the default; Source class handles use global Object ==.
template<class T> struct SourceValueEquality {
    bool operator()(const T& left, const T& right) const { return left == right; }
};

template<class T> requires std::is_polymorphic_v<T>
struct SourceValueEquality<std::shared_ptr<T>> {
    bool operator()(std::shared_ptr<T> left, std::shared_ptr<T> right) const {
        return common::ngClassObjectEquals(std::move(left), std::move(right));
    }
};

template<> struct SourceValueEquality<Vec2> {
    bool operator()(Vec2 left, Vec2 right) const { return left.x == right.x && left.y == right.y; }
};
template<> struct SourceValueEquality<Vec3> {
    bool operator()(Vec3 left, Vec3 right) const {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }
};
template<> struct SourceValueEquality<Vec4> {
    bool operator()(Vec4 left, Vec4 right) const {
        return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
    }
};

template<> struct SourceValueEquality<Node::FilterHook> {
    bool operator()(const Node::FilterHook& left, const Node::FilterHook& right) const {
        // The extra native compatibility tag is not part of the Source tuple.
        return left.stage == right.stage && left.func == right.func;
    }
};

template<class T> struct SourceValueEquality<common::SharedArray<T>> {
    bool operator()(common::SharedArray<T> left, common::SharedArray<T> right) const {
        const auto length = left.size();
        if (length != right.size()) return false;
        for (std::size_t i = 0; i < length; ++i) {
            const T a = left[i];
            const T b = right[i];
            if (!SourceValueEquality<T>{}(a, b)) return false;
        }
        return true;
    }
};

template<class T, std::size_t N> struct SourceValueEquality<std::array<T, N>> {
    bool operator()(const std::array<T, N>& left, const std::array<T, N>& right) const {
        for (std::size_t i = 0; i < N; ++i)
            if (!SourceValueEquality<T>{}(left[i], right[i])) return false;
        return true;
    }
};

template<class... T> struct SourceValueEquality<std::tuple<T...>> {
    bool operator()(const std::tuple<T...>& left, const std::tuple<T...>& right) const {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return (SourceValueEquality<T>{}(std::get<I>(left), std::get<I>(right)) && ...);
        }(std::index_sequence_for<T...>{});
    }
};

template<class A, class B> struct SourceValueEquality<std::pair<A, B>> {
    bool operator()(const std::pair<A, B>& left, const std::pair<A, B>& right) const {
        return SourceValueEquality<A>{}(left.first, right.first) &&
            SourceValueEquality<B>{}(left.second, right.second);
    }
};

namespace detail {
template<class T> std::ptrdiff_t sourceCountUntilValue(common::SharedArray<T> array, T value) {
    // Phobos countUntil's random-access branch freezes a signed length first.
    const auto length = std::bit_cast<std::ptrdiff_t>(array.size());
    for (std::ptrdiff_t i = 0; i < length; ++i) {
        const T current = array[static_cast<std::size_t>(i)];
        if (SourceValueEquality<T>{}(current, value)) return i;
    }
    return -1;
}
}

template<class T> common::SharedArray<T> ngRemoveByValueSource(common::SharedArray<T> array, T value) {
    const auto index = detail::sourceCountUntilValue(array, value);
    if (index != -1) return array.removed(static_cast<std::size_t>(index));
    return array;
}

template<class T, bool Prepend = false>
common::SharedArray<T> ngUpsertSource(common::SharedArray<T> array, T value) {
    const auto index = detail::sourceCountUntilValue(array, value);
    if (index == -1) {
        if constexpr (Prepend) array.insertValue(0, value);
        else array.push_back(value);
    }
    return array;
}

} // namespace nicxlive::core::nodes
