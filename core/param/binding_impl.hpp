#pragma once

#include "parameter.hpp"
#include "binding.hpp"
#include "../nodes/common.hpp"
#include "../serde.hpp"
#include "../debug_log.hpp"
#include "../load_diagnostics.hpp"
#include "../compat/native_access_fault.hpp"
#include "../compat/array_range_error.hpp"
#include "../compat/phobos_decimal_format.hpp"
#include "../compat/source_allocator.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include <type_traits>

namespace nicxlive::core {
class Puppet;
std::shared_ptr<nodes::Node> resolvePuppetNodeById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid);
std::shared_ptr<Resource> resolvePuppetResourceById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid);
std::shared_ptr<::nicxlive::core::param::Parameter> resolvePuppetParameterById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid);
}
namespace nicxlive::core::nodes {
bool areDeformationNodesCompatible(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs);
}

namespace nicxlive::core::param {

inline std::shared_ptr<Parameter> BindTarget::ParameterView::lock() const {
    return std::dynamic_pointer_cast<Parameter>(target_->resource());
}
inline BindTarget::ParameterView& BindTarget::ParameterView::operator=(const std::shared_ptr<Parameter>& value) {
    // An empty compatibility view cannot erase the other Resource category.
    if (value || lock()) *target_ = std::static_pointer_cast<::nicxlive::core::Resource>(value);
    return *this;
}
inline BindTarget::ParameterView& BindTarget::ParameterView::operator=(const std::weak_ptr<Parameter>& value) {
    return *this = value.lock();
}
inline BindTarget::ParameterView& BindTarget::ParameterView::operator=(const ParameterView& other) {
    return *this = other.lock();
}
inline void BindTarget::ParameterView::reset() {
    if (lock()) target_->reset();
}

inline serde::ReadError readBindingResourceString(serde::TypedJsonView data, common::NullableString& value) {
    if (data.kind() == serde::JsonKind::Null) { value = nullptr; return {}; }
    if (data.kind() != serde::JsonKind::String) return serde::unexpectedJsonKind(data);
    // fghj.deserializeValue(string) duplicates bytes; an empty duplicate is null.
    value = data.stringBytes().empty() ? common::NullableString{}
                                      : common::NullableString(data.stringBytes());
    return {};
}

using BindingAssertion = d_runtime::AssertError;
[[noreturn]] inline void bindingUnreachable(const char* message, const char* file, unsigned line) {
    d_runtime::ngAssertUnreachable(message, file, line);
}

template<class T> T cloneBindingValue(T value) { return value; }
inline DeformSlot cloneBindingValue(DeformSlot value) {
    DeformSlot result;
    result.vertexOffsets = value.vertexOffsets.dup();
    return result;
}

bool pushDeformationToNode(const std::shared_ptr<Node>& node, const DeformSlot& value);
bool getDeformationNodeVertexCount(const std::shared_ptr<Node>& node, std::size_t& outVertexCount);
std::size_t recoverDeformationOffsetsForNode(const std::shared_ptr<Node>& node, Vec2Array& offsets,
                                           std::span<const std::uint8_t> missing);
bool isDeformationNode(const std::shared_ptr<Node>& node);
void resetBindingDeformationForNode(std::shared_ptr<Node> node, DeformSlot& value);
std::size_t recoverBindingDeformationOffsetsForNode(std::shared_ptr<Node> node,
    common::SharedArray<common::SharedArray<DeformSlot>>& values, std::size_t x, std::size_t y,
    common::SharedArray<std::uint8_t> missing);

// Actual inmath.interpolate.lerp production expression, with Source value arguments.
// David Herberth / Inochi2D Project, MIT; see ../animation/inmath.LICENSE.
template<class T> T bindingLerpValue(T a, T b, float t) {
    return a * (1.0f - t) + b * t;
}

template <typename T>
inline T cubicValue(const T& p0, const T& p1, const T& p2, const T& p3, float t);

inline bool traceParamBindingEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_PARAM_BIND");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

template <typename T>
class ParameterBindingImpl : public ParameterBinding {
private:
    // Only unmanaged native-pointer constructors use this compatibility borrow.
    Parameter* borrowedParameter_{};

    struct ParameterAccess {
        std::shared_ptr<Parameter> owner;
        Parameter* pointer;
        Parameter* operator->() const { return d_runtime::ngRequireObject(pointer); }
    };

protected:
    ParameterAccess parameterAccess() const {
        const auto retained = parameter;
        return {retained, retained ? retained.get() : borrowedParameter_};
    }

    explicit ParameterBindingImpl(OwnedBindingConstruction) {}
    void constructSource(std::shared_ptr<Parameter> parent) { setParameter(std::move(parent)); }
    void constructSource(std::shared_ptr<Parameter> parent, std::shared_ptr<Resource> resource,
            common::NullableString name) {
        setParameter(std::move(parent));
        target.target = std::move(resource);
        target.name = std::move(name);
        clear();
    }

private:
    void retainLegacyParameter(Parameter* value) {
        // Capture a managed owner during construction. Unmanaged native callers
        // remain explicitly borrowed; getParameter never reconstructs an owner.
        auto owner = value ? value->weak_from_this().lock() : std::shared_ptr<Parameter>{};
        parameter = std::move(owner);
        borrowedParameter_ = parameter ? nullptr : value;
    }

public:
    std::shared_ptr<Parameter> parameter;
    std::shared_ptr<Parameter> getParameter() const { return parameter; }
    void setParameter(std::shared_ptr<Parameter> value) {
        [[maybe_unused]] const auto previous = parameter;
        parameter = std::move(value);
        borrowedParameter_ = nullptr;
    }
    bool hasBorrowedParameter() const { return borrowedParameter_ && !parameter; }

    template<class Derived, class... Args>
        requires (std::is_base_of_v<ParameterBindingImpl, Derived> &&
            std::is_constructible_v<Derived, OwnedBindingConstruction>)
    static std::shared_ptr<Derived> createSource(Args... arguments) {
        auto result = std::allocate_shared<Derived>(d_runtime::SourceAllocator<Derived>{},
            OwnedBindingConstruction{});
        result->constructSource(std::move(arguments)...);
        return result;
    }

    explicit ParameterBindingImpl(std::shared_ptr<Parameter> param) {
        setParameter(std::move(param));
    }
    ParameterBindingImpl(std::shared_ptr<Parameter> param,
                         const std::shared_ptr<::nicxlive::core::Resource>& resource,
                         common::NullableString name) : ParameterBindingImpl(std::move(param)) {
        setResourceTarget(resource, std::move(name));
    }
    ParameterBindingImpl(Parameter* param, const std::shared_ptr<Node>& target, const std::string& paramName) {
        retainLegacyParameter(param);
        setTarget(target, paramName);
    }

    ParameterBindingImpl(Parameter* param, const std::shared_ptr<::nicxlive::core::Resource>& resource,
                         common::NullableString name) {
        retainLegacyParameter(param);
        setResourceTarget(resource, std::move(name));
    }

    explicit ParameterBindingImpl(Parameter* param) { retainLegacyParameter(param); }

    BindTarget getTarget() const override { return target; }
    std::shared_ptr<Node> getNode() const override { return target.target.lock(); }
    std::shared_ptr<::nicxlive::core::Resource> getResourceTarget() const override {
        return target.getResource();
    }
    void setResourceTarget(const std::shared_ptr<::nicxlive::core::Resource>& resource,
                           common::NullableString name) override {
        target.target = resource;
        target.name = std::move(name);
        // The source setter does not invoke a UUID getter or another callback.
        // uuid is only a legacy deserialization mirror, not the Resource identity.
    }
    void setTarget(const std::shared_ptr<Node>& node, const std::string& paramName) override {
        target.target = node;
        target.name = paramName;
        target.uuid = node ? node->uuid : 0;
    }
    std::string getName() const override { return getNameDescriptor().string(); }
    common::NullableString getNameDescriptor() const override { return target.name; }

    serde::ReadError serializeSelf(serde::TypedJsonWriter& serializer) const override;
    serde::ReadError serializeSelf(serde::TypedJsonWriter& serializer) override;
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) override;
    ::nicxlive::core::serde::SerdeException serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    void apply(const Vec2u& leftKeypoint, const Vec2& offset) override {
        const auto leftKeypointValue = leftKeypoint;
        const auto offsetValue = offset;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        applyToTarget(interpolate(leftKeypointValue, offsetValue));
    }

    void reconstruct(const std::shared_ptr<::nicxlive::core::Puppet>& /*puppet*/) override {}

    void finalize(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto puppetValue = puppet;
        target.target = ::nicxlive::core::resolvePuppetResourceById(puppetValue, nodeUuid_);
        target.uuid = nodeUuid_;
        if constexpr (std::is_same_v<T, DeformSlot>) {
            const auto deformable = target.target.lock();
            if (isDeformationNode(deformable)) {
                const auto masks = recoveryMasks;
                for (std::size_t x = 0; x < masks.size(); ++x) {
                    const auto row = masks[x];
                    for (std::size_t y = 0; y < row.size(); ++y) {
                        const auto missing = row[y];
                        std::size_t expected = 0;
                        for (auto mask : missing) expected += (mask & 1 ? 1 : 0) + (mask & 2 ? 1 : 0);
                        if (!expected) continue;
                        // The helper reads the virtual vertices property BEFORE indexing
                        // current values, as the original call's argument order requires.
                        const auto recovered = recoverBindingDeformationOffsetsForNode(
                            deformable, values, x, y, missing);
                        // Source enforce arguments are eager, even on success.
                        const auto parameterName = parameterAccess()->getName();
                        const auto nodeId = nodeUuid_;
                        auto message = "Parameter '" + parameterName.string() + "', node " +
                            std::to_string(nodeId) + ", key [" + std::to_string(x) + "," +
                            std::to_string(y) + "]: cannot reconstruct " + std::to_string(expected - recovered) +
                            " missing deformation components";
                        if (recovered != expected) throw d_runtime::ThrowableException(
                            d_runtime::RetainedString(std::move(message)),
                            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 436);
                    }
                }
            }
            recoveryMasks = {};
        }
    }

    void clear() override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto xCount = parameterAccess()->axisPointCount(0);
        const auto yCount = parameterAccess()->axisPointCount(1);
        values.resize(xCount);
        isSetFlags.resize(xCount);
        for (auto x = 0u; x < xCount; ++x) {
            isSetFlags[x].resize(0);
            isSetFlags[x].resize(yCount);
            values[x].resize(yCount);
            for (auto y = 0u; y < yCount; ++y) {
                auto row = values[x];
                clearValue(row[y]);
            }
        }
    }

    void setCurrent(const Vec2u& point) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        isSetFlags[point.x][point.y] = true;
        reInterpolate();
    }

    void unset(const Vec2u& point) override {
        const auto pointValue = point;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        auto row = values[pointValue.x];
        clearValue(row[pointValue.y]);
        isSetFlags[pointValue.x][pointValue.y] = false;
        reInterpolate();
    }

    void reset(const Vec2u& point) override {
        const auto pointValue = point;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        auto row = values[pointValue.x];
        clearValue(row[pointValue.y]);
        isSetFlags[pointValue.x][pointValue.y] = true;
        reInterpolate();
    }

    bool isSet(const Vec2u& index) const override {
        return isSetFlags[index.x][index.y];
    }

    void reverseAxis(uint32_t axis) override {
        if (axis == 0) {
            auto valueRange = values;
            std::reverse(valueRange.begin(), valueRange.end());
            auto flagRange = isSetFlags;
            std::reverse(flagRange.begin(), flagRange.end());
        } else {
            auto valueRows = values;
            for (auto& row : valueRows) {
                auto range = row;
                std::reverse(range.begin(), range.end());
            }
            auto flagRows = isSetFlags;
            for (auto& row : flagRows) {
                auto range = row;
                std::reverse(range.begin(), range.end());
            }
        }
    }

    void reInterpolate() override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto xCount = static_cast<std::uint32_t>(parameterAccess()->axisPointCount(0));
        const auto yCount = static_cast<std::uint32_t>(parameterAccess()->axisPointCount(1));
        uint32_t setCount = 0;
        for (auto x = 0u; x < xCount; ++x) {
            for (auto y = 0u; y < yCount; ++y) {
                if (isSetFlags[x][y]) setCount++;
            }
        }
        if (setCount == 0) {
            clear();
            return;
        }

        std::vector<std::vector<bool>> valid(xCount, std::vector<bool>(yCount, false));
        std::vector<std::vector<bool>> newlySet(xCount, std::vector<bool>(yCount, false));
        std::vector<Vec2u> commitPoints;
        std::vector<std::vector<float>> interpDistance(xCount,
            std::vector<float>(yCount, std::numeric_limits<float>::quiet_NaN()));
        for (auto x = 0u; x < xCount; ++x) {
            for (auto y = 0u; y < yCount; ++y) {
                valid[x][y] = isSetFlags[x][y];
            }
        }
        auto axisPoint = [&](bool yMajor, uint32_t idx) -> float {
            return parameterAccess()->axisPoints[yMajor ? 0 : 1][idx];
        };
        auto get = [&](bool yMajor, uint32_t maj, uint32_t min) -> T {
            if (yMajor) return cloneBindingValue(values[min][maj]);
            return cloneBindingValue(values[maj][min]);
        };
        auto isValid = [&](bool yMajor, uint32_t maj, uint32_t min) -> bool {
            if (yMajor) return valid[min][maj];
            return valid[maj][min];
        };
        auto isNew = [&](bool yMajor, uint32_t maj, uint32_t min) -> bool {
            if (yMajor) return newlySet[min][maj];
            return newlySet[maj][min];
        };
        auto getDistance = [&](bool yMajor, uint32_t maj, uint32_t min) -> float {
            if (yMajor) return interpDistance[min][maj];
            return interpDistance[maj][min];
        };
        auto setPoint = [&](bool yMajor, uint32_t maj, uint32_t min, const T& val, float distance, bool commit) {
            #ifndef NDEBUG
            if (isValid(yMajor, maj, min)) throw BindingAssertion{nullptr, "nijilive/core/param/binding.d", yMajor ? 612u : 618u};
#endif
            if (yMajor) {
                values[min][maj] = cloneBindingValue(val);
                interpDistance[min][maj] = distance;
                newlySet[min][maj] = true;
                if (commit) commitPoints.push_back(Vec2u{min, maj});
            } else {
                values[maj][min] = cloneBindingValue(val);
                interpDistance[maj][min] = distance;
                newlySet[maj][min] = true;
                if (commit) commitPoints.push_back(Vec2u{maj, min});
            }
        };
        auto lerpT = [&](const T& a, const T& b, float t) {
            return bindingLerpValue(a, b, t);
        };

        auto interpolate1D2D = [&](bool yMajor, bool secondPass) {
            bool detectedIntersections = false;
            auto majorCnt = yMajor ? yCount : xCount;
            auto minorCnt = yMajor ? xCount : yCount;
            for (uint32_t i = 0; i < majorCnt; ++i) {
                uint32_t l = 0;
                // find first set
                for (; l < minorCnt && !isValid(yMajor, i, l); ++l) {}
                if (l >= minorCnt) continue;
                while (true) {
                    for (; l < minorCnt - 1 && isValid(yMajor, i, l + 1); ++l) {}
                    if (l >= minorCnt - 1) break;
                    uint32_t r = l + 1;
                    for (; r < minorCnt && !isValid(yMajor, i, r); ++r) {}
                    if (r >= minorCnt) break;
                    for (uint32_t m = l + 1; m < r; ++m) {
                        float leftOff = axisPoint(yMajor, l);
                        float midOff = axisPoint(yMajor, m);
                        float rightOff = axisPoint(yMajor, r);
                        float off = (midOff - leftOff) / (rightOff - leftOff);
                        auto leftValue = get(yMajor, i, l);
                        auto rightValue = get(yMajor, i, r);
                        auto val = lerpT(leftValue, rightValue, off);
                        if (secondPass && isNew(yMajor, i, m)) {
                            if (!detectedIntersections) commitPoints.clear();
                            setPoint(yMajor, i, m, (val + get(yMajor, i, m)) * 0.5f, 0.0f, true);
                            detectedIntersections = true;
                        }
                        if (!detectedIntersections) {
                            setPoint(yMajor, i, m, val, 0.0f, true);
                        }
                    }
                    l = r;
                }
            }
        };

        auto extrapolateCorners = [&]() {
            if (yCount <= 1 || xCount <= 1) return;
            auto extrapolateCorner = [&](uint32_t baseX, uint32_t baseY, int offX, int offY) {
                T base = values[baseX][baseY];
                T dX = values[baseX + offX][baseY] + (base * -1.0f);
                T dY = values[baseX][baseY + offY] + (base * -1.0f);
                values[baseX + offX][baseY + offY] = cloneBindingValue(base + dX + dY);
                commitPoints.push_back(Vec2u{baseX + offX, baseY + offY});
            };
            for (uint32_t x = 0; x < xCount - 1; ++x) {
                for (uint32_t y = 0; y < yCount - 1; ++y) {
                    if (valid[x][y] && valid[x + 1][y] && valid[x][y + 1] && !valid[x + 1][y + 1])
                        extrapolateCorner(x, y, 1, 1);
                    else if (valid[x][y] && valid[x + 1][y] && !valid[x][y + 1] && valid[x + 1][y + 1])
                        extrapolateCorner(x + 1, y, -1, 1);
                    else if (valid[x][y] && !valid[x + 1][y] && valid[x][y + 1] && valid[x + 1][y + 1])
                        extrapolateCorner(x, y + 1, 1, -1);
                    else if (!valid[x][y] && valid[x + 1][y] && valid[x][y + 1] && valid[x + 1][y + 1])
                        extrapolateCorner(x + 1, y + 1, -1, -1);
                }
            }
        };

        auto extendAndIntersect = [&](bool yMajor) {
            bool detectedIntersections = false;
            auto majorCnt = yMajor ? yCount : xCount;
            auto minorCnt = yMajor ? xCount : yCount;
            for (uint32_t i = 0; i < majorCnt; ++i) {
                uint32_t j = 0;
                for (; j < minorCnt && !isValid(yMajor, i, j); ++j) {}
                if (j >= minorCnt) continue;
                T val = get(yMajor, i, j);
                float origin = axisPoint(yMajor, j);
                for (uint32_t k = 0; k < j; ++k) {
                    float minDist = std::abs(axisPoint(yMajor, k) - origin);
                    if (yMajor && isNew(yMajor, i, k)) {
                        if (!detectedIntersections) commitPoints.clear();
                        float majDist = getDistance(yMajor, i, k);
                        float frac = minDist / (minDist + majDist * majDist / minDist);
                        auto cur = get(yMajor, i, k);
                        setPoint(yMajor, i, k, lerpT(val, cur, frac), 0.0f, true);
                        detectedIntersections = true;
                    }
                    if (!detectedIntersections) {
                        setPoint(yMajor, i, k, val, minDist, true);
                    }
                }
                for (j = minorCnt - 1; j < minorCnt && !isValid(yMajor, i, j); --j) {}
                val = get(yMajor, i, j);
                origin = axisPoint(yMajor, j);
                for (uint32_t k = j + 1; k < minorCnt; ++k) {
                    float minDist = std::abs(axisPoint(yMajor, k) - origin);
                    if (yMajor && isNew(yMajor, i, k)) {
                        if (!detectedIntersections) commitPoints.clear();
                        float majDist = getDistance(yMajor, i, k);
                        float frac = minDist / (minDist + majDist * majDist / minDist);
                        auto cur = get(yMajor, i, k);
                        setPoint(yMajor, i, k, lerpT(val, cur, frac), 0.0f, true);
                        detectedIntersections = true;
                    }
                    if (!detectedIntersections) {
                        setPoint(yMajor, i, k, val, minDist, true);
                    }
                }
            }
        };

        auto totalCount = xCount * yCount;
        uint32_t validCount = setCount;
        while (true) {
            for (auto& p : commitPoints) {
                #ifndef NDEBUG
                if (valid[p.x][p.y]) throw BindingAssertion{"trying to double-set a point", "nijilive/core/param/binding.d", 780};
#endif
                valid[p.x][p.y] = true;
                ++validCount;
            }
            commitPoints.clear();
            if (validCount == totalCount) break;
            for (auto& r : newlySet) std::fill(r.begin(), r.end(), false);
            interpolate1D2D(false, false);
            interpolate1D2D(true, true);
            if (!commitPoints.empty()) continue;
            extrapolateCorners();
            if (!commitPoints.empty()) continue;
            extendAndIntersect(false);
            extendAndIntersect(true);
            if (!commitPoints.empty()) continue;
            break;
        }
        if (validCount != totalCount) {
            throw d_runtime::ThrowableException(d_runtime::RetainedString("Interpolation failed to complete"),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 824);
        }
    }

    common::SharedArray<common::SharedArray<bool>>& getIsSet() override { return isSetFlags; }

    uint32_t getSetCount() const override {
        uint32_t count = 0;
        for (auto& row : isSetFlags) {
            for (auto b : row) {
                if (b) ++count;
            }
        }
        return count;
    }

    void moveKeypoints(uint32_t axis, uint32_t oldindex, uint32_t newindex) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
#ifndef NDEBUG
        if (axis > 1) throw BindingAssertion{nullptr, "nijilive/core/param/binding.d", 936};
#endif
        if (axis == 0) {
            auto value = values[oldindex];
            values = values.removed(oldindex);
            values.insertValue(newindex, value);
            auto flag = isSetFlags[oldindex];
            isSetFlags = isSetFlags.removed(oldindex);
            isSetFlags.insertValue(newindex, flag);
        } else if (axis == 1) {
            auto valueRows = values;
            for (auto& row : valueRows) {
                auto value = row[oldindex];
                row = row.removed(oldindex);
                row.insertValue(newindex, value);
            }
            // The original foreach copies each flag-row descriptor here.
            auto flagRows = isSetFlags;
            for (auto row : flagRows) {
                auto flag = row[oldindex];
                row = row.removed(oldindex);
                row.insertValue(newindex, flag);
            }
        }
        reInterpolate();
    }

    void insertKeypoints(uint32_t axis, uint32_t index) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
#ifndef NDEBUG
        if (axis > 1) throw BindingAssertion{nullptr, "nijilive/core/param/binding.d", 913};
#endif
        if (axis == 0) {
            const auto yCount = parameterAccess()->axisPointCount(1);
            values.insertValue(index, common::SharedArray<T>{});
            values[index].resize(yCount);
            isSetFlags.insertValue(index, common::SharedArray<bool>{});
            isSetFlags[index].resize(yCount);
        } else if (axis == 1) {
            const T initial = [] {
                if constexpr (std::is_floating_point_v<T>) return std::numeric_limits<T>::quiet_NaN();
                else return T{};
            }();
            auto valueRows = values;
            for (auto& row : valueRows) row.insertValue(index, initial);
            auto flagRows = isSetFlags;
            for (auto& row : flagRows) row.insertValue(index, false);
        }
        reInterpolate();
    }

    void deleteKeypoints(uint32_t axis, uint32_t index) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
#ifndef NDEBUG
        if (axis > 1) throw BindingAssertion{nullptr, "nijilive/core/param/binding.d", 972};
#endif
        if (axis == 0) {
            values = values.removed(index);
            isSetFlags = isSetFlags.removed(index);
        } else if (axis == 1) {
            const auto valueCount = values.size();
            for (std::size_t i = 0; i < valueCount; ++i) values[i] = values[i].removed(index);
            const auto flagCount = isSetFlags.size();
            for (std::size_t i = 0; i < flagCount; ++i) isSetFlags[i] = isSetFlags[i].removed(index);
        }
        reInterpolate();
    }

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        const auto indexValue = index;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        (void)axis;
        setValue(indexValue, getValue(indexValue) * scale);
    }

    void extrapolateValueAt(const Vec2u& index, int axis) override {
        const auto indexValue = index;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        Vec2 offset = parameterAccess()->getKeypointOffset(indexValue);
        switch (axis) {
        case -1: offset = Vec2{1.0f - offset.x, 1.0f - offset.y}; break;
        case 0: offset.x = 1.0f - offset.x; break;
        case 1: offset.y = 1.0f - offset.y; break;
        default: bindingUnreachable("bad axis", "nijilive/core/param/binding.d", 1003);
        }
        Vec2u src{};
        Vec2 sub{};
        parameterAccess()->findOffset(offset, src, sub);
        auto v = interpolate(src, sub);
        setValue(indexValue, v);
        scaleValueAt(indexValue, axis, -1.0f);
    }

    void copyKeypointToBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other,
                               const Vec2u& dest) override {
        const auto srcValue = src;
        const auto destValue = dest;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto retainedOther = other;
        if (!isSet(srcValue)) d_runtime::ngRequireObject(retainedOther)->unset(destValue);
        else if (auto destination = std::dynamic_pointer_cast<ParameterBindingImpl<T>>(retainedOther)) {
            destination->setValue(destValue, getValue(srcValue));
        } else bindingUnreachable("ParameterBinding class mismatch", "nijilive/core/param/binding.d", 1023);
    }

    void swapKeypointWithBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other,
                                const Vec2u& dest) override {
        const auto srcValue = src;
        const auto destValue = dest;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto retainedOther = other;
        if (auto destination = std::dynamic_pointer_cast<ParameterBindingImpl<T>>(retainedOther)) {
            const bool thisSet = isSet(srcValue);
            const bool otherSet = retainedOther->isSet(destValue);
            const auto thisValue = cloneBindingValue(getValue(srcValue));
            const auto otherValue = cloneBindingValue(destination->getValue(destValue));
            destination->values[destValue.x][destValue.y] = cloneBindingValue(thisValue);
            destination->isSetFlags[destValue.x][destValue.y] = thisSet;
            values[srcValue.x][srcValue.y] = cloneBindingValue(otherValue);
            isSetFlags[srcValue.x][srcValue.y] = otherSet;
            reInterpolate();
            destination->reInterpolate();
        } else bindingUnreachable("ParameterBinding class mismatch", "nijilive/core/param/binding.d", 1044);
    }

    uint32_t getNodeUUID() const override {
        return nodeUuid_;
    }

    InterpolateMode interpolateMode() const override { return interpolateMode_; }
    void setInterpolateMode(InterpolateMode mode) override { interpolateMode_ = mode; }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const override {
        return isCompatibleWithResource(other);
    }
    bool isCompatibleWithResource(const std::shared_ptr<Resource>&) const override { return true; }

public:
    BindTarget target{};
protected:
    uint32_t nodeUuid_{0};
    common::SharedArray<common::SharedArray<common::SharedArray<std::uint8_t>>> recoveryMasks;
public:
    common::SharedArray<common::SharedArray<T>> values{};
    common::SharedArray<common::SharedArray<bool>> isSet_;
protected:
    common::SharedArray<common::SharedArray<bool>>& isSetFlags = isSet_;
    InterpolateMode interpolateMode_{InterpolateMode::Linear};

public:
    virtual void applyToTarget(const T& value) = 0;

    virtual void clearValue(T&) {}

public:
    virtual T& getValue(const Vec2u& point) {
        const auto pointValue = point;
        if (pointValue.x >= values.size())
            throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(pointValue.x, values.size(),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 474);
        auto& column = values[pointValue.x];
        if (pointValue.y >= column.size())
            throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(pointValue.y, column.size(),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 474);
        return column[pointValue.y];
    }

    virtual void setValue(const Vec2u& point, const T& value) {
        const auto pointValue = point;
        const T v = value;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        if (pointValue.x >= values.size())
            throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(pointValue.x, values.size(),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 481);
        auto valuesRow = values[pointValue.x];
        if (pointValue.y >= valuesRow.size())
            throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(pointValue.y, valuesRow.size(),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 481);
        // The source captures the destination cell before cloning the value.
        auto&& target = valuesRow[pointValue.y];
        target = cloneBindingValue(v);
        if (pointValue.x >= isSetFlags.size())
            throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(pointValue.x, isSetFlags.size(),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 482);
        auto flagsRow = isSetFlags[pointValue.x];
        if (pointValue.y >= flagsRow.size())
            throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(pointValue.y, flagsRow.size(),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 482);
        flagsRow[pointValue.y] = true;
        reInterpolate();
    }

    virtual T interpolate(const Vec2u& leftKeypoint, const Vec2& offset) {
        const auto leftKeypointValue = leftKeypoint;
        const auto offsetValue = offset;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        switch (interpolateMode_) {
        case InterpolateMode::Nearest: return interpolateNearest(leftKeypointValue, offsetValue);
        case InterpolateMode::Linear: return interpolateLinear(leftKeypointValue, offsetValue);
        case InterpolateMode::Cubic: return interpolateCubic(leftKeypointValue, offsetValue);
        default: bindingUnreachable(nullptr, "nijilive/core/param/binding.d", 835);
        }
    }

    virtual T interpolateNearest(const Vec2u& leftKeypoint, const Vec2& offset) {
        const auto leftValue = leftKeypoint;
        const auto offsetValue = offset;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto x = static_cast<std::uint32_t>(leftValue.x);
        const auto y = static_cast<std::uint32_t>(leftValue.y);
        const std::uint32_t px = x + (offsetValue.x >= 0.5 ? 1u : 0u);
        if (parameterAccess()->isVec2) {
            const std::uint32_t py = y + (offsetValue.y >= 0.5 ? 1u : 0u);
            return values[px][py];
        }
        return values[px][0];
    }

    virtual T interpolateLinear(const Vec2u& leftKeypoint, const Vec2& offset) {
        const auto leftValue = leftKeypoint;
        const auto offsetValue = offset;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto x = static_cast<std::uint32_t>(leftValue.x);
        const auto y = static_cast<std::uint32_t>(leftValue.y);

        T p0, p1;
        if (parameterAccess()->isVec2) {
            T p00 = values[x][y], p01 = values[x][static_cast<std::uint32_t>(y + 1u)];
            T p10 = values[static_cast<std::uint32_t>(x + 1u)][y];
            T p11 = values[static_cast<std::uint32_t>(x + 1u)][static_cast<std::uint32_t>(y + 1u)];
            p0 = bindingLerpValue(p00, p01, offsetValue.y);
            p1 = bindingLerpValue(p10, p11, offsetValue.y);
        } else {
            p0 = values[x][0];
            p1 = values[static_cast<std::uint32_t>(x + 1u)][0];
        }
        return bindingLerpValue(p0, p1, offsetValue.x);
    }

    virtual T interpolateCubic(const Vec2u& leftKeypoint, const Vec2& offset) {
        const auto leftValue = leftKeypoint;
        const auto offsetValue = offset;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto x = static_cast<std::uint32_t>(leftValue.x);
        const auto y = static_cast<std::uint32_t>(leftValue.y);
        const std::ptrdiff_t xkp = x, ykp = y;
        const std::size_t xlen = values.size() - 1;
        const auto row = [&](std::size_t yp) {
            T p0 = values[static_cast<std::size_t>(std::max<std::ptrdiff_t>(xkp - 1, 0))][yp];
            T p1 = values[static_cast<std::size_t>(xkp)][yp];
            T p2 = values[static_cast<std::size_t>(xkp + 1)][yp];
            T p3 = values[std::min(static_cast<std::size_t>(xkp + 2), xlen)][yp];
            return cubicValue(p0, p1, p2, p3, offsetValue.x);
        };
        if (!parameterAccess()->isVec2) return row(0);
        const std::size_t ylen = values[0].size() - 1;
        std::array<T, 4> result;
        for (int i = 0; i < 4; ++i) {
            const auto yp = static_cast<std::size_t>(std::min(static_cast<std::size_t>(std::max<std::ptrdiff_t>(ykp + i - 1, 0)), ylen));
            result[i] = row(yp);
        }
        return cubicValue(result[0], result[1], result[2], result[3], offsetValue.y);
    }

};

class ValueParameterBinding : public ParameterBindingImpl<float> {
public:
    explicit ValueParameterBinding(OwnedBindingConstruction tag) : ParameterBindingImpl<float>(tag) {}
    explicit ValueParameterBinding(std::shared_ptr<Parameter> parameter)
        : ParameterBindingImpl<float>(std::move(parameter)) {}
    ValueParameterBinding(std::shared_ptr<Parameter> parameter,
                         const std::shared_ptr<::nicxlive::core::Resource>& resource,
                         common::NullableString name)
        : ParameterBindingImpl<float>(std::move(parameter), resource, std::move(name)) { clear(); }

    ValueParameterBinding(Parameter* parameter, const std::shared_ptr<Node>& targetNode, const std::string& paramName)
        : ParameterBindingImpl<float>(parameter, targetNode, paramName) { clear(); }

    ValueParameterBinding(Parameter* parameter, const std::shared_ptr<::nicxlive::core::Resource>& resource,
                            common::NullableString name)
        : ParameterBindingImpl<float>(parameter, resource, std::move(name)) { clear(); }

    explicit ValueParameterBinding(Parameter* parameter)
        : ParameterBindingImpl<float>(parameter) {}

    float valueAt(const Vec2u& idx) const {
        if (idx.x < values.size() && idx.y < values[idx.x].size()) {
            return values[idx.x][idx.y];
        }
        return 0.0f;
    }

    void setRawValueAt(const Vec2u& point, float value) {
        values[point.x][point.y] = value;
        isSetFlags[point.x][point.y] = true;
    }

    void update(const Vec2u& point, float value) {
        setValue(point, value);
    }

    virtual std::shared_ptr<Node> targetNode() const { return target.target.lock(); }

    void applyToTarget(const float& value) override {
        const auto sourceValue = value;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto node = d_runtime::ngRequireObject(targetNode());
        const auto name = d_runtime::RetainedString(target.name);
        node->setValueSource(name, sourceValue);
    }

    void clearValue(float& value) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto node = d_runtime::ngRequireObject(targetNode());
        const auto name = d_runtime::RetainedString(target.name);
        value = node->getDefaultValueSource(name);
    }

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        const auto indexValue = index;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto point = indexValue;
        // Evaluate the real virtual receiver before its callback-bearing arguments.
        const auto node = d_runtime::ngRequireObject(targetNode());
        const auto name = d_runtime::RetainedString(target.name);
        const auto value = getValue(point);
        setValue(point, node->scaleValueSource(name, value, axis, scale));
    }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const override {
        return isCompatibleWithResource(other);
    }
    bool isCompatibleWithResource(const std::shared_ptr<Resource>& other) const override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto node = d_runtime::ngRequireObject(std::dynamic_pointer_cast<Node>(other));
        const auto name = d_runtime::RetainedString(target.name);
        return node->hasParamSource(name);
    }
};

class DeformationParameterBinding : public ParameterBindingImpl<DeformSlot> {
public:
    explicit DeformationParameterBinding(OwnedBindingConstruction tag) : ParameterBindingImpl<DeformSlot>(tag) {}
    explicit DeformationParameterBinding(std::shared_ptr<Parameter> parameter)
        : ParameterBindingImpl<DeformSlot>(std::move(parameter)) {}
    DeformationParameterBinding(std::shared_ptr<Parameter> parameter,
                         const std::shared_ptr<::nicxlive::core::Resource>& resource,
                         common::NullableString name)
        : ParameterBindingImpl<DeformSlot>(std::move(parameter), resource, std::move(name)) { clear(); }

    DeformationParameterBinding(Parameter* parameter, const std::shared_ptr<Node>& targetNode, const std::string& paramName)
        : ParameterBindingImpl<DeformSlot>(parameter, targetNode, paramName) { clear(); }

    DeformationParameterBinding(Parameter* parameter, const std::shared_ptr<::nicxlive::core::Resource>& resource,
                            common::NullableString name)
        : ParameterBindingImpl<DeformSlot>(parameter, resource, std::move(name)) { clear(); }

    explicit DeformationParameterBinding(Parameter* parameter)
        : ParameterBindingImpl<DeformSlot>(parameter) {}

    DeformSlot interpolateLinear(const Vec2u& leftKeypoint, const Vec2& offset) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        const auto x = static_cast<std::uint32_t>(leftKeypoint.x);
        const auto y = static_cast<std::uint32_t>(leftKeypoint.y);
        const auto rowY = parameterAccess()->isVec2 ? y : 0u;
        const auto& p00 = values[x][rowY];
        const auto& p10 = values[static_cast<std::uint32_t>(x + 1u)][rowY];
        const auto count = p00.vertexOffsets.size();
        if (count != p10.vertexOffsets.size())
            throw d_runtime::ThrowableException(d_runtime::RetainedString("Deformation keyframes have different vertex counts"),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 1111);
        DeformSlot result;
        result.vertexOffsets.resize(count);
        if (parameterAccess()->isVec2) {
            const auto& p01 = values[x][static_cast<std::uint32_t>(y + 1u)];
            const auto& p11 = values[static_cast<std::uint32_t>(x + 1u)][static_cast<std::uint32_t>(y + 1u)];
            if (count != p01.vertexOffsets.size() || count != p11.vertexOffsets.size())
                throw d_runtime::ThrowableException(d_runtime::RetainedString("Deformation keyframes have different vertex counts"),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 1118);
            for (int lane = 0; lane < 2; ++lane) {
                for (std::size_t i = 0; i < count; ++i) {
                    const float a = lane ? p00.vertexOffsets.yAt(i) : p00.vertexOffsets.xAt(i);
                    const float b = lane ? p01.vertexOffsets.yAt(i) : p01.vertexOffsets.xAt(i);
                    const float c = lane ? p10.vertexOffsets.yAt(i) : p10.vertexOffsets.xAt(i);
                    const float d = lane ? p11.vertexOffsets.yAt(i) : p11.vertexOffsets.xAt(i);
                    const float p0 = a * (1 - offset.y) + b * offset.y;
                    const float p1 = c * (1 - offset.y) + d * offset.y;
                    (lane ? result.vertexOffsets.yAt(i) : result.vertexOffsets.xAt(i)) =
                        p0 * (1 - offset.x) + p1 * offset.x;
                }
            }
        } else {
            for (int lane = 0; lane < 2; ++lane) {
                for (std::size_t i = 0; i < count; ++i) {
                    const float a = lane ? p00.vertexOffsets.yAt(i) : p00.vertexOffsets.xAt(i);
                    const float b = lane ? p10.vertexOffsets.yAt(i) : p10.vertexOffsets.xAt(i);
                    (lane ? result.vertexOffsets.yAt(i) : result.vertexOffsets.xAt(i)) =
                        a * (1 - offset.x) + b * offset.x;
                }
            }
        }
        return result;
    }

    virtual std::shared_ptr<Node> targetNode() const { return target.target.lock(); }

    void applyToTarget(const DeformSlot& value) override {
        const auto sourceValue = value;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        if (target.name != "deform") {
            throw d_runtime::ThrowableException(d_runtime::RetainedString("Enforcement failed"),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 1163);
        }
        pushDeformationToNode(target.target.lock(), sourceValue);
    }

    void clearValue(DeformSlot& value) override {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        resetBindingDeformationForNode(target.target.lock(), value);
    }

    bool isSetAt(const Vec2u& idx) const {
        return idx.x < isSetFlags.size() && idx.y < isSetFlags[idx.x].size() && isSetFlags[idx.x][idx.y];
    }

    DeformSlot valueAt(const Vec2u& idx) const {
        if (idx.x < values.size() && idx.y < values[idx.x].size()) {
            return values[idx.x][idx.y];
        }
        return DeformSlot{};
    }

    DeformSlot sample(const Vec2u& leftKey, const Vec2& offset) {
        return this->interpolate(leftKey, offset);
    }

    void setRawOffsetsAt(const Vec2u& point, const Vec2Array& offsets) {
        values[point.x][point.y].vertexOffsets = offsets.dup();
        isSetFlags[point.x][point.y] = true;
    }

    virtual void update(const Vec2u& point, const Vec2Array& offsets) {
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        isSetFlags[point.x][point.y] = true;
        values[point.x][point.y].vertexOffsets = offsets.dup();
        reInterpolate();
    }

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        const auto indexValue = index;
        [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
        Vec2 vectorScale;
        switch (axis) {
        case -1: vectorScale = Vec2{scale, scale}; break;
        case 0: vectorScale = Vec2{scale, 1.0f}; break;
        case 1: vectorScale = Vec2{1.0f, scale}; break;
        default: bindingUnreachable("Bad axis", "nijilive/core/param/binding.d", 1190);
        }
        auto value = getValue(indexValue);
        for (std::size_t vertex = 0; vertex < value.vertexOffsets.size(); ++vertex) {
            value.vertexOffsets.xAt(vertex) *= vectorScale.x;
            value.vertexOffsets.yAt(vertex) *= vectorScale.y;
        }
        setValue(indexValue, value);
    }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const override {
        return isCompatibleWithResource(other);
    }
    bool isCompatibleWithResource(const std::shared_ptr<Resource>& other) const override {
        return ::nicxlive::core::nodes::areDeformationNodesCompatible(
            target.target.lock(), std::dynamic_pointer_cast<Node>(other));
    }

    void remapOffsets(const std::vector<std::size_t>& remap, const ::nicxlive::core::common::Vec2Array& replacement, std::size_t newLength) {
        for (std::size_t x = 0; x < values.size(); ++x) {
            for (std::size_t y = 0; y < values[x].size(); ++y) {
                auto& offsets = values[x][y].vertexOffsets;
                if (!remap.empty() && remap.size() == offsets.size()) {
                    ::nicxlive::core::common::Vec2Array reordered;
                    reordered.resize(remap.size());
                    for (std::size_t oldIdx = 0; oldIdx < remap.size(); ++oldIdx) {
                        auto newIdx = remap[oldIdx];
                        if (newIdx < reordered.size()) {
                            reordered.set(newIdx, Vec2{offsets.xAt(oldIdx), offsets.yAt(oldIdx)});
                        }
                    }
                    offsets = reordered;
                    if (x < isSetFlags.size() && y < isSetFlags[x].size()) isSetFlags[x][y] = !offsets.empty();
                } else if (replacement.size() == newLength && newLength > 0) {
                    offsets = replacement.dup();
                    if (x < isSetFlags.size() && y < isSetFlags[x].size()) isSetFlags[x][y] = true;
                } else {
                    offsets.resize(newLength);
                    offsets.fill(Vec2{0.0f, 0.0f});
                    if (x < isSetFlags.size() && y < isSetFlags[x].size()) isSetFlags[x][y] = false;
                }
            }
        }
        reInterpolate();
    }
};

class ParameterParameterBinding;
std::shared_ptr<ParameterBinding> makeParameterParameterBinding(Parameter* parameter);
std::shared_ptr<ParameterBinding> makeParameterParameterBinding(std::shared_ptr<Parameter> parameter);

inline std::shared_ptr<ParameterBinding> Parameter::getBinding(const std::shared_ptr<Node>& self, const std::string& key) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return getBinding(std::static_pointer_cast<Resource>(self), common::NullableString(key));
}
inline std::shared_ptr<ParameterBinding> Parameter::getBinding(const std::shared_ptr<Parameter>& target, const std::string& key) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return getBinding(std::static_pointer_cast<Resource>(target), common::NullableString(key));
}
inline bool Parameter::hasBinding(const std::shared_ptr<Node>& target, const std::string& key) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return hasBinding(std::static_pointer_cast<Resource>(target), common::NullableString(key));
}
inline bool Parameter::hasAnyBinding(const std::shared_ptr<Node>& target) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return hasAnyBinding(std::static_pointer_cast<Resource>(target));
}
inline void Parameter::addBinding(const std::shared_ptr<ParameterBinding>& binding) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto retainedBinding = binding;
#ifndef NDEBUG
    // Source assertion arguments evaluate two separate virtual getTarget calls.
    const auto resource = d_runtime::ngRequireObject(retainedBinding)->getTarget().getResource();
    const auto name = d_runtime::ngRequireObject(retainedBinding)->getTarget().name;
    if (hasBinding(resource, name)) throw d_runtime::AssertError(nullptr, __FILE__, __LINE__);
#endif
    bindings.push_back(retainedBinding);
}

inline std::shared_ptr<ParameterBinding> Parameter::getOrAddBinding(const std::shared_ptr<Node>& target, const std::string& key, bool setZero) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto retainedTarget = target;
    const auto retainedKey = key;
    if (auto existing = getBinding(retainedTarget, retainedKey)) return existing;
    auto binding = createBinding(retainedTarget, retainedKey, setZero);
    addBinding(binding);
    return binding;
}

inline void Parameter::removeBinding(const std::shared_ptr<ParameterBinding>& binding) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto sought = binding;
    const auto range = bindings;
    for (std::size_t index = 0; index < range.size(); ++index) {
        const auto candidate = range[index];
        if (ngParameterBindingObjectEquals(candidate, sought)) {
            // Source remove snapshots the current descriptor after opEquals.
            const auto removalRange = bindings;
            const auto length = removalRange.size();
#ifndef NDEBUG
            // removeStable's first/third popFrontExactly fail at the same site.
            if (index >= length)
                throw d_runtime::ngStaticError<d_runtime::AssertError>(
                    d_runtime::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
                    d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#else
            if (index > length)
                throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(index, length, length,
                    d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
            if (index == length)
                throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(1, 0, 0,
                    d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
#endif
            bindings = removalRange.removed(index);
            break;
        }
    }
}

inline void Parameter::moveAxisPoint(std::uint32_t which, std::uint32_t oldIndex, float offset) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
#ifndef NDEBUG
    if (!(oldIndex > 0 && oldIndex < axisPointCount(which) - 1))
        throw d_runtime::AssertError("invalid point index", __FILE__, __LINE__);
    if (!(offset > 0 && offset < 1)) throw d_runtime::AssertError("offset out of bounds", __FILE__, __LINE__);
    if (!(isVec2 ? which <= 1 : which == 0)) throw d_runtime::AssertError("bad axis", __FILE__, __LINE__);
#endif
    std::uint32_t index;
    for (index = 1; index < axis(which).size(); ++index)
        if (axis(which)[index + 1] > offset) break;
    if (oldIndex != index) {
        // Preserve the authored outer-row access, not a corrected scalar move.
        const auto moved = axis(oldIndex);
        axis(which) = axis(which).removed(oldIndex);
        axis(which).insertRange(index, moved);
    }
    const auto iteration = bindings;
    for (auto binding : iteration) binding->moveKeypoints(which, oldIndex, index);
}
inline void Parameter::insertAxisPoint(std::uint32_t which, float offset) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
#ifndef NDEBUG
    if (!(offset > 0 && offset < 1)) throw d_runtime::AssertError("offset out of bounds", __FILE__, __LINE__);
    if (!(isVec2 ? which <= 1 : which == 0)) throw d_runtime::AssertError("bad axis", __FILE__, __LINE__);
#endif
    std::uint32_t index;
    for (index = 1; index < axis(which).size(); ++index) if (axis(which)[index] > offset) break;
    axis(which).insertValue(index, offset);
    const auto iteration = bindings;
    for (auto binding : iteration) binding->insertKeypoints(which, index);
}
inline void Parameter::deleteAxisPoint(std::uint32_t which, std::uint32_t index) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
#ifndef NDEBUG
    if (!(isVec2 ? which <= 1 : which == 0)) throw d_runtime::AssertError("bad axis", __FILE__, __LINE__);
    if (!(index > 0)) throw d_runtime::AssertError("cannot delete axis point at 0", __FILE__, __LINE__);
    if (!(index < axis(which).size() - 1)) throw d_runtime::AssertError("cannot delete axis point at 1", __FILE__, __LINE__);
#endif
    axis(which) = axis(which).removed(index);
    const auto iteration = bindings;
    for (auto binding : iteration) binding->deleteKeypoints(which, index);
}
inline void Parameter::reverseAxis(std::uint32_t which) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto row = axis(which);
    std::reverse(row.begin(), row.end());
    row = axis(which);
    for (auto& value : row) value = 1 - value;
    const auto iteration = bindings;
    for (auto binding : iteration) binding->reverseAxis(which);
}

inline void Parameter::makeIndexable() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    indexableName = common::unicode::toLower(getName());
}

inline serde::ReadError Parameter::deserializeFromFghj(serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    (void)serde::deserializeValue(data.findFirst("uuid"), uuid);
    (void)readBindingResourceString(data.findFirst("name"), name);
    const auto dimensionality = data.findFirst("is_vec2");
    if (!dimensionality.isMissing()) (void)serde::deserializeValue(dimensionality, isVec2);
    const auto readVector = [](serde::TypedJsonView field, Vec2& value) {
        if (!field.isMissing()) (void)math::deserialize(value, field);
    };
    readVector(data.findFirst("min"), min);
    readVector(data.findFirst("max"), max);
    const auto points = data.findFirst("axis_points");
    // D static-array decoding leaves short trailing axes untouched and stops
    // at the first returned row error, which this caller deliberately ignores.
    if (!points.isMissing() && points.kind() == serde::JsonKind::Array) {
        auto axes = points.byElement();
        for (std::size_t axis = 0; !axes.empty() && axis < axisPoints.size(); ++axis) {
            if (serde::deserializeValue(axes.front(), axisPoints[axis])) break;
            axes.popFront();
        }
    }
    readVector(data.findFirst("defaults"), defaults);
    const auto mode = data.findFirst("merge_mode");
    if (!mode.isMissing()) {
        std::optional<std::string> text;
        (void)serde::deserializeValue(mode, text);
        constexpr std::string_view names[]{"Additive", "Weighted", "Multiplicative", "Forced", "Passthrough"};
        constexpr std::string_view aliases[]{"additive", "weighted", "multiplicative", "forced", "passthrough"};
        bool found = false;
        for (unsigned index = 0; index < std::size(names); ++index) {
            if (text && (*text == names[index] || *text == aliases[index])) {
                mergeMode = static_cast<ParamMergeMode>(index); found = true; break;
            }
        }
        if (!found) throw d_runtime::ThrowableException(d_runtime::RetainedString(
            "Unable to deserialize string '" + text.value_or("") +
            "' to ParamMergeModeAllowed keys:allowedKeys"),
            d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
    }
    const auto entries = data.findFirst("bindings");
    if (!entries.isMissing() && entries.kind() == serde::JsonKind::Array) {
        for (const auto child : entries.byElement()) {
            const auto field = child.findFirst("param_name");
            if (field.isMissing()) continue;
            std::string paramName;
            (void)serde::deserializeValue(field, paramName);
            int paramId = -1;
            (void)serde::deserializeValue(field, paramId);
            std::shared_ptr<ParameterBinding> binding;
            if (paramName == "deform") binding = receiverLifetime ? std::make_shared<DeformationParameterBinding>(receiverLifetime)
                    : std::make_shared<DeformationParameterBinding>(this);
            else if (paramName == "X" || paramName == "Y" || paramId == 0 || paramId == 1)
                binding = receiverLifetime ? makeParameterParameterBinding(receiverLifetime)
                    : makeParameterParameterBinding(this);
            else binding = receiverLifetime ? std::make_shared<ValueParameterBinding>(receiverLifetime)
                    : std::make_shared<ValueParameterBinding>(this);
            (void)binding->deserializeFromFghj(child);
            bindings.push_back(std::move(binding));
        }
    }
    return {};
}

inline ::nicxlive::core::serde::SerdeException Parameter::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto readVec2 = [](const ::nicxlive::core::serde::Fghj& node, Vec2& out) {
        if (node.empty()) return;
        std::size_t i = 0;
        for (const auto& elem : node) {
            float v = elem.second.get_value<float>();
            if (i == 0) out.x = v;
            else if (i == 1) out.y = v;
            ++i;
            if (i >= 2) break;
        }
    };

    try {
        if (auto u = data.get_optional<uint32_t>("uuid")) uuid = *u;
        if (auto n = data.get_optional<std::string>("name")) name = *n;
        if (auto vec2 = data.get_optional<bool>("is_vec2")) isVec2 = *vec2;
        if (auto d = data.get_child_optional("defaults")) readVec2(*d, defaults);
        if (auto mn = data.get_child_optional("min")) readVec2(*mn, min);
        if (auto mx = data.get_child_optional("max")) readVec2(*mx, max);

        if (auto mode = data.get_optional<std::string>("merge_mode")) {
            if (*mode == "Additive") mergeMode = ParamMergeMode::Additive;
            else if (*mode == "Weighted") mergeMode = ParamMergeMode::Weighted;
            else if (*mode == "Multiplicative") mergeMode = ParamMergeMode::Multiplicative;
            else if (*mode == "Forced") mergeMode = ParamMergeMode::Forced;
            else mergeMode = ParamMergeMode::Passthrough;
        }

        if (auto points = data.get_child_optional("axis_points")) {
            axisPoints[0].clear();
            axisPoints[1].clear();
            std::size_t axis = 0;
            for (const auto& axisNode : *points) {
                if (axis > 1) break;
                for (const auto& valNode : axisNode.second) {
                    axisPoints[axis].push_back(valNode.second.get_value<float>());
                }
                ++axis;
            }
            if (axisPoints[0].empty()) axisPoints[0] = {0.0f, 1.0f};
            if (!isVec2 || axisPoints[1].empty()) axisPoints[1] = {0.0f};
        }

        if (auto bindingEntries = data.get_child_optional("bindings")) {
            for (const auto& bindingNode : *bindingEntries) {
                const auto& child = bindingNode.second;
                auto paramName = child.get<std::string>("param_name", "");
                if (paramName.empty()) continue;
                int paramId = child.get<int>("param_name", -1);

                std::shared_ptr<ParameterBinding> binding;
                if (paramName == "deform") {
                    binding = receiverLifetime ? std::make_shared<DeformationParameterBinding>(receiverLifetime)
                    : std::make_shared<DeformationParameterBinding>(this);
                } else if (paramName == "X" || paramName == "Y" || paramId == 0 || paramId == 1) {
                    binding = receiverLifetime ? makeParameterParameterBinding(receiverLifetime)
                    : makeParameterParameterBinding(this);
                } else {
                    binding = receiverLifetime ? std::make_shared<ValueParameterBinding>(receiverLifetime)
                    : std::make_shared<ValueParameterBinding>(this);
                }
                if (!binding) continue;
                if (auto err = binding->deserializeFromFghj(child)) return err;
                this->bindings.push_back(binding);
            }
        }
    } catch (const std::exception& ex) {
        return std::string(ex.what());
    }
    return std::nullopt;
}

inline void Parameter::reconstruct(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto puppetValue = puppet;
    const auto iteration = bindings;
    for (auto binding : iteration) {
        binding->reconstruct(puppetValue);
    }
}

inline void Parameter::finalize(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto puppetValue = puppet;
    makeIndexable();
    value = defaults;

    common::SharedArray<std::shared_ptr<ParameterBinding>> valid;
    auto snapshot = bindings;
    for (auto b : snapshot) {
        // The source reevaluates the virtual UUID getter for the second lookup.
        auto targetUuid = b->getNodeUUID();
        // D parity: keep binding when either Node or Parameter UUID resolves.
        bool validTarget = false;
        if (::nicxlive::core::resolvePuppetNodeById(puppetValue, targetUuid)) {
            validTarget = true;
        } else if (::nicxlive::core::resolvePuppetParameterById(puppetValue, b->getNodeUUID())) {
            validTarget = true;
        }
        if (validTarget) {
            b->finalize(puppetValue);
            valid.push_back(b);
        }
    }
    bindings = std::move(valid);
}

inline void Parameter::update() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!active) return;
    previousInternal = latestInternal;
    auto sum = iadd.csum();
    auto mul = imul.avg();
    latestInternal = Vec2{
        (value.x + sum.x) * mul.x,
        (value.y + sum.y) * mul.y,
    };

    Vec2u left{};
    Vec2 sub{};
    const auto internalValue = latestInternal;
    const auto mapped = mapValue(internalValue);
    findOffset(mapped, left, sub);
    const auto iteration = bindings;
    for (auto binding : iteration) {
        binding->apply(left, sub);
        if (auto node = std::dynamic_pointer_cast<Node>(d_runtime::ngRequireObject(binding)->getTarget().getResource()))
            if (valueChanged()) node->notifyChange(node);
    }
    iadd.clear();
    imul.clear();
}

class ParameterParameterBinding : public ParameterBindingImpl<float> {
public:
    explicit ParameterParameterBinding(OwnedBindingConstruction tag)
        : ParameterBindingImpl<float>(tag) {}
    using ParameterBindingImpl<float>::deserializeFromFghj;
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) override;

    ParameterParameterBinding(Parameter* parameter, const std::shared_ptr<Parameter>& targetParameter, int axis);
    explicit ParameterParameterBinding(Parameter* parameter);
    explicit ParameterParameterBinding(std::shared_ptr<Parameter> parameter)
        : ParameterBindingImpl<float>(std::move(parameter)) {}
    ParameterParameterBinding(std::shared_ptr<Parameter> parameter,
                              const std::shared_ptr<Parameter>& targetParameter, int axis)
        : ParameterBindingImpl<float>(std::move(parameter), std::static_pointer_cast<Resource>(targetParameter),
              axis == 0 ? common::NullableString::fromStaticLiteral("X")
                        : common::NullableString::fromStaticLiteral("Y")), axisId(axis) { clear(); }

    virtual std::shared_ptr<Parameter> targetParam() const;
    virtual int paramId() const;
    virtual std::shared_ptr<Parameter> targetParameter() const { return targetParam(); }
    virtual int paramAxis() const { return paramId(); }
    BindTarget getTarget() const override;

    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    void applyToTarget(const float& value) override;
    void clearValue(float& v) override;
    bool isCompatibleWithNode(const std::shared_ptr<Node>& /*other*/) const override;
    bool isCompatibleWithResource(const std::shared_ptr<Resource>&) const override { return false; }

private:
    int axisId{0};
};

inline ParameterParameterBinding::ParameterParameterBinding(Parameter* parameter, const std::shared_ptr<Parameter>& targetParameter, int axis)
    : ParameterBindingImpl<float>(parameter, std::static_pointer_cast<Resource>(targetParameter),
          axis == 0 ? common::NullableString::fromStaticLiteral("X")
                    : common::NullableString::fromStaticLiteral("Y")), axisId(axis) {
    clear();
}

inline ParameterParameterBinding::ParameterParameterBinding(Parameter* parameter)
    : ParameterBindingImpl<float>(parameter) {}

inline std::shared_ptr<Parameter> ParameterParameterBinding::targetParam() const { return target.parameterTarget.lock(); }
inline int ParameterParameterBinding::paramId() const { return target.name == "X" ? 0 : 1; }
inline BindTarget ParameterParameterBinding::getTarget() const {
    auto result = target;
    return result;
}

inline ::nicxlive::core::serde::SerdeException ParameterParameterBinding::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    if (auto axis = data.get_optional<int>("param_name")) {
        axisId = (*axis == 0) ? 0 : 1;
        target.name = (axisId == 0) ? "X" : "Y";
    } else {
        target.name = data.get<std::string>("param_name", "X");
        axisId = (target.name == "Y") ? 1 : 0;
    }
    return ParameterBindingImpl<float>::deserializeFromFghj(data);
}

inline void ParameterParameterBinding::applyToTarget(const float& value) {
    const auto sourceValue = value;
    [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
    // Each source property evaluation can observe a callback changing the target.
    const bool previousChanged = d_runtime::ngRequireObject(targetParam())->valueChanged();
    Vec2 parameterValue = d_runtime::ngRequireObject(targetParam())->latestInternal;
    if (paramId() == 0) parameterValue.x = sourceValue;
    else parameterValue.y = sourceValue;
    const auto receiver = targetParam();
    const auto axis = paramId();
    const auto selectedValue = paramId() == 0 ? parameterValue.x : parameterValue.y;
    d_runtime::ngRequireObject(receiver)->pushIOffsetAxis(axis, selectedValue, ParamMergeMode::Forced);
    const bool changed = d_runtime::ngRequireObject(targetParam())->valueChanged();
    if (!previousChanged && changed) d_runtime::ngRequireObject(targetParam())->update();
}

inline void ParameterParameterBinding::clearValue(float& value) {
    [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
    value = paramId() == 0 ? d_runtime::ngRequireObject(targetParam())->defaults.x
                            : d_runtime::ngRequireObject(targetParam())->defaults.y;
}

inline bool ParameterParameterBinding::isCompatibleWithNode(const std::shared_ptr<Node>& /*other*/) const {
    return false;
}

inline std::shared_ptr<ParameterBinding> makeParameterParameterBinding(std::shared_ptr<Parameter> parameter) {
    return std::make_shared<ParameterParameterBinding>(std::move(parameter));
}

inline std::shared_ptr<ParameterBinding> makeParameterParameterBinding(Parameter* parameter) {
    return std::make_shared<ParameterParameterBinding>(parameter);
}

inline std::shared_ptr<ParameterBinding> Parameter::createBinding(const std::shared_ptr<Node>& target, const std::string& key, bool setZero) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return createBinding(target, common::NullableString(key), setZero);
}
inline std::shared_ptr<ParameterBinding> Parameter::createBinding(const std::shared_ptr<Node>& target,
                                                                 common::NullableString key, bool setZero) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto resource = std::static_pointer_cast<Resource>(target);
    std::shared_ptr<ParameterBinding> binding;
    if (key == "deform") binding = receiverLifetime ? ParameterBindingImpl<DeformSlot>::createSource<DeformationParameterBinding>(receiverLifetime, resource, key)
                    : std::make_shared<DeformationParameterBinding>(this, resource, key);
    else binding = receiverLifetime ? ParameterBindingImpl<float>::createSource<ValueParameterBinding>(receiverLifetime, resource, key)
                    : std::make_shared<ValueParameterBinding>(this, resource, key);
    if (setZero) {
        const auto index = findClosestKeypoint({0, 0});
        const auto zero = getKeypointValue(index);
        if (std::abs(zero.x) < 0.001f && std::abs(zero.y) < 0.001f) binding->reset(index);
    }
    return binding;
}

inline std::shared_ptr<ParameterBinding> Parameter::createBinding(const std::shared_ptr<Parameter>& target, int axis, bool setZero) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto binding = receiverLifetime ? ParameterBindingImpl<float>::createSource<ParameterParameterBinding>(receiverLifetime,
            std::static_pointer_cast<Resource>(target), axis == 0 ? common::NullableString::fromStaticLiteral("X")
                                                               : common::NullableString::fromStaticLiteral("Y"))
                                    : std::make_shared<ParameterParameterBinding>(this, target, axis);
    if (setZero) {
        const auto index = findClosestKeypoint({0, 0});
        const auto zero = getKeypointValue(index);
        if (std::abs(zero.x) < 0.001f && std::abs(zero.y) < 0.001f) binding->reset(index);
    }
    return binding;
}

inline std::shared_ptr<ParameterBinding> Parameter::getBinding(const std::shared_ptr<Resource>& resource,
                                                              common::NullableString key) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto retainedResource = resource;
    const auto snapshot = bindings;
    for (std::size_t index = 0; index < snapshot.size(); ++index) {
        if (!ngResourceObjectEquals(d_runtime::ngRequireObject(snapshot[index])->getTarget().getResource(),
                retainedResource)) continue;
        if (d_runtime::ngRequireObject(snapshot[index])->getNameDescriptor() == key) return snapshot[index];
    }
    return nullptr;
}
inline bool Parameter::hasBinding(const std::shared_ptr<Resource>& resource, common::NullableString key) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto retainedResource = resource;
    const auto snapshot = bindings;
    for (std::size_t index = 0; index < snapshot.size(); ++index) {
        if (!ngResourceObjectEquals(d_runtime::ngRequireObject(snapshot[index])->getTarget().getResource(),
                retainedResource)) continue;
        if (d_runtime::ngRequireObject(snapshot[index])->getTarget().name == key) return true;
    }
    return false;
}
inline bool Parameter::hasAnyBinding(const std::shared_ptr<Resource>& resource) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto retainedResource = resource;
    const auto snapshot = bindings;
    for (auto binding : snapshot)
        if (ngResourceObjectEquals(d_runtime::ngRequireObject(binding)->getTarget().getResource(),
                retainedResource)) return true;
    return false;
}
inline std::shared_ptr<ParameterBinding> Parameter::createBinding(const std::shared_ptr<Resource>& resource,
                                                                 common::NullableString key, bool setZero) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto node = std::dynamic_pointer_cast<Node>(resource)) return createBinding(node, key, setZero);
    if (auto parameter = std::dynamic_pointer_cast<Parameter>(resource))
        // Source Resource dispatch intentionally uses this overload's default setZero.
        return createBinding(parameter, key == "X" ? 0 : 1);
    return nullptr;
}
inline std::shared_ptr<ParameterBinding> Parameter::getOrAddBinding(const std::shared_ptr<Resource>& resource,
                                                                  common::NullableString key, bool setZero) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto retainedResource = resource;
    auto binding = getBinding(retainedResource, key);
    if (!binding) { binding = createBinding(retainedResource, key, setZero); addBinding(binding); }
    return binding;
}

inline std::shared_ptr<ParameterBinding> Parameter::createBinding(const BindTarget& target, bool setZero) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return createBinding(target.getResource(), target.name, setZero);
}

inline std::shared_ptr<Parameter> Parameter::dup() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto result = Parameter::createSourceDeferred([&] {
        const auto originalName = getName();
        try {
            std::string copiedName(originalName.view());
            copiedName += " (Copy)";
            return common::NullableString(std::move(copiedName));
        } catch (const std::bad_alloc&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        }
    }, [&] { return isVec2; });
    result->min = min;
    result->max = max;
    {
        const auto copiedAxes = duplicateSourceAxisDescriptors();
        result->axisPoints[0] = copiedAxes[0];
        result->axisPoints[1] = copiedAxes[1];
    }
    const auto sourceBindings = bindings;
    for (const auto binding : sourceBindings) {
        std::shared_ptr<ParameterBinding> copied;
        if (binding) {
            // The actual ParameterBinding cast precedes the two virtual reads.
            const auto resource = d_runtime::ngRequireObject(binding)->getResourceTarget();
            const auto name = d_runtime::ngRequireObject(binding)->getNameDescriptor();
            copied = result->createBinding(resource, name, false);
        }
        const auto mode = d_runtime::ngRequireObject(binding)->interpolateMode();
        d_runtime::ngRequireObject(copied)->setInterpolateMode(mode);
        const auto xCount = axisPointCount(0);
        for (std::uint32_t x = 0; x < xCount; ++x) {
            const auto yCount = axisPointCount(1);
            for (std::uint32_t y = 0; y < yCount; ++y)
                d_runtime::ngRequireObject(binding)->copyKeypointToBinding({x, y}, copied, {x, y});
        }
        result->addBinding(copied);
    }
    return result;
}

// --- Serialization helpers (template) ---
namespace detail {
inline boost::property_tree::ptree serializeValueNode(const float& v) {
    boost::property_tree::ptree node;
    node.put("", v);
    return node;
}

inline boost::property_tree::ptree serializeValueNode(const DeformSlot& v) {
    boost::property_tree::ptree node;
    boost::property_tree::ptree xs;
    boost::property_tree::ptree ys;
    for (std::size_t i = 0; i < v.vertexOffsets.size(); ++i) {
        boost::property_tree::ptree xv;
        xv.put("", v.vertexOffsets.xAt(i));
        xs.push_back({"", xv});
        boost::property_tree::ptree yv;
        yv.put("", v.vertexOffsets.yAt(i));
        ys.push_back({"", yv});
    }
    node.add_child("x", xs);
    node.add_child("y", ys);
    return node;
}

inline void deserializeValueNode(const boost::property_tree::ptree& node, float& out) {
    out = node.get_value<float>();
}

inline void deserializeValueNode(const boost::property_tree::ptree& node, DeformSlot& out) {
    static const boost::property_tree::ptree empty{};
    if (const auto voIt = node.find("vertexOffsets"); voIt != node.not_found()) {
        deserializeValueNode(voIt->second, out);
        return;
    }
    const auto xsIt = node.find("x");
    const auto ysIt = node.find("y");
    if (xsIt != node.not_found() && ysIt != node.not_found()) {
        const auto& xs = xsIt->second;
        const auto& ys = ysIt->second;
        std::size_t n = std::min(xs.size(), ys.size());
        out.vertexOffsets.resize(n);
        std::size_t idx = 0;
        for (auto it = xs.begin(); it != xs.end() && idx < n; ++it, ++idx) {
            out.vertexOffsets.xAt(idx) = it->second.get_value<float>();
        }
        idx = 0;
        for (auto it = ys.begin(); it != ys.end() && idx < n; ++it, ++idx) {
            out.vertexOffsets.yAt(idx) = it->second.get_value<float>();
        }
        return;
    }

    // D nijilive format: deform slot is a list of vec2 entries (each vec2 serialized as [x, y]).
    std::vector<float> xs;
    std::vector<float> ys;
    xs.reserve(node.size());
    ys.reserve(node.size());
    for (const auto& elem : node) {
        const auto& v = elem.second;
        float x = 0.0f;
        float y = 0.0f;
        bool parsed = false;

        if (auto ox = v.get_optional<float>("x")) {
            x = *ox;
            y = v.get<float>("y", 0.0f);
            parsed = true;
        } else {
            auto it = v.begin();
            if (it != v.end()) {
                x = it->second.get_value<float>();
                ++it;
                if (it != v.end()) y = it->second.get_value<float>();
                parsed = true;
            }
        }

        if (parsed) {
            xs.push_back(x);
            ys.push_back(y);
        }
    }
    out.vertexOffsets.resize(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i) {
        out.vertexOffsets.set(i, Vec2{xs[i], ys[i]});
    }
}
} // namespace detail

template <typename T>
inline T cubicValue(const T& p0, const T& p1, const T& p2, const T& p3, float t) {
    T a, b, c;
    if constexpr (std::is_same_v<T, float>) {
        a = static_cast<float>(-0.5 * p0 + 1.5 * p1 - 1.5 * p2 + 0.5 * p3);
        b = static_cast<float>(p0 - 2.5 * p1 + 2 * p2 - 0.5 * p3);
        c = static_cast<float>(-0.5 * p0 + 0.5 * p2);
    } else {
        a = p0 * -0.5f + p1 * 1.5f - p2 * 1.5f + p3 * 0.5f;
        b = p0 - p1 * 2.5f + p2 * 2.0f - p3 * 0.5f;
        c = p0 * -0.5f + p2 * 0.5f;
    }
    const T d = p1;
    return a * ((t * t) * t) + b * (t * t) + c * t + d;
}

template <typename T>
::nicxlive::core::serde::SerdeException ParameterBindingImpl<T>::serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const {
    [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
    serializer.putKey("node");
    serializer.putValue(getNodeUUID());
    serializer.putKey("param_name");
    serializer.putValue(target.name.view());
    serializer.putKey("interpolate_mode");
    serializer.putValue(static_cast<int>(interpolateMode_));

    boost::property_tree::ptree valuesTree;
    for (const auto& row : values) {
        boost::property_tree::ptree rowNode;
        for (const auto& v : row) {
            rowNode.push_back({"", detail::serializeValueNode(v)});
        }
        valuesTree.push_back({"", rowNode});
    }
    serializer.root.add_child("values", valuesTree);

    boost::property_tree::ptree isSetTree;
    for (const auto& row : isSetFlags) {
        boost::property_tree::ptree rowNode;
        for (bool b : row) {
            boost::property_tree::ptree v;
            v.put("", b);
            rowNode.push_back({"", v});
        }
        isSetTree.push_back({"", rowNode});
    }
    serializer.root.add_child("isSet", isSetTree);

    return std::nullopt;
}

template <typename T>
::nicxlive::core::serde::SerdeException ParameterBindingImpl<T>::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
    nodeUuid_ = data.get<uint32_t>("node", 0);
    target.uuid = nodeUuid_;
    target.name = data.get<std::string>("param_name", target.name);
    interpolateMode_ = InterpolateMode::Linear;
    if (auto modeInt = data.get_optional<int>("interpolate_mode")) {
        interpolateMode_ = static_cast<InterpolateMode>(*modeInt);
    } else if (auto modeStr = data.get_optional<std::string>("interpolate_mode")) {
        auto mode = *modeStr;
        std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (mode == "nearest") interpolateMode_ = InterpolateMode::Nearest;
        else if (mode == "linear") interpolateMode_ = InterpolateMode::Linear;
        else if (mode == "cubic") interpolateMode_ = InterpolateMode::Cubic;
        else if (mode == "step") interpolateMode_ = InterpolateMode::Step;
    }

    static const boost::property_tree::ptree empty{};
    const auto valuesIt = data.find("values");
    const auto isSetIt = data.find("isSet");
    const auto& valuesTree = valuesIt != data.not_found() ? valuesIt->second : empty;
    const auto& isSetTree = isSetIt != data.not_found() ? isSetIt->second : empty;

    auto xCount = parameterAccess().pointer ? parameterAccess()->axisPointCount(0) : static_cast<std::size_t>(valuesTree.size());
    auto yCount = parameterAccess().pointer ? parameterAccess()->axisPointCount(1) : 0u;
    if (yCount == 0 && !valuesTree.empty()) yCount = valuesTree.front().second.size();

    values = {};
    isSetFlags = {};
    values.resize(xCount);
    isSetFlags.resize(xCount);
    for (std::size_t x = 0; x < xCount; ++x) {
        values[x].resize(yCount);
        isSetFlags[x].resize(yCount);
    }

    std::size_t xi = 0;
    for (auto& row : valuesTree) {
        if (xi >= values.size()) break;
        std::size_t yi = 0;
        for (auto& valNode : row.second) {
            if (yi >= values[xi].size()) break;
            detail::deserializeValueNode(valNode.second, values[xi][yi]);
            yi++;
        }
        xi++;
    }

    xi = 0;
    for (auto& row : isSetTree) {
        if (xi >= isSetFlags.size()) break;
        std::size_t yi = 0;
        for (auto& valNode : row.second) {
            if (yi >= isSetFlags[xi].size()) break;
            isSetFlags[xi][yi] = valNode.second.get_value<bool>();
            yi++;
        }
        xi++;
    }

    if (parameterAccess().pointer) {
        const auto xCount = parameterAccess()->axisPointCount(0);
        const auto yCount = parameterAccess()->axisPointCount(1);
        if (values.size() != xCount) return std::string("Mismatched X value count");
        for (const auto& row : values) {
            if (row.size() != yCount) return std::string("Mismatched Y value count");
        }
        if (isSetFlags.size() != xCount) return std::string("Mismatched X isSet count");
        for (const auto& row : isSetFlags) {
            if (row.size() != yCount) return std::string("Mismatched Y isSet count");
        }
    }

    return std::nullopt;
}


// The typed path consumes original JSON kinds and first duplicate lookup.
template <typename T>
serde::ReadError ParameterBindingImpl<T>::serializeSelf(serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
    if constexpr (!std::is_same_v<T, float> && !std::is_same_v<T, DeformSlot>) {
        throw std::logic_error("Typed deformation binding writer is not implemented");
    } else {
        const auto sourceValues = values;
        for (std::size_t x = 0; x < sourceValues.size(); ++x) {
            const auto row = sourceValues[x];
            for (std::size_t y = 0; y < row.size(); ++y) {
                const auto value = row[y];
                if constexpr (std::is_same_v<T, float>) {
                    if (!std::isfinite(value)) {
                        std::ostringstream message;
                        message << "Cannot save parameter '" << parameterAccess()->getName() << "', node " << getNodeUUID()
                            << ", binding '" << target.name << "', key [" << x << ',' << y << "]: " << value;
                        throw d_runtime::ThrowableException(d_runtime::RetainedString(message.str()));
                    }
                } else {
                    const auto& offsets = value.vertexOffsets;
                    for (std::size_t vertex = 0; vertex < offsets.size(); ++vertex) {
                        if (x < recoveryMasks.size() && y < recoveryMasks[x].size() &&
                            vertex < recoveryMasks[x][y].size() && recoveryMasks[x][y][vertex])
                            throw d_runtime::ThrowableException(d_runtime::RetainedString("Cannot save parameter '" + parameterAccess()->getName() + "', node " +
                                std::to_string(target.getResource() ? target.getResource()->getUUID() : nodeUuid_) +
                                ", binding '" + target.name + "', key [" + std::to_string(x) + "," + std::to_string(y) +
                                "], vertex " + std::to_string(vertex) + ": missing deformation has not been reconstructed"));
                        for (unsigned component = 0; component < 2; ++component) {
                            const float number = component == 0 ? offsets.xAt(vertex) : offsets.yAt(vertex);
                            if (!std::isfinite(number)) {
                                std::ostringstream message;
                                message << "Cannot save parameter '" << parameterAccess()->getName() << "', node " << getNodeUUID()
                                    << ", binding '" << target.name << "', key [" << x << ',' << y << "], vertex "
                                    << vertex << ", component " << component << ": " << number;
                                throw d_runtime::ThrowableException(d_runtime::RetainedString(message.str()));
                            }
                        }
                    }
                }
            }
        }
        const auto object = serializer.structBegin();
        serializer.putKey("node");
        serializer.putValue(d_runtime::ngRequireObject(target.getResource())->getUUID());
        serializer.putKey("param_name");
        serializer.putValue(target.name.view());
        serializer.putField("values", values);
        serializer.putField("isSet", isSetFlags);
        constexpr std::string_view names[]{"Nearest", "Linear", "Stepped", "Cubic", "Bezier", "COUNT"};
        serializer.putKey("interpolate_mode");
        const auto index = static_cast<unsigned>(interpolateMode_);
        if (index >= std::size(names)) d_runtime::ngAssertUnreachable(nullptr, __FILE__, __LINE__);
        serializer.putValue(names[index]);
        serializer.structEnd(object);
        return {};
    }
}

template <typename T>
serde::ReadError ParameterBindingImpl<T>::serializeSelf(serde::TypedJsonWriter& serializer) {
    [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
    const auto fail = [](std::string message, std::size_t line) {
        throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), line);
    };
    const auto sourceValues = values;
    for (std::size_t x = 0; x < sourceValues.size(); ++x) {
        const auto row = sourceValues[x];
        for (std::size_t y = 0; y < row.size(); ++y) {
            const auto value = row[y];
            if constexpr (std::is_same_v<T, DeformSlot>) {
                const auto offsets = value.vertexOffsets;
                for (std::size_t vertex = 0; vertex < offsets.size(); ++vertex) {
                    if (x < recoveryMasks.size() && y < recoveryMasks[x].size() &&
                        vertex < recoveryMasks[x][y].size() && recoveryMasks[x][y][vertex]) {
                        const auto parameterName = parameterAccess()->getName();
                        const auto resource = target.getResource();
                        const auto nodeId = resource ? resource->getUUID() : nodeUuid_;
                        const auto bindingName = target.name;
                        fail("Cannot save parameter '" + parameterName.string() + "', node " +
                            std::to_string(nodeId) + ", binding '" + bindingName.string() + "', key [" +
                            std::to_string(x) + "," + std::to_string(y) + "], vertex " +
                            std::to_string(vertex) + ": missing deformation has not been reconstructed", 312);
                    }
                    // Source foreach copies this vector before iterating its components.
                    const auto offset = offsets.at(vertex);
                    for (unsigned component = 0; component < 2; ++component) {
                        const float number = component ? offset.y : offset.x;
                        if (!std::isfinite(number)) {
                            const auto parameterName = parameterAccess()->getName();
                            const auto nodeId = getNodeUUID();
                            const auto bindingName = target.name;
                            fail("Cannot save parameter '" + parameterName.string() + "', node " +
                                std::to_string(nodeId) + ", binding '" + bindingName.string() + "', key [" +
                                std::to_string(x) + "," + std::to_string(y) + "], vertex " +
                                std::to_string(vertex) + ", component " + std::to_string(component) + ": " +
                                d_runtime::decimal_format::ngFormatDecimal(number), 316);
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, float>) {
                if (!std::isfinite(value)) {
                    const auto parameterName = parameterAccess()->getName();
                    const auto nodeId = getNodeUUID();
                    const auto bindingName = target.name;
                    fail("Cannot save parameter '" + parameterName.string() + "', node " +
                        std::to_string(nodeId) + ", binding '" + bindingName.string() + "', key [" +
                        std::to_string(x) + "," + std::to_string(y) + "]: " +
                        d_runtime::decimal_format::ngFormatDecimal(value), 322);
                }
            }
        }
    }
    auto state = serializer.structBegin();
    serializer.putKey("node");
    serializer.putValue(d_runtime::ngRequireObject(target.getResource())->getUUID());
    serializer.putKey("param_name");
    serializer.putValue(target.name.view());
    serializer.putKey("values");
    serializer.serializeValue(values);
    serializer.putKey("isSet");
    serializer.serializeValue(isSetFlags);
    serializer.putKey("interpolate_mode");
    constexpr std::string_view names[]{"Nearest", "Linear", "Stepped", "Cubic", "Bezier", "COUNT"};
    const auto index = static_cast<unsigned>(interpolateMode_);
    if (index >= std::size(names)) d_runtime::ngAssertUnreachable(nullptr, __FILE__, __LINE__);
    serializer.putValue(names[index]);
    serializer.structEnd(state);
    return {};
}

template <typename T>
serde::ReadError ParameterBindingImpl<T>::deserializeFromFghj(serde::TypedJsonView data) {
    [[maybe_unused]] const auto bindingLifetime = this->weak_from_this().lock();
    (void)serde::deserializeValue(data.findFirst("node"), nodeUuid_);
    target.uuid = nodeUuid_; // Native mirror, never a second Source target.
    (void)readBindingResourceString(data.findFirst("param_name"), target.name);
    values.resize(0);
    if constexpr (std::is_same_v<T, DeformSlot>) recoveryMasks.resize(0);
    std::size_t x = 0;
    for (auto row : data.findFirst("values").byElement()) {
        common::SharedArray<T> decoded;
        common::SharedArray<common::SharedArray<std::uint8_t>> decodedMasks;
        std::size_t y = 0;
        for (auto cell : row.byElement()) {
            T value{};
            if constexpr (std::is_floating_point_v<T>) value = std::numeric_limits<T>::quiet_NaN();
            const auto parameterName = parameterAccess()->getName();
            const auto nodeId = nodeUuid_;
            const auto bindingName = target.name;
            const auto context = "Parameter '" + parameterName.string() + "', node " +
                std::to_string(nodeId) + ", binding '" + bindingName.string() + "', key [" +
                std::to_string(x) + "," + std::to_string(y) + "]";
            try {
                serde::ReadError error;
                if constexpr (std::is_same_v<T, float>) error = serde::deserializeNumber(cell, value);
                else if constexpr (requires { value.deserializeFromFghj(cell); })
                    error = value.deserializeFromFghj(cell);
                else {
                    using serde::deserializeValue;
                    error = deserializeValue(cell, value);
                }
                if (error) throw d_runtime::ThrowableException(error->msg,
                    d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 366);
            } catch (d_runtime::ThrowableException& error) {
                auto message = context + ": ";
                if (!error.msg.empty()) message.append(error.msg.view());
                throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
                    d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), 368,
                    d_runtime::ngRetainCurrentThrowable(error));
            }
            if constexpr (std::is_same_v<T, DeformSlot>) {
                std::size_t recovered = 0;
                common::SharedArray<std::uint8_t> missing(value.vertexOffsets.size(), 0);
                for (std::size_t vertex = 0; vertex < value.vertexOffsets.size(); ++vertex) {
                    auto offset = value.vertexOffsets.at(vertex);
                    for (unsigned axis = 0; axis < 2; ++axis) {
                        float& component = axis ? offset.y : offset.x;
                        if (!std::isfinite(component)) {
                            missing[vertex] |= static_cast<std::uint8_t>(1 << axis);
                            component = 0;
                            ++recovered;
                        }
                    }
                    value.vertexOffsets.set(vertex, offset);
                }
                decodedMasks.push_back(missing);
                if (recovered) serde::inRecordLoadRecovery(context, recovered);
            }
            decoded.push_back(value);
            ++y;
        }
        values.push_back(decoded);
        if constexpr (std::is_same_v<T, DeformSlot>) recoveryMasks.push_back(decodedMasks);
        ++x;
    }
    (void)serde::deserializeValue(data.findFirst("isSet"), isSetFlags);
    const auto mode = data.findFirst("interpolate_mode");
    if (mode.isMissing()) interpolateMode_ = InterpolateMode::Linear;
    else {
        // FGHJ enum decoding ignores the scoped-string conversion error.
        const auto name = mode.kind() == serde::JsonKind::String ? mode.stringBytes() : std::string_view{};
        constexpr std::string_view names[]{"Nearest", "Linear", "Stepped", "Cubic", "Bezier", "COUNT"};
        bool found = false;
        for (unsigned index = 0; index < std::size(names); ++index) {
            if (name == names[index]) {
                interpolateMode_ = static_cast<InterpolateMode>(index); found = true; break;
            }
        }
        if (!found) throw d_runtime::ThrowableException(d_runtime::RetainedString(
            "Unable to deserialize string '" + std::string(name) +
            "' to InterpolateModeAllowed keys:allowedKeys"),
            d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
    }
    const auto xCount = parameterAccess()->axisPointCount(0);
    const auto yCount = parameterAccess()->axisPointCount(1);
    const auto enforce = [](bool condition, const char* message, std::size_t line) {
        if (!condition) throw d_runtime::ThrowableException(d_runtime::RetainedString(message),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/param/binding.d"), line);
    };
    enforce(values.size() == xCount, "Mismatched X value count", 404);
    for (auto row : values) enforce(row.size() == yCount, "Mismatched Y value count", 406);
    enforce(isSetFlags.size() == xCount, "Mismatched X isSet_ count", 409);
    for (auto row : isSetFlags) enforce(row.size() == yCount, "Mismatched Y isSet_ count", 411);
    return {};
}

inline serde::ReadError ParameterParameterBinding::deserializeFromFghj(serde::TypedJsonView data) {
    int axis = 0;
    if (!serde::deserializeValue(data.findFirst("param_name"), axis)) target.name = axis == 0 ? "X" : "Y";
    try {
        auto error = ParameterBindingImpl<float>::deserializeFromFghj(data);
        axisId = target.name == "X" ? 0 : 1;
        return error;
    } catch (...) {
        axisId = target.name == "X" ? 0 : 1;
        throw;
    }
}

} // namespace nicxlive::core::param
