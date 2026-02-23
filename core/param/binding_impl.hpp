#pragma once

#include "binding.hpp"
#include "../nodes/common.hpp"
#include "../serde.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <memory>
#include <vector>
#include <string>

namespace nicxlive::core {
class Puppet;
std::shared_ptr<nodes::Node> resolvePuppetNodeById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid);
std::shared_ptr<::nicxlive::core::param::Parameter> resolvePuppetParameterById(const std::shared_ptr<Puppet>& puppet, uint32_t uuid);
}
namespace nicxlive::core::nodes {
bool areDeformationNodesCompatible(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs);
}

namespace nicxlive::core::param {

bool pushDeformationToNode(const std::shared_ptr<Node>& node, const DeformSlot& value);
bool getDeformationNodeVertexCount(const std::shared_ptr<Node>& node, std::size_t& outVertexCount);
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
    std::shared_ptr<Node> getNode() const override { return target.target.lock(); }
    void setTarget(const std::shared_ptr<Node>& node, const std::string& paramName) override {
        target.target = node;
        target.name = paramName;
        target.uuid = node ? node->uuid : 0;
        nodeUuid_ = target.uuid;
    }
    std::string getName() const override { return target.name; }

    ::nicxlive::core::serde::SerdeException serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    void apply(const Vec2u& leftKeypoint, const Vec2& offset) override {
        applyToTarget(interpolate(leftKeypoint, offset));
    }

    void reconstruct(const std::shared_ptr<::nicxlive::core::Puppet>& /*puppet*/) override {}

    void finalize(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) override {
        if (!puppet) return;
        auto uuid = nodeUuid_ != 0 ? nodeUuid_ : target.uuid;
        if (uuid == 0) return;
        auto resolved = ::nicxlive::core::resolvePuppetNodeById(puppet, uuid);
        target.target = resolved;
        target.uuid = uuid;
        nodeUuid_ = uuid;
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
            if (isValid(yMajor, maj, min)) return;
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
                for (uint32_t k = 0; k < j; ++k) {
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
        return nodeUuid_;
    }

    InterpolateMode interpolateMode() const override { return interpolateMode_; }
    void setInterpolateMode(InterpolateMode mode) override { interpolateMode_ = mode; }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& /*other*/) const override { return true; }

protected:
    Parameter* parameter{};
    BindTarget target{};
    uint32_t nodeUuid_{0};
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
                const std::ptrdiff_t xkp = static_cast<std::ptrdiff_t>(lx);
                const std::size_t xlen = values.size() - 1;
                const auto& c0 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp - 1, 0, static_cast<std::ptrdiff_t>(xlen))), 0);
                const auto& c1 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp, 0, static_cast<std::ptrdiff_t>(xlen))), 0);
                const auto& c2 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp + 1, 0, static_cast<std::ptrdiff_t>(xlen))), 0);
                const auto& c3 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp + 2, 0, static_cast<std::ptrdiff_t>(xlen))), 0);
                return cubicValue(c0, c1, c2, c3, t);
            }
            return lerpValue(p0, p1, t);
        } else {
            const auto& p00 = sample(lx, ly);
            const auto& p01 = sample(lx, ly + 1);
            const auto& p10 = sample(lx + 1, ly);
            const auto& p11 = sample(lx + 1, ly + 1);
            auto ty = std::clamp(offset.y, 0.0f, 1.0f);
            auto tx = std::clamp(offset.x, 0.0f, 1.0f);
            if (interpolateMode_ == InterpolateMode::Cubic) {
                const std::size_t xlen = values.size() - 1;
                const std::size_t ylen = values[0].size() - 1;
                const std::ptrdiff_t xkp = static_cast<std::ptrdiff_t>(lx);
                const std::ptrdiff_t ykp = static_cast<std::ptrdiff_t>(ly);
                auto bicubicInterp = [&](float xt, float yt) {
                    std::array<T, 4> outRow{};
                    for (std::size_t y = 0; y < 4; ++y) {
                        const std::size_t yp = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(ykp + static_cast<std::ptrdiff_t>(y) - 1, 0, static_cast<std::ptrdiff_t>(ylen)));
                        const auto& c0 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp - 1, 0, static_cast<std::ptrdiff_t>(xlen))), yp);
                        const auto& c1 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp, 0, static_cast<std::ptrdiff_t>(xlen))), yp);
                        const auto& c2 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp + 1, 0, static_cast<std::ptrdiff_t>(xlen))), yp);
                        const auto& c3 = sample(static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(xkp + 2, 0, static_cast<std::ptrdiff_t>(xlen))), yp);
                        outRow[y] = cubicValue(c0, c1, c2, c3, xt);
                    }
                    return cubicValue(outRow[0], outRow[1], outRow[2], outRow[3], yt);
                };
                return bicubicInterp(tx, ty);
            }
            auto p0 = lerpValue(p00, p01, ty);
            auto p1 = lerpValue(p10, p11, ty);
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

    float valueAt(const Vec2u& idx) const {
        if (idx.x < values.size() && idx.y < values[idx.x].size()) {
            return values[idx.x][idx.y];
        }
        return 0.0f;
    }

    void update(const Vec2u& point, float value) {
        setValue(point, value);
    }

    void applyToTarget(const float& value) override {
        if (auto n = target.target.lock()) {
            n->setValue(target.name, value);
        }
    }

    void clearValue(float& v) override {
        if (auto n = target.target.lock()) {
            v = n->getDefaultValue(target.name);
        } else {
            v = 0.0f;
        }
    }

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        if (auto n = target.target.lock()) {
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
        if (target.name != "deform") return;
        float maxAbs = 0.0f;
        for (std::size_t i = 0; i < value.vertexOffsets.size(); ++i) {
            maxAbs = std::max(maxAbs, std::max(std::fabs(value.vertexOffsets.xAt(i)), std::fabs(value.vertexOffsets.yAt(i))));
        }
        auto node = target.target.lock();
        if (maxAbs > 10.0f) {
            std::fprintf(stderr,
                         "[nicxlive][DeformBind][LargeApply] param=%u:%s target=%u:%s maxAbs=%.6f first=(%.6f,%.6f)\n",
                         parameter ? parameter->uuid : 0u,
                         parameter ? parameter->name.c_str() : "<null>",
                         node ? node->uuid : 0u,
                         node ? node->name.c_str() : "<null>",
                         maxAbs,
                         value.vertexOffsets.size() ? value.vertexOffsets.xAt(0) : 0.0f,
                         value.vertexOffsets.size() ? value.vertexOffsets.yAt(0) : 0.0f);
        }
        pushDeformationToNode(node, value);
    }

    void clearValue(DeformSlot& v) override {
        std::size_t vertexCount = 0;
        if (getDeformationNodeVertexCount(target.target.lock(), vertexCount)) {
            v.vertexOffsets.resize(vertexCount);
            v.vertexOffsets.fill(Vec2{0.0f, 0.0f});
            return;
        }
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

    void update(const Vec2u& point, const Vec2Array& offsets) {
        if (point.x >= values.size() || point.y >= values[point.x].size()) return;
        values[point.x][point.y].vertexOffsets = offsets.dup();
        isSetFlags[point.x][point.y] = true;
        reInterpolate();
    }

    void scaleValueAt(const Vec2u& index, int axis, float scale) override {
        if (index.x >= values.size() || index.y >= values[index.x].size()) return;
        DeformSlot cur = values[index.x][index.y];
        if (axis == -1) {
            for (std::size_t i = 0; i < cur.vertexOffsets.size(); ++i) {
                cur.vertexOffsets.xAt(i) *= scale;
                cur.vertexOffsets.yAt(i) *= scale;
            }
        } else if (axis == 0) {
            for (std::size_t i = 0; i < cur.vertexOffsets.size(); ++i) {
                cur.vertexOffsets.xAt(i) *= scale;
            }
        } else if (axis == 1) {
            for (std::size_t i = 0; i < cur.vertexOffsets.size(); ++i) {
                cur.vertexOffsets.yAt(i) *= scale;
            }
        }
        setValue(index, cur);
    }

    bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const override {
        return ::nicxlive::core::nodes::areDeformationNodesCompatible(target.target.lock(), other);
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

inline std::shared_ptr<ParameterBinding> Parameter::getBinding(const std::shared_ptr<Node>& self, const std::string& key) const {
    const auto selfUuid = self ? self->uuid : 0u;
    for (const auto& kv : bindingMap) {
        const auto& b = kv.second;
        if (!b) continue;
        if (b->getName() != key) continue;
        auto bt = b->getTarget();
        if (selfUuid != 0 && bt.uuid != selfUuid) continue;
        return b;
    }
    return nullptr;
}

inline std::shared_ptr<ParameterBinding> Parameter::getOrAddBinding(const std::shared_ptr<Node>& target, const std::string& key) {
    const auto targetUuid = target ? target->uuid : 0u;
    for (const auto& kv : bindingMap) {
        const auto& b = kv.second;
        if (!b) continue;
        auto bt = b->getTarget();
        if (bt.uuid == targetUuid && bt.name == key) return b;
    }

    auto makeMapKey = [&](const std::string& name) {
        return std::to_string(targetUuid) + ":" + name + ":" + std::to_string(bindingMap.size());
    };
    if (key == "deform") {
        auto b = std::make_shared<DeformationParameterBinding>(this, target, key);
        bindingMap[makeMapKey(key)] = b;
        return b;
    } else {
        auto b = std::make_shared<ValueParameterBinding>(this, target, key);
        bindingMap[makeMapKey(key)] = b;
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

inline ::nicxlive::core::serde::SerdeException Parameter::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
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

        bindingMap.clear();
        if (auto bindings = data.get_child_optional("bindings")) {
            for (const auto& bindingNode : *bindings) {
                const auto& child = bindingNode.second;
                auto paramName = child.get<std::string>("param_name", "");
                if (paramName.empty()) continue;
                int paramId = child.get<int>("param_name", -1);

                std::shared_ptr<ParameterBinding> binding;
                if (paramName == "deform") {
                    binding = std::make_shared<DeformationParameterBinding>(this);
                } else if (paramName == "X" || paramName == "Y" || paramId == 0 || paramId == 1) {
                    binding = makeParameterParameterBinding(this);
                } else {
                    binding = std::make_shared<ValueParameterBinding>(this);
                }
                if (!binding) continue;
                if (auto err = binding->deserializeFromFghj(child)) return err;
                auto t = binding->getTarget();
                auto mapKey = std::to_string(t.uuid) + ":" + binding->getName() + ":" + std::to_string(bindingMap.size());
                bindingMap[mapKey] = binding;
            }
        }
    } catch (const std::exception& ex) {
        return std::string(ex.what());
    }
    return std::nullopt;
}

inline void Parameter::reconstruct(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) {
    for (auto& kv : bindingMap) {
        if (kv.second) kv.second->reconstruct(puppet);
    }
}

inline void Parameter::finalize(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) {
    makeIndexable();
    value = defaults;

    std::map<std::string, std::shared_ptr<ParameterBinding>> valid;
    for (auto& kv : bindingMap) {
        auto& b = kv.second;
        if (!b) continue;
        auto targetUuid = b->getNodeUUID();
        if (targetUuid == 0) continue;
        // D parity: keep binding when either Node or Parameter UUID resolves.
        bool validTarget = false;
        if (::nicxlive::core::resolvePuppetNodeById(puppet, targetUuid)) {
            validTarget = true;
        } else if (::nicxlive::core::resolvePuppetParameterById(puppet, targetUuid)) {
            validTarget = true;
        }
        if (validTarget) {
            b->finalize(puppet);
            valid.emplace(kv.first, b);
        }
    }
    bindingMap.swap(valid);
}

inline void Parameter::update() {
    if (!active) return;
    previousInternal = latestInternal;
    auto sum = iadd.csum();
    auto mul = imul.avg();
    latestInternal = Vec2{
        (value.x + sum.x) * mul.x,
        (value.y + sum.y) * mul.y,
    };
    if (bindingMap.empty() && traceParamBindingEnabled()) {
        std::fprintf(stderr,
                     "[nicxlive][Param] no-binding uuid=%u name=%s value=(%.6f,%.6f) latest=(%.6f,%.6f)\n",
                     uuid, name.c_str(), value.x, value.y, latestInternal.x, latestInternal.y);
    }

    auto mapInternalValue = [&](const Vec2& in) {
        const float rx = (max.x - min.x);
        const float ry = (max.y - min.y);
        float ox = (rx == 0.0f) ? 0.0f : (in.x - min.x) / rx;
        float oy = (ry == 0.0f) ? 0.0f : (in.y - min.y) / ry;
        ox = std::clamp(ox, 0.0f, 1.0f);
        oy = std::clamp(oy, 0.0f, 1.0f);
        return Vec2{ox, oy};
    };

    Vec2u left{};
    Vec2 sub{};
    auto mapped = mapInternalValue(latestInternal);
    findOffset(mapped, left, sub);
    for (auto& [_, b] : bindingMap) {
        if (!b) continue;
        b->apply(left, sub);
        if (auto node = b->getTarget().target.lock()) {
            if (valueChanged()) {
                node->notifyChange(node);
            }
        }
    }
    iadd.clear();
    imul.clear();
}

class ParameterParameterBinding : public ParameterBindingImpl<float> {
public:
    ParameterParameterBinding(Parameter* parameter, const std::shared_ptr<Parameter>& targetParameter, int axis);
    explicit ParameterParameterBinding(Parameter* parameter);

    std::shared_ptr<Parameter> targetParameter() const;
    int paramAxis() const;

    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    void finalize(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) override;
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

inline void ParameterParameterBinding::finalize(const std::shared_ptr<::nicxlive::core::Puppet>& puppet) {
    targetParam = ::nicxlive::core::resolvePuppetParameterById(puppet, nodeUuid_);
}

inline void ParameterParameterBinding::applyToTarget(const float& value) {
    if (auto tp = targetParam.lock()) {
        bool prevChanged = tp->valueChanged();
        Vec2 paramVal = tp->latestInternal;
        if (axisId == 0) {
            paramVal.x = value;
            tp->pushIOffsetAxis(axisId, paramVal.x, ParamMergeMode::Forced);
        } else {
            paramVal.y = value;
            tp->pushIOffsetAxis(axisId, paramVal.y, ParamMergeMode::Forced);
        }
        bool changed = tp->valueChanged();
        if (!prevChanged && changed) {
            tp->update();
        }
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

inline std::shared_ptr<ParameterBinding> makeParameterParameterBinding(Parameter* parameter) {
    return std::make_shared<ParameterParameterBinding>(parameter);
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
    const float t2 = t * t;
    const float t3 = t2 * t;
    const T a = p1 * 2.0f;
    const T b = (p2 - p0) * t;
    const T c = (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2;
    const T d = (p1 * 3.0f - p2 * 3.0f + p3 - p0) * t3;
    return (a + b + c + d) * 0.5f;
}

template <typename T>
::nicxlive::core::serde::SerdeException ParameterBindingImpl<T>::serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const {
    serializer.putKey("node");
    serializer.putValue(getNodeUUID());
    serializer.putKey("param_name");
    serializer.putValue(target.name);
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

    auto xCount = parameter ? parameter->axisPointCount(0) : static_cast<std::size_t>(valuesTree.size());
    auto yCount = parameter ? parameter->axisPointCount(1) : 0u;
    if (yCount == 0 && !valuesTree.empty()) yCount = valuesTree.front().second.size();

    values.assign(xCount, std::vector<T>(yCount, T{}));
    isSetFlags.assign(xCount, std::vector<bool>(yCount, false));

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

    if (parameter) {
        const auto xCount = parameter->axisPointCount(0);
        const auto yCount = parameter->axisPointCount(1);
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

} // namespace nicxlive::core::param
