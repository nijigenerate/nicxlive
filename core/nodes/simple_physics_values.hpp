#pragma once
// String-valued enums from nijilive/core/nodes/drivers/simplephysics.d.
// Copyright Inochi2D/nijigenerate contributors, BSD-2-Clause; see SDK LICENSE.
#include "../compat/retained_string.hpp"
#include "../compat/object_metadata.hpp"
#include <array>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nicxlive::core::nodes {

// Native compatibility tags. Source values below retain their actual strings.
enum class PhysicsModel { Pendulum, SpringPendulum };
enum class ParamMapMode { AngleLength, XY, LengthAngle, YX };

namespace detail {
// std.math.constants.PI, carried at the host's selected real precision.
inline constexpr long double physicsPi = 0x1.921fb54442d18469898cc51701b84p+1L;

template<class Tag> struct PhysicsEnumMember {
    Tag tag;
    std::string_view value;
    std::string_view name;
};

struct PhysicsModelDomain {
    using Tag = PhysicsModel;
    inline static constexpr std::string_view name = "PhysicsModel";
    inline static constexpr std::array members{
        PhysicsEnumMember{Tag::Pendulum, "pendulum", "Pendulum"},
        PhysicsEnumMember{Tag::SpringPendulum, "spring_pendulum", "SpringPendulum"}};
};

struct ParamMapModeDomain {
    using Tag = ParamMapMode;
    inline static constexpr std::string_view name = "ParamMapMode";
    inline static constexpr std::array members{
        PhysicsEnumMember{Tag::AngleLength, "angle_length", "AngleLength"},
        PhysicsEnumMember{Tag::XY, "xy", "XY"},
        PhysicsEnumMember{Tag::LengthAngle, "length_angle", "LengthAngle"},
        PhysicsEnumMember{Tag::YX, "yx", "YX"}};
};

template<class Domain> class PhysicsStringEnum {
public:
    using NativeTag = typename Domain::Tag;
    PhysicsStringEnum() noexcept
        : value_(d_runtime::RetainedString::fromStaticView(Domain::members.front().value)) {}
    PhysicsStringEnum(NativeTag tag) {
        for (const auto& member : Domain::members) {
            if (member.tag == tag) {
                value_ = d_runtime::RetainedString::fromStaticView(member.value);
                return;
            }
        }
        throw std::invalid_argument("Native physics enum tag has no D string representation");
    }
    explicit PhysicsStringEnum(d_runtime::RetainedString value) noexcept : value_(std::move(value)) {}

    d_runtime::RetainedString raw() const noexcept { return value_; }
    std::string_view view() const noexcept { return value_.view(); }
    bool isNull() const noexcept { return value_.isNull(); }
    explicit operator bool() const noexcept { return !value_.isNull(); }

    std::optional<NativeTag> knownTag() const noexcept {
        for (const auto& member : Domain::members)
            if (member.value == value_.view()) return member.tag;
        return std::nullopt;
    }
    std::optional<std::string_view> memberName() const noexcept {
        for (const auto& member : Domain::members)
            if (member.value == value_.view()) return member.name;
        return std::nullopt;
    }
    static constexpr std::string_view sourceTypeName() noexcept { return Domain::name; }
    static constexpr const auto& declaredMembers() noexcept { return Domain::members; }

    friend bool operator==(const PhysicsStringEnum& left, const PhysicsStringEnum& right) noexcept {
        return left.value_.view() == right.value_.view();
    }
    friend bool operator==(const PhysicsStringEnum& left, NativeTag right) noexcept {
        const auto known = left.knownTag();
        return known && *known == right;
    }
    friend bool operator==(NativeTag left, const PhysicsStringEnum& right) noexcept { return right == left; }
    friend bool operator==(const PhysicsStringEnum& left, std::string_view right) noexcept {
        return left.value_.view() == right;
    }

private:
    d_runtime::RetainedString value_;
};
} // namespace detail

class SourcePhysicsModel : public detail::PhysicsStringEnum<detail::PhysicsModelDomain> {
    using Base = detail::PhysicsStringEnum<detail::PhysicsModelDomain>;
public:
    using Base::Base;
    inline static constexpr auto Pendulum = PhysicsModel::Pendulum;
    inline static constexpr auto SpringPendulum = PhysicsModel::SpringPendulum;
};

class SourceParamMapMode : public detail::PhysicsStringEnum<detail::ParamMapModeDomain> {
    using Base = detail::PhysicsStringEnum<detail::ParamMapModeDomain>;
public:
    using Base::Base;
    inline static constexpr auto AngleLength = ParamMapMode::AngleLength;
    inline static constexpr auto XY = ParamMapMode::XY;
    inline static constexpr auto LengthAngle = ParamMapMode::LengthAngle;
    inline static constexpr auto YX = ParamMapMode::YX;
};

} // namespace nicxlive::core::nodes

namespace nicxlive::core::d_runtime {

template<> struct SourceTypeSpellings<nodes::SourcePhysicsModel> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijilive.core.nodes.drivers.simplephysics.PhysicsModel)"),
            RetainedString::fromStaticLiteral("(PhysicsModel)"),
            RetainedString::fromStaticLiteral("nijilive.core.nodes.drivers.simplephysics.PhysicsModel"),
            RetainedString::fromStaticLiteral("PhysicsModel")};
    }
};
template<> struct SourceTypeSpellings<nodes::SourceParamMapMode> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijilive.core.nodes.drivers.simplephysics.ParamMapMode)"),
            RetainedString::fromStaticLiteral("(ParamMapMode)"),
            RetainedString::fromStaticLiteral("nijilive.core.nodes.drivers.simplephysics.ParamMapMode"),
            RetainedString::fromStaticLiteral("ParamMapMode")};
    }
};

} // namespace nicxlive::core::d_runtime
