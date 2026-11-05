#pragma once

#include "../common/utils.hpp"
#include "../nodes/common.hpp"
#include "../nodes/node.hpp"
#include "binding.hpp"
#include "../common/shared_array.hpp"
#include "deformation_io.hpp"
#include "../math/serialization.hpp"
#include "../common/unicode_lowercase.hpp"
#include "../compat/assert_error.hpp"
#include "../compat/source_allocator.hpp"
#include <cmath>
#include <concepts>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <functional>
#include <exception>
#include <stdexcept>
#include <map>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

namespace nicxlive::core::param {

using common::InterpolateMode;
using common::Vec2Array;
using nodes::Node;
using nodes::Vec2;
using nodes::Vec3;
using nodes::Vec4;
using Vec2u = ::nicxlive::core::param::Vec2u;

enum class ParamMergeMode {
    Additive,
    Weighted,
    Multiplicative,
    Forced,
    Passthrough,
};

using Resource = ::nicxlive::core::Resource;

inline float lerpValue(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

struct DeformSlot {
    Vec2Array vertexOffsets{};

    DeformSlot() = default;
    DeformSlot(Vec2Array offsets) : vertexOffsets(std::move(offsets)) {}
    DeformSlot(const DeformSlot& rhs) : vertexOffsets(rhs.vertexOffsets.dup()) {}
    DeformSlot(DeformSlot&&) noexcept = default;

    DeformSlot& operator=(const DeformSlot& rhs) {
        auto independent = rhs.vertexOffsets.dup();
        std::destroy_at(std::addressof(vertexOffsets));
        std::construct_at(std::addressof(vertexOffsets), std::move(independent));
        return *this;
    }

    DeformSlot& operator=(DeformSlot&& rhs) noexcept {
        if (this == std::addressof(rhs)) return *this;
        // Relocation transfers views; Vec2Array move assignment writes through them.
        std::destroy_at(std::addressof(vertexOffsets));
        std::construct_at(std::addressof(vertexOffsets), std::move(rhs.vertexOffsets));
        return *this;
    }

    void serialize(serde::TypedJsonWriter& writer) const {
        detail::serializeDeformationOffsets(writer, vertexOffsets);
    }
    template<class Serializer> void serialize(Serializer& writer) const {
        detail::serializeDeformationOffsets(writer, vertexOffsets);
    }

    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) {
        return detail::deserializeDeformationOffsets(data, vertexOffsets);
    }
};

inline DeformSlot scaleValue(const DeformSlot& in, float scale) {
    DeformSlot out = in;
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i) {
        out.vertexOffsets.xAt(i) *= scale;
        out.vertexOffsets.yAt(i) *= scale;
    }
    return out;
}

inline DeformSlot lerpValue(const DeformSlot& a, const DeformSlot& b, float t) {
    if (a.vertexOffsets.size() != b.vertexOffsets.size()) {
        assert(false && "DeformSlot::lerpValue requires equal vertex counts");
        DeformSlot out = a;
        return out;
    }
    DeformSlot out;
    out.vertexOffsets = Vec2Array(a.vertexOffsets.size());
    for (std::size_t i = 0; i < a.vertexOffsets.size(); ++i) {
        out.vertexOffsets.xAt(i) = lerpValue(a.vertexOffsets.xAt(i), b.vertexOffsets.xAt(i), t);
        out.vertexOffsets.yAt(i) = lerpValue(a.vertexOffsets.yAt(i), b.vertexOffsets.yAt(i), t);
    }
    return out;
}

template <typename T>
T scaleValue(const T& in, float scale) {
    return in * scale;
}

template <>
inline float scaleValue<float>(const float& in, float scale) {
    return in * scale;
}

template <>
inline DeformSlot scaleValue<DeformSlot>(const DeformSlot& in, float scale) {
    return scaleValue(static_cast<const DeformSlot&>(in), scale);
}

inline DeformSlot operator+(const DeformSlot& a, const DeformSlot& b) {
    DeformSlot out = a;
#ifndef NDEBUG
    // The original Deformation delegates this contract to veca.opOpAssign.
    if (a.vertexOffsets.size() != b.vertexOffsets.size())
        throw d_runtime::AssertError(d_runtime::RetainedString::fromStaticLiteral("Mismatched vector lengths"),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 181);
#endif
    // Preserve the authored lane order; these are the same actual arrays.
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i)
        out.vertexOffsets.xAt(i) += b.vertexOffsets.xAt(i);
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i)
        out.vertexOffsets.yAt(i) += b.vertexOffsets.yAt(i);
    return out;
}

inline DeformSlot operator-(const DeformSlot& a, const DeformSlot& b) {
    DeformSlot out = a;
#ifndef NDEBUG
    // The original Deformation delegates this contract to veca.opOpAssign.
    if (a.vertexOffsets.size() != b.vertexOffsets.size())
        throw d_runtime::AssertError(d_runtime::RetainedString::fromStaticLiteral("Mismatched vector lengths"),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 181);
#endif
    // Preserve the authored lane order; these are the same actual arrays.
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i)
        out.vertexOffsets.xAt(i) -= b.vertexOffsets.xAt(i);
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i)
        out.vertexOffsets.yAt(i) -= b.vertexOffsets.yAt(i);
    return out;
}

inline DeformSlot operator*(const DeformSlot& a, float s) {
    return scaleValue(a, s);
}

struct OwnedParameterConstruction {};

class Parameter : public Resource, public std::enable_shared_from_this<Parameter> {
private:
    struct Combinator {
        Vec2Array ivalues{};
        std::vector<float> iweights{};
        int isum{0};

        void clear() { isum = 0; }

        void resize(int reqLength) {
            ivalues.resize(static_cast<std::size_t>(reqLength));
            iweights.resize(static_cast<std::size_t>(reqLength));
        }

        void add(const Vec2& value, float weight) {
            if (isum >= static_cast<int>(ivalues.size())) resize(isum + 8);
            ivalues.set(isum, value);
            iweights[isum] = weight;
            ++isum;
        }

        void add(int axis, float value, float weight) {
            if (isum >= static_cast<int>(ivalues.size())) resize(isum + 8);
            ivalues.set(isum, Vec2{(axis == 0) ? value : 1.0f, (axis == 1) ? value : 1.0f});
            iweights[isum] = weight;
            ++isum;
        }

        Vec2 csum() const {
            Vec2 val{0.0f, 0.0f};
            for (int i = 0; i < isum; ++i) {
                val.x += ivalues.xAt(i);
                val.y += ivalues.yAt(i);
            }
            return val;
        }

        Vec2 avg() const {
            if (isum == 0) return Vec2{1.0f, 1.0f};
            Vec2 val{0.0f, 0.0f};
            for (int i = 0; i < isum; ++i) {
                val.x += ivalues.xAt(i) * iweights[i];
                val.y += ivalues.yAt(i) * iweights[i];
            }
            float denom = static_cast<float>(isum);
            return Vec2{val.x / denom, val.y / denom};
        }
    };

private:
    Combinator iadd{};
    Combinator imul{};

    static common::SharedArray<float> sourceAxisLiteral(std::initializer_list<float> values) {
        // Only primitive literal backing is inside this allocation boundary.
        try {
            return common::SharedArray<float>(values);
        } catch (const std::bad_alloc&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        }
    }

    common::SharedArray<float>& axis(std::size_t index) {
        if (index >= axisPoints.size()) throw d_runtime::ArrayIndexError(index, axisPoints.size());
        return axisPoints[index];
    }
    const common::SharedArray<float>& axis(std::size_t index) const {
        if (index >= axisPoints.size()) throw d_runtime::ArrayIndexError(index, axisPoints.size());
        return axisPoints[index];
    }
public:
    uint32_t uuid{0};
    common::NullableString name{};
    common::NullableString getName() const override { return name; }
    virtual void setName(common::NullableString value) { name = std::move(value); }
    std::uint32_t getUUID() const override { return uuid; }
    virtual void setUUID(std::uint32_t value) { uuid = value; }
    common::NullableString indexableName{};
    bool active{true};
    Vec2 value{0, 0};
    Vec2 latestInternal{0, 0};
    Vec2 previousInternal{0, 0};
    ParamMergeMode mergeMode{ParamMergeMode::Additive};
    Vec2 defaults{0, 0};
    bool isVec2{false};
    Vec2 min{0, 0};
    Vec2 max{1, 1};
    std::array<common::SharedArray<float>, 2> axisPoints{{sourceAxisLiteral({0, 1}), sourceAxisLiteral({0, 1})}};
    // D array copies retain the backing while keeping independent lengths.
    common::SharedArray<std::shared_ptr<ParameterBinding>> bindings{};

    Parameter() = default;
    explicit Parameter(OwnedParameterConstruction) {}
    template<class Derived = Parameter>
        requires (std::is_base_of_v<Parameter, Derived> &&
            std::is_constructible_v<Derived, OwnedParameterConstruction>)
    static std::shared_ptr<Derived> createSource(common::NullableString name, bool twoDimensional) {
        auto result = std::allocate_shared<Derived>(d_runtime::SourceAllocator<Derived>{},
            OwnedParameterConstruction{});
        result->constructSource(std::move(name), twoDimensional);
        return result;
    }
    template<class Derived = Parameter, class NameProducer, class DimensionProducer>
        requires (std::is_base_of_v<Parameter, Derived> &&
            std::is_constructible_v<Derived, OwnedParameterConstruction>)
    static std::shared_ptr<Derived> createSourceDeferred(NameProducer&& produceName,
            DimensionProducer&& produceDimension) {
        // Source new allocates the actual receiver before either argument.
        // Retain its strong owner before callbacks can reenter the application.
        auto result = std::allocate_shared<Derived>(d_runtime::SourceAllocator<Derived>{},
            OwnedParameterConstruction{});
        common::NullableString name = std::invoke(std::forward<NameProducer>(produceName));
        const bool twoDimensional = std::invoke(std::forward<DimensionProducer>(produceDimension));
        result->constructSource(std::move(name), twoDimensional);
        return result;
    }
protected:
    common::SharedArray<common::SharedArray<float>> duplicateSourceAxisDescriptors() const {
        // D static-array .dup allocates outer descriptor cells, not new float lanes.
        // These actual SharedArray<float> copies have no user callbacks.
        try {
            return common::SharedArray<common::SharedArray<float>>(
                std::vector<common::SharedArray<float>>(axisPoints.begin(), axisPoints.end()));
        } catch (const std::bad_alloc&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        }
    }

    void constructSource(common::NullableString name, bool twoDimensional) {
        setUUID(Node::inCreateUUID());
        setName(std::move(name));
        isVec2 = twoDimensional;
        if (!isVec2) axisPoints[1] = sourceAxisLiteral({0});
        makeIndexable();
    }
public:
    Parameter(const Parameter&) = delete;
    Parameter& operator=(const Parameter&) = delete;
    Parameter(Parameter&&) = delete;
    Parameter& operator=(Parameter&&) = delete;
    ~Parameter() override { Node::inUnloadUUID(getUUID()); }
    Parameter(common::NullableString n, bool vec2) : Parameter() {
        // Complete field defaults before the authored constructor statements.
        setUUID(Node::inCreateUUID());
        setName(std::move(n));
        isVec2 = vec2;
        if (!isVec2) {
            axisPoints[1] = sourceAxisLiteral({0});
        }
        makeIndexable();
    }

    virtual std::uint32_t axisPointCount(std::uint32_t index = 0) const {
        return static_cast<std::uint32_t>(axis(index).size());
    }

    float axisPointValue(std::size_t axis, std::size_t idx) const {
        const auto& arr = axisPoints[axis == 0 ? 0 : 1];
        if (idx < arr.size()) return arr[idx];
        return 0.0f;
    }

    virtual Vec2 getKeypointOffset(const Vec2u& idx) const {
        return Vec2{axisPoints[0][idx.x], axisPoints[1][idx.y]};
    }

    virtual void findOffset(Vec2 offset, Vec2u& leftKeypoint, Vec2& subOffset) const {
        leftKeypoint = {};
        subOffset = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
        const auto interpAxis = [&](std::uint32_t axis, float value, std::uint32_t& index, float& fraction) {
            index = 0;
            fraction = std::numeric_limits<float>::quiet_NaN();
            const auto positions = this->axis(axis);
            for (std::size_t i = 0; i < positions.size() - 1; ++i) {
                if (positions[i + 1] > value || i == positions.size() - 2) {
                    index = static_cast<std::uint32_t>(i);
                    fraction = (value - positions[i]) / (positions[i + 1] - positions[i]);
                    return;
                }
            }
        };
        interpAxis(0, offset.x, leftKeypoint.x, subOffset.x);
        if (isVec2) interpAxis(1, offset.y, leftKeypoint.y, subOffset.y);
    }

    virtual Vec2 normalizedValue() const { const auto input = value; return mapValue(input); }

    virtual void normalizedValue(const Vec2& input) {
        const auto v = input;
        value.x = v.x * (max.x - min.x) + min.x;
        value.y = v.y * (max.y - min.y) + min.y;
    }
    virtual void setNormalizedValue(const Vec2& v) { normalizedValue(v); }

    virtual std::shared_ptr<Parameter> dup();
    virtual Vec2 mapValue(const Vec2& value) const {
        const auto input = value;
        return {std::clamp((input.x - min.x) / (max.x - min.x), 0.0f, 1.0f),
                std::clamp((input.y - min.y) / (max.y - min.y), 0.0f, 1.0f)};
    }
    virtual float mapAxis(std::uint32_t axis, float inputValue) const {
        Vec2 input = min;
        if (axis == 0) input.x = inputValue;
        else input.y = inputValue;
        const Vec2 output = mapValue(input);
        if (axis == 0) return output.x;
        return output.y;
    }
    virtual Vec2 unmapValue(const Vec2& value) const {
        const auto offset = value;
        return {(max.x - min.x) * offset.x + min.x, (max.y - min.y) * offset.y + min.y};
    }
    virtual float unmapAxis(std::uint32_t axis, float offset) const {
        auto input = min;
        if (axis == 0) input.x = offset; else input.y = offset;
        const auto output = unmapValue(input);
        return axis == 0 ? output.x : output.y;
    }
    virtual Vec2 getKeypointValue(const Vec2u& index) const {
        const auto indexValue = index;
        return unmapValue(getKeypointOffset(indexValue));
    }
    virtual std::uint32_t getClosestAxisPointIndex(std::uint32_t axis, float offset) const {
        std::uint32_t closest = 0;
        float distance = std::numeric_limits<float>::infinity();
        const auto points = this->axis(axis);
        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto candidate = std::abs(points[i] - offset);
            if (candidate < distance) { distance = candidate; closest = static_cast<std::uint32_t>(i); }
        }
        return closest;
    }
    virtual Vec2u findClosestKeypoint() const { return findClosestKeypoint(value); }
    virtual Vec2 getClosestKeypointValue() const { return getKeypointValue(findClosestKeypoint()); }
    virtual Vec2 getClosestKeypointValue(const Vec2& input) const {
        const auto inputValue = input;
        return getKeypointValue(findClosestKeypoint(inputValue));
    }
    virtual Vec2u findClosestKeypoint(const Vec2& input) const {
        const auto inputValue = input;
        const auto mapped = mapValue(inputValue);
        return {getClosestAxisPointIndex(0, mapped.x), getClosestAxisPointIndex(1, mapped.y)};
    }
    virtual std::shared_ptr<ParameterBinding> createBinding(const std::shared_ptr<Node>& target, const std::string& key, bool setZero = true);
    virtual std::shared_ptr<ParameterBinding> createBinding(const std::shared_ptr<Node>& target,
                                                           common::NullableString key, bool setZero = true);
    std::shared_ptr<ParameterBinding> createBinding(const std::shared_ptr<Node>& target,
                                                   const char* key, bool setZero = true) {
        return createBinding(target, common::NullableString(key), setZero);
    }
    virtual std::shared_ptr<ParameterBinding> createBinding(const std::shared_ptr<Parameter>& target, int axis, bool setZero = true);
    virtual std::shared_ptr<ParameterBinding> createBinding(const BindTarget& target, bool setZero = true);
    virtual std::shared_ptr<ParameterBinding> createBinding(const std::shared_ptr<Resource>& target,
                                                    common::NullableString key, bool setZero = true);
    virtual std::shared_ptr<ParameterBinding> getBinding(const std::shared_ptr<Resource>& target,
                                                common::NullableString key) const;
    virtual bool hasBinding(const std::shared_ptr<Resource>& target, common::NullableString key) const;
    virtual bool hasAnyBinding(const std::shared_ptr<Resource>& target) const;
    virtual std::shared_ptr<ParameterBinding> getOrAddBinding(const std::shared_ptr<Resource>& target,
                                                    common::NullableString key, bool setZero = true);
    virtual std::shared_ptr<ParameterBinding> getBinding(const std::shared_ptr<Node>& /*self*/, const std::string& key) const;
    virtual std::shared_ptr<ParameterBinding> getBinding(const std::shared_ptr<Parameter>& target, const std::string& key) const;
    virtual std::shared_ptr<ParameterBinding> getOrAddBinding(const std::shared_ptr<Node>& target, const std::string& key, bool setZero = true);
    virtual bool hasBinding(const std::shared_ptr<Node>& target, const std::string& key) const;
    virtual bool hasAnyBinding(const std::shared_ptr<Node>& target) const;
    // shared_ptr conversions do not rank the Node-to-Resource inheritance depth.
    // These exact-handle adapters keep descriptor arguments from ambiguously
    // choosing a legacy owned-string overload and the real Resource overload.
    template<class R, class S> requires (std::derived_from<R, Resource> && !std::same_as<R, Resource> &&
        std::constructible_from<common::NullableString, S>)
    std::shared_ptr<ParameterBinding> getBinding(const std::shared_ptr<R>& target, S&& key) const {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return getBinding(std::static_pointer_cast<Resource>(target),
                          common::NullableString(std::forward<S>(key)));
    }
    template<class R, class S> requires (std::derived_from<R, Resource> && !std::same_as<R, Resource> &&
        std::constructible_from<common::NullableString, S>)
    bool hasBinding(const std::shared_ptr<R>& target, S&& key) const {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return hasBinding(std::static_pointer_cast<Resource>(target),
                          common::NullableString(std::forward<S>(key)));
    }
    template<class R, class S> requires (std::derived_from<R, Resource> && !std::same_as<R, Resource> &&
        std::constructible_from<common::NullableString, S>)
    std::shared_ptr<ParameterBinding> getOrAddBinding(const std::shared_ptr<R>& target, S&& key,
                                                     bool setZero = true) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        return getOrAddBinding(std::static_pointer_cast<Resource>(target),
                               common::NullableString(std::forward<S>(key)), setZero);
    }
    template<class R, class S> requires (std::derived_from<R, Resource> && !std::same_as<R, Resource> &&
        std::constructible_from<common::NullableString, S>)
    std::shared_ptr<ParameterBinding> createBinding(const std::shared_ptr<R>& target, S&& key,
                                                   bool setZero = true) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if constexpr (std::derived_from<R, Node>) {
            using Create = std::shared_ptr<ParameterBinding> (Parameter::*)(
                const std::shared_ptr<Node>&, common::NullableString, bool);
            const auto method = static_cast<Create>(&Parameter::createBinding);
            return (this->*method)(std::static_pointer_cast<Node>(target),
                                  common::NullableString(std::forward<S>(key)), setZero);
        } else {
            return createBinding(std::static_pointer_cast<Resource>(target),
                                 common::NullableString(std::forward<S>(key)), setZero);
        }
    }

    virtual void addBinding(const std::shared_ptr<ParameterBinding>& binding);
    virtual void removeBinding(const std::shared_ptr<ParameterBinding>& binding);
    virtual void moveAxisPoint(std::uint32_t axis, std::uint32_t oldIndex, float offset);
    virtual void reverseAxis(std::uint32_t axis);
    virtual void insertAxisPoint(std::uint32_t axis, float offset);
    virtual void deleteAxisPoint(std::uint32_t axis, std::uint32_t index);
    virtual void makeIndexable();
    virtual void update();
    virtual bool valueChanged() const { return latestInternal.x != previousInternal.x || latestInternal.y != previousInternal.y; }
    virtual void pushIOffset(const Vec2& input, ParamMergeMode mode = ParamMergeMode::Passthrough, float weight = 1.0f) {
        const auto offset = input;
        if (mode == ParamMergeMode::Passthrough) mode = mergeMode;
        switch (mode) {
        case ParamMergeMode::Forced:
            value = offset;
            break;
        case ParamMergeMode::Additive:
            iadd.add(offset, 1.0f);
            break;
        case ParamMergeMode::Multiplicative:
            imul.add(offset, 1.0f);
            break;
        case ParamMergeMode::Weighted:
            imul.add(offset, weight);
            break;
        default:
            break;
        }
    }
    virtual void pushIOffsetAxis(int axis, float offset, ParamMergeMode mode = ParamMergeMode::Passthrough, float weight = 1.0f) {
        if (mode == ParamMergeMode::Passthrough) mode = mergeMode;
        switch (mode) {
        case ParamMergeMode::Forced:
            if (axis < 0 || axis >= 2) throw d_runtime::ArrayIndexError(static_cast<std::size_t>(axis), 2);
            if (axis == 0) value.x = offset;
            else value.y = offset;
            break;
        case ParamMergeMode::Additive:
            iadd.add(axis, offset, 1.0f);
            break;
        case ParamMergeMode::Multiplicative:
            imul.add(axis, offset, 1.0f);
            break;
        case ParamMergeMode::Weighted:
            imul.add(axis, offset, weight);
            break;
        default:
            break;
        }
    }
    virtual void serializeSelf(serde::TypedJsonWriter& serializer) const;
    virtual void serialize(serde::TypedJsonWriter& serializer) const {
        const auto state = serializer.structBegin();
        serializeSelf(serializer);
        serializer.structEnd(state);
    }
    virtual serde::ReadError deserializeFromFghj(serde::TypedJsonView data);
    virtual void serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const;
    virtual ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data);
    virtual void reconstruct(const std::shared_ptr<::nicxlive::core::Puppet>& puppet);
    virtual void finalize(const std::shared_ptr<::nicxlive::core::Puppet>& puppet);
    template<class S> void serialize(S& serializer) {
        const auto receiverLifetime = weak_from_this().lock();
        const auto state = serializer.structBegin();
        this->serializeSelf(serializer);
        serializer.structEnd(state);
    }
    virtual void serializeSelf(serde::TypedJsonWriter& serializer) {
        const auto receiverLifetime = weak_from_this().lock();
        serializer.putKey("uuid");
        serializer.putValue(getUUID());
        serializer.putKey("name");
        serializer.putValue(getName().view());
        serializer.putKey("is_vec2");
        serializer.putValue(isVec2);
        serializer.putKey("min");
        math::serialize(min, serializer);
        serializer.putKey("max");
        math::serialize(max, serializer);
        serializer.putKey("defaults");
        math::serialize(defaults, serializer);
        serializer.putKey("axis_points");
        serializer.serializeValue(axisPoints);
        serializer.putKey("merge_mode");
        constexpr std::string_view modes[]{"Additive", "Weighted", "Multiplicative", "Forced", "Passthrough"};
        const auto index = static_cast<unsigned>(mergeMode);
        if (index >= std::size(modes)) d_runtime::ngAssertUnreachable("Assertion failure", __FILE__, __LINE__);
        serializer.putValue(modes[index]);
        serializer.putKey("bindings");
        const auto state = serializer.listBegin();
        const auto iteration = bindings;
        for (auto binding : iteration) {
            serializer.elemBegin();
            (void)d_runtime::ngRequireObject(binding)->serializeSelf(serializer);
        }
        serializer.listEnd(state);
    }
};


} // namespace nicxlive::core::param

#include "binding_impl.hpp"

namespace nicxlive::core::param {
using ParameterFactory = std::function<std::shared_ptr<Parameter>(const serde::Fghj&)>;
using ParameterFactoryHandle = std::shared_ptr<const ParameterFactory>;
inline ParameterFactoryHandle& parameterFactory() {
    // Source module state is TLS; copies retain the same real delegate context.
    static thread_local ParameterFactoryHandle factory;
    return factory;
}
inline std::shared_ptr<Parameter> inParameterCreate(const serde::Fghj& data) {
    const auto factory = parameterFactory();
    if (!factory || !*factory) d_runtime::ngFailNullAccess(false);
    return (*factory)(data);
}
inline void inParameterSetRetainedFactory(ParameterFactoryHandle factory) {
    parameterFactory() = std::move(factory);
}
inline void inParameterSetFactory(ParameterFactory factory) {
    inParameterSetRetainedFactory(factory ? std::make_shared<const ParameterFactory>(std::move(factory))
                                         : ParameterFactoryHandle{});
}

using TypedParameterFactory = std::function<std::shared_ptr<Parameter>(serde::TypedJsonView)>;
using TypedParameterFactoryHandle = std::shared_ptr<const TypedParameterFactory>;
inline TypedParameterFactoryHandle& typedParameterFactory() {
    static thread_local TypedParameterFactoryHandle factory;
    return factory;
}
inline std::shared_ptr<Parameter> inParameterCreate(serde::TypedJsonView data) {
    // Retain the actual callable during reentrant replacement of the TLS slot.
    const auto factory = typedParameterFactory();
    if (!factory || !*factory) d_runtime::ngFailNullAccess(false);
    return (*factory)(data);
}
inline void inParameterSetRetainedTypedFactory(TypedParameterFactoryHandle factory) {
    typedParameterFactory() = std::move(factory);
}
// An explicit setter name keeps existing inParameterSetFactory({}) unambiguous.
inline void inParameterSetTypedFactory(TypedParameterFactory factory) {
    inParameterSetRetainedTypedFactory(factory ? std::make_shared<const TypedParameterFactory>(std::move(factory))
                                              : TypedParameterFactoryHandle{});
}

inline void Parameter::serializeSelf(serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    serializer.putField("uuid", getUUID());
    serializer.putKey("name");
    // Source uses primitive putValue, which quotes even a null string descriptor.
    serializer.putValue(getName().view());
    serializer.putField("is_vec2", isVec2);
    serializer.putField("min", std::array<float, 2>{min.x, min.y});
    serializer.putField("max", std::array<float, 2>{max.x, max.y});
    serializer.putField("defaults", std::array<float, 2>{defaults.x, defaults.y});
    serializer.putField("axis_points", axisPoints);
    constexpr std::string_view modes[]{"Additive", "Weighted", "Multiplicative", "Forced", "Passthrough"};
    serializer.putKey("merge_mode");
    const auto index = static_cast<unsigned>(mergeMode);
    if (index >= std::size(modes)) d_runtime::ngAssertUnreachable("Assertion failure", __FILE__, __LINE__);
    serializer.putValue(modes[index]);
    serializer.putKey("bindings");
    const auto state = serializer.listBegin();
    const auto iteration = bindings;
    for (auto binding : iteration) {
        serializer.elemBegin();
        (void)binding->serializeSelf(serializer);
    }
    serializer.listEnd(state);
}

inline void Parameter::serializeSelf(serde::InochiSerializer& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto& data = serializer.root;
    data.put("uuid", getUUID());
    data.put("name", getName().string());
    data.put("is_vec2", isVec2);
    auto array = [](const auto& values) {
        serde::Fghj result;
        for (auto value : values) { serde::Fghj item; item.put_value(value); result.push_back({"", item}); }
        return result;
    };
    data.put_child("min", array(std::array{min.x, min.y}));
    data.put_child("max", array(std::array{max.x, max.y}));
    data.put_child("defaults", array(std::array{defaults.x, defaults.y}));
    serde::Fghj points;
    for (const auto& axis : axisPoints) points.push_back({"", array(axis)});
    data.put_child("axis_points", points);
    static constexpr const char* modes[] = {"Additive", "Weighted", "Multiplicative", "Forced", "Passthrough"};
    const auto modeIndex = static_cast<std::size_t>(mergeMode);
    if (modeIndex >= std::size(modes)) d_runtime::ngAssertUnreachable("Assertion failure", __FILE__, __LINE__);
    data.put("merge_mode", modes[modeIndex]);
    serde::Fghj serializedBindings;
    const auto iteration = bindings;
    for (auto binding : iteration) {
        serde::InochiSerializer item;
        (void)binding->serializeSelf(item);
        serializedBindings.push_back({"", item.root});
    }
    data.put_child("bindings", serializedBindings);
}
} // namespace nicxlive::core::param
