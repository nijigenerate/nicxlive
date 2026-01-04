#pragma once

#include "../nodes/common.hpp"
#include "../nodes/node.hpp"
#include "../common/utils.hpp"

#include <memory>
#include <string>
#include <vector>

namespace nicxlive::core::param {

using nodes::Vec2;
using nodes::Vec3;
using nodes::Vec4;
using nodes::Node;
using common::InterpolateMode;

class Parameter;

struct Vec2u {
    std::size_t x{};
    std::size_t y{};
};

struct BindTarget {
    std::weak_ptr<nodes::Node> node{};
    std::string name{};
};

class ParameterBinding : public std::enable_shared_from_this<ParameterBinding> {
public:
    virtual ~ParameterBinding() = default;

    virtual void reconstruct(const std::shared_ptr<::nicxlive::core::Puppet>& /*puppet*/) {}
    virtual void finalize(const std::shared_ptr<::nicxlive::core::Puppet>& /*puppet*/) {}

    virtual void apply(const Vec2u& leftKeypoint, const Vec2& offset) = 0;
    virtual void clear() = 0;
    virtual void setCurrent(const Vec2u& point) = 0;
    virtual void unset(const Vec2u& point) = 0;
    virtual void reset(const Vec2u& point) = 0;
    virtual bool isSet(const Vec2u& index) const = 0;
    virtual void scaleValueAt(const Vec2u& index, int axis, float scale) = 0;
    virtual void extrapolateValueAt(const Vec2u& index, int axis) = 0;
    virtual void copyKeypointToBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other, const Vec2u& dest) = 0;
    virtual void swapKeypointWithBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other, const Vec2u& dest) = 0;
    virtual void reverseAxis(uint32_t axis) = 0;
    virtual void reInterpolate() = 0;
    virtual std::vector<std::vector<bool>>& getIsSet() = 0;
    virtual uint32_t getSetCount() const = 0;
    virtual void moveKeypoints(uint32_t axis, uint32_t oldindex, uint32_t newindex) = 0;
    virtual void insertKeypoints(uint32_t axis, uint32_t index) = 0;
    virtual void deleteKeypoints(uint32_t axis, uint32_t index) = 0;
    virtual uint32_t getNodeUUID() const = 0;
    virtual InterpolateMode interpolateMode() const = 0;
    virtual void setInterpolateMode(InterpolateMode mode) = 0;
    virtual BindTarget getTarget() const = 0;
    virtual std::shared_ptr<Node> getNode() const = 0;
    virtual void setTarget(const std::shared_ptr<Node>& node, const std::string& paramName) = 0;
    virtual bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const = 0;
    virtual std::string getName() const = 0;
};


} // namespace nicxlive::core::param
