#pragma once

#include "../common/utils.hpp"
#include "../nodes/common.hpp"
#include "../nodes/node.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
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

struct Vec2u {
    std::size_t x{};
    std::size_t y{};
};

enum class ParamMergeMode {
    Additive,
    Weighted,
    Multiplicative,
    Forced,
    Passthrough,
};

class Resource {
public:
    virtual ~Resource() = default;
};

struct BindTarget {
    std::weak_ptr<Node> node{};
    std::string name{};
};

class Parameter;

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

inline float lerpValue(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

struct DeformSlot {
    Vec2Array vertexOffsets{};
};

inline DeformSlot scaleValue(const DeformSlot& in, float scale) {
    DeformSlot out = in;
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i) {
        out.vertexOffsets.x[i] *= scale;
        out.vertexOffsets.y[i] *= scale;
    }
    return out;
}

inline DeformSlot lerpValue(const DeformSlot& a, const DeformSlot& b, float t) {
    if (a.vertexOffsets.size() != b.vertexOffsets.size()) {
        return t < 0.5f ? a : b;
    }
    DeformSlot out;
    out.vertexOffsets = Vec2Array(a.vertexOffsets.size());
    for (std::size_t i = 0; i < a.vertexOffsets.size(); ++i) {
        out.vertexOffsets.x[i] = lerpValue(a.vertexOffsets.x[i], b.vertexOffsets.x[i], t);
        out.vertexOffsets.y[i] = lerpValue(a.vertexOffsets.y[i], b.vertexOffsets.y[i], t);
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
    DeformSlot out;
    auto n = std::min(a.vertexOffsets.size(), b.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.x[i] = a.vertexOffsets.x[i] + b.vertexOffsets.x[i];
        out.vertexOffsets.y[i] = a.vertexOffsets.y[i] + b.vertexOffsets.y[i];
    }
    return out;
}

inline DeformSlot operator-(const DeformSlot& a, const DeformSlot& b) {
    DeformSlot out;
    auto n = std::min(a.vertexOffsets.size(), b.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.x[i] = a.vertexOffsets.x[i] - b.vertexOffsets.x[i];
        out.vertexOffsets.y[i] = a.vertexOffsets.y[i] - b.vertexOffsets.y[i];
    }
    return out;
}

inline DeformSlot operator*(const DeformSlot& a, float s) {
    return scaleValue(a, s);
}

class Parameter : public Resource, public std::enable_shared_from_this<Parameter> {
public:
    uint32_t uuid{0};
    std::string name{};
    std::string indexableName{};
    bool active{true};
    Vec2 value{0, 0};
    Vec2 latestInternal{0, 0};
    Vec2 previousInternal{0, 0};
    ParamMergeMode mergeMode{ParamMergeMode::Additive};
    Vec2 defaults{0, 0};
    bool isVec2{false};
    Vec2 min{0, 0};
    Vec2 max{1, 1};
    std::array<std::vector<float>, 2> axisPoints{{{0, 1}, {0, 1}}};
    std::map<std::string, std::shared_ptr<ParameterBinding>> bindingMap{};

    Parameter() = default;
    Parameter(const std::string& n, bool vec2)
        : name(n), isVec2(vec2) {
        if (!isVec2) {
            axisPoints[1] = {0};
        }
    }

    std::size_t axisPointCount(std::size_t axis) const {
        if (axis == 0) return axisPoints[0].size();
        if (!isVec2) return 1;
        return axisPoints[1].size();
    }

    float axisPointValue(std::size_t axis, std::size_t idx) const {
        const auto& arr = axisPoints[axis == 0 ? 0 : 1];
        if (idx < arr.size()) return arr[idx];
        return 0.0f;
    }

    Vec2 getKeypointOffset(const Vec2u& idx) const {
        float x = (idx.x < axisPoints[0].size()) ? axisPoints[0][idx.x] : 0.0f;
        float y = (!isVec2 || idx.y >= axisPoints[1].size()) ? 0.0f : axisPoints[1][idx.y];
        return Vec2{x, y};
    }

    void findOffset(const Vec2& offset, Vec2u& leftKeypoint, Vec2& subOffset) const {
        auto findAxis = [&](std::size_t axis, float val, std::size_t& left, float& frac) {
            const auto& arr = axisPoints[axis];
            if (arr.size() <= 1) {
                left = 0;
                frac = 0.0f;
                return;
            }
            left = 0;
            for (std::size_t i = 0; i + 1 < arr.size(); ++i) {
                if (val < arr[i + 1]) { left = i; break; }
                left = i;
            }
            std::size_t right = std::min(left + 1, arr.size() - 1);
            float denom = arr[right] - arr[left];
            frac = denom == 0.0f ? 0.0f : std::clamp((val - arr[left]) / denom, 0.0f, 1.0f);
        };

        findAxis(0, offset.x, leftKeypoint.x, subOffset.x);
        if (isVec2) {
            findAxis(1, offset.y, leftKeypoint.y, subOffset.y);
        } else {
            leftKeypoint.y = 0;
            subOffset.y = 0.0f;
        }
    }

    Vec2 normalizedValue() const {
        return Vec2{
            (value.x - min.x) / (max.x - min.x),
            (value.y - min.y) / (max.y - min.y),
        };
    }

    void setNormalizedValue(const Vec2& v) {
        value.x = v.x * (max.x - min.x) + min.x;
        value.y = v.y * (max.y - min.y) + min.y;
    }

    std::shared_ptr<ParameterBinding> getBinding(const std::shared_ptr<Node>& /*self*/, const std::string& key) const;
    std::shared_ptr<ParameterBinding> getOrAddBinding(const std::shared_ptr<Node>& target, const std::string& key);
    void removeBinding(const std::shared_ptr<ParameterBinding>& binding);
    void makeIndexable();
    void update();
};

template <typename T>
class ParameterBindingImpl : public ParameterBinding {
public:
    ParameterBindingImpl(Parameter* param, const std::shared_ptr<Node>& target, const std::string& paramName)
        : parameter(param) {
        setTarget(target, paramName);
        clear();
    }

    explicit ParameterBindingImpl(Parameter* param)
        : parameter(param) {
        clear();
    }

    BindTarget getTarget() const override { return target; }
    std::shared_ptr<Node> getNode() const override { return target.node.lock(); }
    void setTarget(const std::shared_ptr<Node>& node, const std::string& paramName) override {
        target.node = node;
        target.name = paramName;
    }
    std::string getName() const override { return target.name; }

    void apply(const Vec2u& leftKeypoint, const Vec2& offset) override {
        applyToTarget(interpolate(leftKeypoint, offset));
    }

    void clear() override {
        auto xCount = parameter ? parameter->axisPointCount(0) : 1;
        auto yCount = parameter ? parameter->axisPointCount(1) : 1;
        values.assign(xCount, std::vector<T>(yCount, T{}));
        isSetFlags.assign(xCount, std::vector<bool>(yCount, false));
        for (auto x = 0u; x < xCount; ++x) {
            for (auto y = 0u; y < yCount; ++y) {
                clearValue(values[x][y]);
            }
        }
    }

    void setCurrent(const Vec2u& point) override {
        if (point.x >= values.size() || point.y >= values[point.x].size()) return;
        isSetFlags[point.x][point.y] = true;
        reInterpolate();
    }

    void unset(const Vec2u& point) override {
        if (point.x >= values.size() || point.y >= values[point.x].size()) return;
        clearValue(values[point.x][point.y]);
        isSetFlags[point.x][point.y] = false;
        reInterpolate();
    }

    void reset(const Vec2u& point) override {
        if (point.x >= values.size() || point.y >= values[point.x].size()) return;
        clearValue(values[point.x][point.y]);
        isSetFlags[point.x][point.y] = true;
        reInterpolate();
    }

    bool isSet(const Vec2u& index) const override {
        if (index.x >= isSetFlags.size() || index.y >= isSetFlags[index.x].size()) return false;
        return isSetFlags[index.x][index.y];
    }

    void reverseAxis(uint32_t axis) override {
        if (axis == 0) {
            std::reverse(values.begin(), values.end());
            std::reverse(isSetFlags.begin(), isSetFlags.end());
        } else {
            for (auto& v : values) {
                std::reverse(v.begin(), v.end());
            }
            for (auto& s : isSetFlags) {
                std::reverse(s.begin(), s.end());
            }
        }
    }

    void reInterpolate() override {
        auto xCount = parameter ? parameter->axisPointCount(0) : values.size();
        auto yCount = parameter ? parameter->axisPointCount(1) : (values.empty() ? 0 : values[0].size());
        if (values.size() != xCount || (yCount > 0 && !values.empty() && values.front().size() != yCount)) {
            clear();
        }
        uint32_t setCount = 0;
        for (auto x = 0u; x < values.size(); ++x) {
            for (auto y = 0u; y < values[x].size(); ++y) {
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
        std::vector<std::vector<float>> interpDistance(xCount, std::vector<float>(yCount, 0.0f));
        for (auto x = 0u; x < xCount; ++x) {
            for (auto y = 0u; y < yCount; ++y) {
                valid[x][y] = (x < isSetFlags.size() && y < isSetFlags[x].size()) ? isSetFlags[x][y] : false;
            }
        }
        auto axisPoint = [&](bool yMajor, uint32_t idx) -> float {
            return parameter ? parameter->axisPointValue(yMajor ? 0 : 1, idx) : static_cast<float>(idx);
        };
        auto get = [&](bool yMajor, uint32_t maj, uint32_t min) -> T& {
            if (yMajor) return values[min][maj];
            return values[maj][min];
        };
        auto isValid = [&](bool yMajor, uint32_t maj, uint32_t min) -> bool {
            if (yMajor) return valid[min][maj];
            return valid[maj][min];
        };
        auto isNew = [&](bool yMajor, uint32_t maj, uint32_t min) -> bool {
            if (yMajor) return newlySet[min][maj];
            return newlySet[maj][min];
        };
        auto setPoint = [&](bool yMajor, uint32_t maj, uint32_t min, const T& val, float distance, bool commit) {
            if (yMajor) {
                values[min][maj] = val;
                interpDistance[min][maj] = distance;
                newlySet[min][maj] = true;
                if (commit) commitPoints.push_back(Vec2u{min, maj});
            } else {
                values[maj][min] = val;
                interpDistance[maj][min] = distance;
                newlySet[maj][min] = true;
                if (commit) commitPoints.push_back(Vec2u{maj, min});
            }
        };
        auto lerpT = [&](const T& a, const T& b, float t) {
            return lerpValue(a, b, t);
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
                        float off = (rightOff == leftOff) ? 0.0f : (midOff - leftOff) / (rightOff - leftOff);
                        auto val = lerpT(get(yMajor, i, l), get(yMajor, i, r), off);
                        if (secondPass && isNew(yMajor, i, m)) {
                            if (!detectedIntersections) commitPoints.clear();
                            setPoint(yMajor, i, m, lerpT(val, get(yMajor, i, m), 0.5f), 0.0f, true);
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
                T dX = values[baseX + offX][baseY] - base;
                T dY = values[baseX][baseY + offY] - base;
                setPoint(false, baseX + offX, baseY + offY, base + dX + dY, 0.0f, true);
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
                for (uint32_t k = 0; k <= j; ++k) {
                    float minDist = std::abs(axisPoint(yMajor, k) - origin);
                    if (yMajor ? isNew(true, k, i) : isNew(false, i, k)) {
                        if (!detectedIntersections) commitPoints.clear();
                        float majDist = interpDistance[yMajor ? k : i][yMajor ? i : k];
                        float frac = minDist / (minDist + majDist * majDist / (minDist == 0 ? 1.0f : minDist));
                        auto cur = get(yMajor, i, k);
                        setPoint(yMajor, i, k, lerpT(val, cur, frac), minDist, true);
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
                    if (yMajor ? isNew(true, k, i) : isNew(false, i, k)) {
                        if (!detectedIntersections) commitPoints.clear();
                        float majDist = interpDistance[yMajor ? k : i][yMajor ? i : k];
                        float frac = minDist / (minDist + majDist * majDist / (minDist == 0 ? 1.0f : minDist));
                        auto cur = get(yMajor, i, k);
                        setPoint(yMajor, i, k, lerpT(val, cur, frac), minDist, true);
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
                if (!valid[p.x][p.y]) {
                    valid[p.x][p.y] = true;
                    ++validCount;
                }
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
        for (auto x = 0u; x < xCount; ++x) {
            for (auto y = 0u; y < yCount; ++y) {
                isSetFlags[x][y] = true;
            }
        }
    }

    std::vector<std::vector<bool>>& getIsSet() override { return isSetFlags; }

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
        if (axis == 0) {
            if (oldindex >= values.size() || newindex > values.size()) return;
            auto v = values[oldindex];
            values.erase(values.begin() + static_cast<long>(oldindex));
            values.insert(values.begin() + static_cast<long>(newindex), v);
            auto s = isSetFlags[oldindex];
            isSetFlags.erase(isSetFlags.begin() + static_cast<long>(oldindex));
            isSetFlags.insert(isSetFlags.begin() + static_cast<long>(newindex), s);
        } else {
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (oldindex >= values[i].size() || newindex > values[i].size()) continue;
                auto v = values[i][oldindex];
                values[i].erase(values[i].begin() + static_cast<long>(oldindex));
                values[i].insert(values[i].begin() + static_cast<long>(newindex), v);
                auto s = isSetFlags[i][oldindex];
                isSetFlags[i].erase(isSetFlags[i].begin() + static_cast<long>(oldindex));
                isSetFlags[i].insert(isSetFlags[i].begin() + static_cast<long>(newindex), s);
            }
        }
        reInterpolate();
    }

    void insertKeypoints(uint32_t axis, uint32_t index) override {
        if (axis == 0) {
            auto yCount = values.empty() ? 0u : static_cast<uint32_t>(values.front().size());
            values.insert(values.begin() + static_cast<long>(std::min<std::size_t>(index, values.size())), std::vector<T>(yCount, T{}));
            isSetFlags.insert(isSetFlags.begin() + static_cast<long>(std::min<std::size_t>(index, isSetFlags.size())), std::vector<bool>(yCount, false));
        } else {
            for (std::size_t i = 0; i < values.size(); ++i) {
                values[i].insert(values[i].begin() + static_cast<long>(std::min<std::size_t>(index, values[i].size())), T{});
                isSetFlags[i].insert(isSetFlags[i].begin() + static_cast<long>(std::min<std::size_t>(index, isSetFlags[i].size())), false);
            }
        }
        reInterpolate();
    }

    void deleteKeypoints(uint32_t axis, uint32_t index) override {
        if (axis == 0) {
            if (index >= values.size()) return;
            values.erase(values.begin() + static_cast<long>(index));
            isSetFlags.erase(isSetFlags.begin() + static_cast<long>(index));
        } else {
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (index >= values[i].size()) continue;
                values[i].erase(values[i].begin() + static_cast<long>(index));
                isSetFlags[i].erase(isSetFlags[i].begin() + static_cast<long>(index));
            }
        }
        reInterpolate();
    }

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        (void)axis;
        if (index.x >= values.size() || index.y >= values[index.x].size()) return;
        values[index.x][index.y] = scaleValue(values[index.x][index.y], scale);
        isSetFlags[index.x][index.y] = true;
        reInterpolate();
    }

    void extrapolateValueAt(const Vec2u& index, int axis) override {
        if (!parameter) return;
        Vec2 offset = parameter->getKeypointOffset(index);
        switch (axis) {
        case -1: offset = Vec2{1.0f - offset.x, parameter->isVec2 ? 1.0f - offset.y : offset.y}; break;
        case 0: offset.x = 1.0f - offset.x; break;
        case 1: offset.y = 1.0f - offset.y; break;
        default: break;
        }
        Vec2u src{};
        Vec2 sub{};
        parameter->findOffset(offset, src, sub);
        auto v = interpolate(src, sub);
        setValue(index, v);
        scaleValueAt(index, axis, -1.0f);
    }

    void copyKeypointToBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other, const Vec2u& dest) override {
        if (!other) return;
        auto dst = std::dynamic_pointer_cast<ParameterBindingImpl<T>>(other);
        if (!dst) return;
        if (!isSet(src)) {
            dst->unset(dest);
        } else {
            dst->setValue(dest, getValue(src));
        }
    }

    void swapKeypointWithBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other, const Vec2u& dest) override {
        if (!other) return;
        auto dst = std::dynamic_pointer_cast<ParameterBindingImpl<T>>(other);
        if (!dst) return;
        bool thisSet = isSet(src);
        bool otherSet = dst->isSet(dest);
        auto thisVal = getValue(src);
        auto otherVal = dst->getValue(dest);
        if (src.x < values.size() && src.y < values[src.x].size()) {
            values[src.x][src.y] = otherVal;
            isSetFlags[src.x][src.y] = otherSet;
        }
        if (dest.x < dst->values.size() && dest.y < dst->values[dest.x].size()) {
            dst->values[dest.x][dest.y] = thisVal;
            dst->isSetFlags[dest.x][dest.y] = thisSet;
        }
        reInterpolate();
        dst->reInterpolate();
    }

    uint32_t getNodeUUID() const override {
        auto node = target.node.lock();
        return node ? node->uuid : 0;
    }

    InterpolateMode interpolateMode() const override { return interpolateMode_; }
    void setInterpolateMode(InterpolateMode mode) override { interpolateMode_ = mode; }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& /*other*/) const override { return true; }

protected:
    Parameter* parameter{};
    BindTarget target{};
    std::vector<std::vector<T>> values{};
    std::vector<std::vector<bool>> isSetFlags{};
    InterpolateMode interpolateMode_{InterpolateMode::Linear};

    virtual void applyToTarget(const T& value) = 0;

    virtual void clearValue(T& v) { v = T{}; }

    T& getValue(const Vec2u& point) { return values[point.x][point.y]; }

    void setValue(const Vec2u& point, const T& v) {
        if (point.x >= values.size() || point.y >= values[point.x].size()) return;
        values[point.x][point.y] = v;
        isSetFlags[point.x][point.y] = true;
        reInterpolate();
    }

    T interpolate(const Vec2u& leftKeypoint, const Vec2& offset) {
        auto lx = std::min<std::size_t>(leftKeypoint.x, values.size() > 0 ? values.size() - 1 : 0);
        auto ly = std::min<std::size_t>(leftKeypoint.y, values.empty() ? 0 : values[lx].size() > 0 ? values[lx].size() - 1 : 0);
        if (interpolateMode_ == InterpolateMode::Nearest) {
            std::size_t px = lx + (offset.x >= 0.5f && lx + 1 < values.size() ? 1 : 0);
            std::size_t py = ly + ((parameter && parameter->isVec2 && offset.y >= 0.5f && ly + 1 < values[px].size()) ? 1 : 0);
            return values[px][py];
        }
        if (interpolateMode_ == InterpolateMode::Step) {
            return values[lx][ly];
        }

        auto sample = [&](std::size_t x, std::size_t y) -> const T& {
            x = std::min<std::size_t>(x, values.size() - 1);
            y = std::min<std::size_t>(y, values[x].size() - 1);
            return values[x][y];
        };

        if (!parameter || !parameter->isVec2 || values.empty() || values[0].empty()) {
            auto p0 = sample(lx, 0);
            auto p1 = sample(std::min<std::size_t>(lx + 1, values.size() - 1), 0);
            auto t = std::clamp(offset.x, 0.0f, 1.0f);
            if (interpolateMode_ == InterpolateMode::Cubic) {
                return lerpValue(p0, p1, t);
            }
            return lerpValue(p0, p1, t);
        } else {
            const auto& p00 = sample(lx, ly);
            const auto& p01 = sample(lx, ly + 1);
            const auto& p10 = sample(lx + 1, ly);
            const auto& p11 = sample(lx + 1, ly + 1);
            auto ty = std::clamp(offset.y, 0.0f, 1.0f);
            auto tx = std::clamp(offset.x, 0.0f, 1.0f);
            auto p0 = lerpValue(p00, p01, ty);
            auto p1 = lerpValue(p10, p11, ty);
            if (interpolateMode_ == InterpolateMode::Cubic) {
                return lerpValue(p0, p1, tx);
            }
            return lerpValue(p0, p1, tx);
        }
    }
};

class ValueParameterBinding : public ParameterBindingImpl<float> {
public:
    ValueParameterBinding(Parameter* parameter, const std::shared_ptr<Node>& targetNode, const std::string& paramName)
        : ParameterBindingImpl<float>(parameter, targetNode, paramName) {}

    explicit ValueParameterBinding(Parameter* parameter)
        : ParameterBindingImpl<float>(parameter) {}

    void applyToTarget(const float& value) override {
        if (auto n = target.node.lock()) {
            n->setValue(target.name, value);
        }
    }

    void clearValue(float& v) override {
        if (auto n = target.node.lock()) {
            v = n->getDefaultValue(target.name);
        } else {
            v = 0.0f;
        }
    }

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        if (auto n = target.node.lock()) {
            auto cur = getValue(index);
            setValue(index, n->scaleValue(target.name, cur, axis, scale));
        } else {
            ParameterBindingImpl<float>::scaleValueAt(index, axis, scale);
        }
    }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const override {
        return other && other->hasParam(target.name);
    }
};

class DeformationParameterBinding : public ParameterBindingImpl<DeformSlot> {
public:
    DeformationParameterBinding(Parameter* parameter, const std::shared_ptr<Node>& targetNode, const std::string& paramName)
        : ParameterBindingImpl<DeformSlot>(parameter, targetNode, paramName) {}

    explicit DeformationParameterBinding(Parameter* parameter)
        : ParameterBindingImpl<DeformSlot>(parameter) {}

    void applyToTarget(const DeformSlot& value) override {
        if (auto n = target.node.lock()) {
            if (value.vertexOffsets.size() > 0) {
                n->setValue("transform.t.x", value.vertexOffsets.x[0]);
                n->setValue("transform.t.y", value.vertexOffsets.y[0]);
            }
        }
    }

    void clearValue(DeformSlot& v) override {
        v.vertexOffsets.clear();
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

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        if (index.x >= values.size() || index.y >= values[index.x].size()) return;
        DeformSlot cur = values[index.x][index.y];
        if (axis == -1) {
            for (std::size_t i = 0; i < cur.vertexOffsets.size(); ++i) {
                cur.vertexOffsets.x[i] *= scale;
                cur.vertexOffsets.y[i] *= scale;
            }
        } else if (axis == 0) {
            for (std::size_t i = 0; i < cur.vertexOffsets.size(); ++i) {
                cur.vertexOffsets.x[i] *= scale;
            }
        } else if (axis == 1) {
            for (std::size_t i = 0; i < cur.vertexOffsets.size(); ++i) {
                cur.vertexOffsets.y[i] *= scale;
            }
        }
        setValue(index, cur);
    }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const override {
        (void)other;
        return true;
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
                            reordered.x[newIdx] = offsets.x[oldIdx];
                            reordered.y[newIdx] = offsets.y[oldIdx];
                        }
                    }
                    offsets = reordered;
                    if (x < isSetFlags.size() && y < isSetFlags[x].size()) isSetFlags[x][y] = !offsets.empty();
                } else if (replacement.size() == newLength && newLength > 0) {
                    offsets = replacement;
                    if (x < isSetFlags.size() && y < isSetFlags[x].size()) isSetFlags[x][y] = true;
                } else {
                    offsets.resize(newLength);
                    std::fill(offsets.x.begin(), offsets.x.end(), 0.0f);
                    std::fill(offsets.y.begin(), offsets.y.end(), 0.0f);
                    if (x < isSetFlags.size() && y < isSetFlags[x].size()) isSetFlags[x][y] = false;
                }
            }
        }
        reInterpolate();
    }
};

inline std::shared_ptr<ParameterBinding> Parameter::getBinding(const std::shared_ptr<Node>& /*self*/, const std::string& key) const {
    auto it = bindingMap.find(key);
    if (it != bindingMap.end()) return it->second;
    return nullptr;
}

inline std::shared_ptr<ParameterBinding> Parameter::getOrAddBinding(const std::shared_ptr<Node>& target, const std::string& key) {
    auto it = bindingMap.find(key);
    if (it != bindingMap.end()) return it->second;
    if (key == "deform") {
        auto b = std::make_shared<DeformationParameterBinding>(this, target, key);
        bindingMap[key] = b;
        return b;
    } else {
        auto b = std::make_shared<ValueParameterBinding>(this, target, key);
        bindingMap[key] = b;
        return b;
    }
}

inline void Parameter::removeBinding(const std::shared_ptr<ParameterBinding>& binding) {
    for (auto it = bindingMap.begin(); it != bindingMap.end(); ++it) {
        if (it->second == binding) {
            bindingMap.erase(it);
            break;
        }
    }
}

inline void Parameter::makeIndexable() {
    indexableName = name;
}

inline void Parameter::update() {
    if (!active) return;
    previousInternal = latestInternal;
    switch (mergeMode) {
    case ParamMergeMode::Additive:
        latestInternal.x += value.x;
        latestInternal.y += value.y;
        break;
    case ParamMergeMode::Weighted:
        latestInternal.x = (latestInternal.x + value.x) * 0.5f;
        latestInternal.y = (latestInternal.y + value.y) * 0.5f;
        break;
    case ParamMergeMode::Multiplicative:
        latestInternal.x *= value.x;
        latestInternal.y *= value.y;
        break;
    case ParamMergeMode::Forced:
        latestInternal = value;
        break;
    case ParamMergeMode::Passthrough:
    default:
        latestInternal = value;
        break;
    }

    Vec2u left{};
    Vec2 sub{};
    findOffset(normalizedValue(), left, sub);
    for (auto& [_, b] : bindingMap) {
        if (b) b->apply(left, sub);
    }
}

class ParameterParameterBinding : public ParameterBindingImpl<float> {
public:
    ParameterParameterBinding(Parameter* parameter, const std::shared_ptr<Parameter>& targetParameter, int axis);
    explicit ParameterParameterBinding(Parameter* parameter);

    std::shared_ptr<Parameter> targetParameter() const;
    int paramAxis() const;

    void applyToTarget(const float& value) override;
    void clearValue(float& v) override;
    bool isCompatibleWithNode(const std::shared_ptr<Node>& /*other*/) const override;

private:
    std::weak_ptr<Parameter> targetParam{};
    int axisId{0};
};

inline ParameterParameterBinding::ParameterParameterBinding(Parameter* parameter, const std::shared_ptr<Parameter>& targetParameter, int axis)
    : ParameterBindingImpl<float>(parameter, nullptr, axis == 0 ? "X" : "Y"), targetParam(targetParameter), axisId(axis) {}

inline ParameterParameterBinding::ParameterParameterBinding(Parameter* parameter)
    : ParameterBindingImpl<float>(parameter) {}

inline std::shared_ptr<Parameter> ParameterParameterBinding::targetParameter() const { return targetParam.lock(); }
inline int ParameterParameterBinding::paramAxis() const { return axisId; }

inline void ParameterParameterBinding::applyToTarget(const float& value) {
    if (auto tp = targetParam.lock()) {
        if (axisId == 0) tp->value.x = value;
        else tp->value.y = value;
    }
}

inline void ParameterParameterBinding::clearValue(float& v) {
    if (auto tp = targetParam.lock()) {
        v = (axisId == 0) ? tp->defaults.x : tp->defaults.y;
    } else {
        v = 0.0f;
    }
}

inline bool ParameterParameterBinding::isCompatibleWithNode(const std::shared_ptr<Node>& /*other*/) const {
    return false;
}

} // namespace nicxlive::core::param
