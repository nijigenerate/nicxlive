#pragma once

#include "types.hpp"

#include <cassert>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

namespace nicxlive::core::math {

// SoA Vec2 array (D: veca!(float,2)) - SIMD 部分を除き写経ベースで実装
struct Vec2View;
struct Vec2ViewConst;

struct Vec2Array {
    // lanes (shared to allow views)
    std::vector<float> x{};
    std::vector<float> y{};
    float* xPtr_{nullptr};
    float* yPtr_{nullptr};
    std::size_t logicalLength_{0};
    std::size_t laneStride_{0};
    std::size_t laneBase_{0};
    std::size_t viewCapacity_{0};
    bool ownsStorage_{true};

    Vec2Array() = default;
    explicit Vec2Array(std::size_t n) { ensureLength(n); }
    Vec2Array(std::initializer_list<Vec2> init) { assign(std::vector<Vec2>(init)); }
    explicit Vec2Array(const Vec2& v) { ensureLength(1); xPtr_[0] = v.x; yPtr_[0] = v.y; }

    void refreshPointers() {
        xPtr_ = x.empty() ? nullptr : x.data();
        yPtr_ = y.empty() ? nullptr : y.data();
    }

    // length (logical)
    std::size_t size() const { return logicalLength_; }
    bool empty() const { return logicalLength_ == 0; }
    void setLength(std::size_t len) { ensureLength(len); }
    void ensureLength(std::size_t len) {
        if (logicalLength_ == len) return;
        if (!ownsStorage_) {
            assert(len <= viewCapacity_);
            logicalLength_ = len;
            return;
        }
        x.resize(len);
        y.resize(len);
        refreshPointers();
        logicalLength_ = len;
        laneStride_ = len;
        laneBase_ = 0;
        viewCapacity_ = len;
    }
    void resize(std::size_t n) { ensureLength(n); }
    void clear() {
        if (ownsStorage_) {
            x.clear();
            y.clear();
            xPtr_ = yPtr_ = nullptr;
        } else {
            xPtr_ = yPtr_ = nullptr;
        }
        logicalLength_ = 0;
        laneStride_ = 0;
        laneBase_ = 0;
        viewCapacity_ = 0;
        refreshPointers();
    }

    // append AoS
    void push_back(const Vec2& v) {
        ensureLength(logicalLength_ + 1);
        xPtr_[logicalLength_ - 1] = v.x;
        yPtr_[logicalLength_ - 1] = v.y;
    }
    void append(const Vec2& v) { push_back(v); }
    void append(const std::vector<Vec2>& values) {
        if (values.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            xPtr_[laneBase_ + old + i] = values[i].x;
            yPtr_[laneBase_ + old + i] = values[i].y;
        }
    }
    void append(const Vec2Array& rhs) {
        if (rhs.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + rhs.size());
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            xPtr_[laneBase_ + old + i] = rhs.at(i).x;
            yPtr_[laneBase_ + old + i] = rhs.at(i).y;
        }
    }

    // slice assign helpers (D: opSliceAssign)
    void sliceAssign(std::size_t start, const Vec2Array& rhs) {
        sliceAssign(start, rhs.size(), rhs);
    }
    void sliceAssign(std::size_t start, std::size_t len, const Vec2Array& rhs) {
        assert(start + len <= size());
        auto copyLen = std::min(len, rhs.size());
        for (std::size_t i = 0; i < copyLen; ++i) {
            xPtr_[laneBase_ + start + i] = rhs.at(i).x;
            yPtr_[laneBase_ + start + i] = rhs.at(i).y;
        }
    }
    void sliceAssign(std::size_t start, std::size_t len, const Vec2& v) {
        assert(start + len <= size());
        for (std::size_t i = 0; i < len; ++i) {
            xPtr_[laneBase_ + start + i] = v.x;
            yPtr_[laneBase_ + start + i] = v.y;
        }
    }

    // assign AoS
    void assign(const std::vector<Vec2>& source) {
        ensureLength(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            xPtr_[i] = source[i].x;
            yPtr_[i] = source[i].y;
        }
    }
    void operator=(const Vec2Array& rhs) { copyFrom(rhs); }
    void copyFrom(const Vec2Array& rhs) {
        ensureLength(rhs.size());
        if (rhs.size() == 0) return;
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            xPtr_[i] = rhs.at(i).x;
            yPtr_[i] = rhs.at(i).y;
        }
    }
    Vec2Array& operator+=(const Vec2Array& rhs) {
        auto n = std::min(size(), rhs.size());
        for (std::size_t i = 0; i < n; ++i) {
            xPtr_[laneBase_ + i] += rhs.at(i).x;
            yPtr_[laneBase_ + i] += rhs.at(i).y;
        }
        return *this;
    }
    Vec2Array& operator-=(const Vec2Array& rhs) {
        auto n = std::min(size(), rhs.size());
        for (std::size_t i = 0; i < n; ++i) {
            xPtr_[laneBase_ + i] -= rhs.at(i).x;
            yPtr_[laneBase_ + i] -= rhs.at(i).y;
        }
        return *this;
    }
    Vec2Array& operator*=(float s) {
        for (std::size_t i = 0; i < size(); ++i) {
            xPtr_[laneBase_ + i] *= s;
            yPtr_[laneBase_ + i] *= s;
        }
        return *this;
    }

    // element access (const view)
    Vec2 at(std::size_t i) const {
        assert(i < logicalLength_);
        return Vec2{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i]};
    }
    Vec2View operator[](std::size_t i);
    Vec2ViewConst operator[](std::size_t i) const;

    // mutable view helpers
    Vec2 get(std::size_t i) const { return at(i); }
    void set(std::size_t i, const Vec2& v) {
        assert(i < logicalLength_);
        xPtr_[laneBase_ + i] = v.x;
        yPtr_[laneBase_ + i] = v.y;
    }

    std::vector<Vec2> toArray() const {
        std::vector<Vec2> out;
        out.reserve(logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            out.push_back(Vec2{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i]});
        }
        return out;
    }
    void toArrayInto(std::vector<Vec2>& target) const { target = toArray(); }

    Vec2Array dup() const {
        Vec2Array copy;
        copy.x.assign(x.begin() + static_cast<std::ptrdiff_t>(laneBase_), x.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.y.assign(y.begin() + static_cast<std::ptrdiff_t>(laneBase_), y.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.xPtr_ = copy.x.data();
        copy.yPtr_ = copy.y.data();
        copy.logicalLength_ = logicalLength_;
        copy.laneStride_ = logicalLength_;
        copy.laneBase_ = 0;
        copy.viewCapacity_ = logicalLength_;
        return copy;
    }

    // lane view (only for owned contiguous)
    std::vector<float>& lane(std::size_t component) {
        assert(ownsStorage_ && laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0));
        if (component == 0) return x;
        assert(component == 1);
        return y;
    }
    const std::vector<float>& lane(std::size_t component) const {
        assert(ownsStorage_ && laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0));
        if (component == 0) return x;
        assert(component == 1);
        return y;
    }

    // bind external storage (view, non-owning)
    void bindExternalStorage(Vec2Array& storage, std::size_t offset, std::size_t length) {
        if (length == 0 || storage.size() == 0) {
            clear();
            return;
        }
        assert(offset + length <= storage.logicalLength_);
        ownsStorage_ = false;
        xPtr_ = storage.x.data() + offset;
        yPtr_ = storage.y.data() + offset;
        laneStride_ = storage.logicalLength_;
        laneBase_ = 0;
        logicalLength_ = length;
        viewCapacity_ = length;
    }

    // front/back views
    Vec2View front();
    Vec2View back();
    Vec2ViewConst front() const;
    Vec2ViewConst back() const;

    // foreach (apply) helpers
    int forEach(const std::function<int(Vec2View)>& dg);
    int forEach(const std::function<int(std::size_t, Vec2View)>& dg);
    int forEach(const std::function<int(Vec2ViewConst)>& dg) const;
    int forEach(const std::function<int(std::size_t, Vec2ViewConst)>& dg) const;

private:
    friend struct Vec2View;
    friend struct Vec2ViewConst;
};

struct Vec2View {
    Vec2Array* owner{nullptr};
    std::size_t index{0};
    float& x;
    float& y;

    Vec2View(Vec2Array& arr, std::size_t idx)
        : owner(&arr)
        , index(idx)
        , x(owner->xPtr_[owner->laneBase_ + idx])
        , y(owner->yPtr_[owner->laneBase_ + idx]) {
        assert(owner && idx < owner->logicalLength_);
    }

    Vec2 toVector() const { return Vec2{x, y}; }
    operator Vec2() const { return toVector(); }
    void set(const Vec2& v) {
        x = v.x;
        y = v.y;
    }
};

struct Vec2ViewConst {
    std::size_t index{0};
    float x{0};
    float y{0};

    Vec2ViewConst(const Vec2Array& arr, std::size_t idx) : index(idx) {
        assert(idx < arr.logicalLength_);
        x = arr.xPtr_[arr.laneBase_ + idx];
        y = arr.yPtr_[arr.laneBase_ + idx];
    }

    Vec2 toVector() const { return Vec2{x, y}; }
    operator Vec2() const { return toVector(); }
};

inline Vec2View Vec2Array::operator[](std::size_t i) {
    assert(i < logicalLength_);
    return Vec2View{*this, i};
}
inline Vec2ViewConst Vec2Array::operator[](std::size_t i) const {
    assert(i < logicalLength_);
    return Vec2ViewConst{*this, i};
}
inline Vec2View Vec2Array::front() { return Vec2View{*this, 0}; }
inline Vec2View Vec2Array::back() { return Vec2View{*this, logicalLength_ - 1}; }
inline Vec2ViewConst Vec2Array::front() const { return Vec2ViewConst{*this, 0}; }
inline Vec2ViewConst Vec2Array::back() const { return Vec2ViewConst{*this, logicalLength_ - 1}; }

inline int Vec2Array::forEach(const std::function<int(Vec2View)>& dg) {
    for (std::size_t i = 0; i < logicalLength_; ++i) {
        Vec2View v{*this, i};
        int res = dg(v);
        if (res) return res;
    }
    return 0;
}
inline int Vec2Array::forEach(const std::function<int(std::size_t, Vec2View)>& dg) {
    for (std::size_t i = 0; i < logicalLength_; ++i) {
        Vec2View v{*this, i};
        int res = dg(i, v);
        if (res) return res;
    }
    return 0;
}
inline int Vec2Array::forEach(const std::function<int(Vec2ViewConst)>& dg) const {
    for (std::size_t i = 0; i < logicalLength_; ++i) {
        Vec2ViewConst v{*this, i};
        int res = dg(v);
        if (res) return res;
    }
    return 0;
}
inline int Vec2Array::forEach(const std::function<int(std::size_t, Vec2ViewConst)>& dg) const {
    for (std::size_t i = 0; i < logicalLength_; ++i) {
        Vec2ViewConst v{*this, i};
        int res = dg(i, v);
        if (res) return res;
    }
    return 0;
}

// ---- Vec3Array (SoA) ----
struct Vec3View;
struct Vec3ViewConst;

struct Vec3Array {
    std::vector<float> x{};
    std::vector<float> y{};
    std::vector<float> z{};
    float* xPtr_{nullptr};
    float* yPtr_{nullptr};
    float* zPtr_{nullptr};
    std::size_t logicalLength_{0};
    std::size_t laneStride_{0};
    std::size_t laneBase_{0};
    std::size_t viewCapacity_{0};
    bool ownsStorage_{true};

    Vec3Array() = default;
    explicit Vec3Array(std::size_t n) { ensureLength(n); }
    Vec3Array(std::initializer_list<Vec3> init) { assign(std::vector<Vec3>(init)); }

    void refreshPointers() {
        xPtr_ = x.empty() ? nullptr : x.data();
        yPtr_ = y.empty() ? nullptr : y.data();
        zPtr_ = z.empty() ? nullptr : z.data();
    }

    std::size_t size() const { return logicalLength_; }
    bool empty() const { return logicalLength_ == 0; }
    void setLength(std::size_t len) { ensureLength(len); }
    void ensureLength(std::size_t len) {
        if (logicalLength_ == len) return;
        if (!ownsStorage_) {
            assert(len <= viewCapacity_);
            logicalLength_ = len;
            return;
        }
        x.resize(len);
        y.resize(len);
        z.resize(len);
        refreshPointers();
        logicalLength_ = len;
        laneStride_ = len;
        laneBase_ = 0;
        viewCapacity_ = len;
    }
    void resize(std::size_t n) { ensureLength(n); }
    void clear() {
        if (ownsStorage_) {
            x.clear();
            y.clear();
            z.clear();
            xPtr_ = yPtr_ = zPtr_ = nullptr;
        } else {
            xPtr_ = yPtr_ = zPtr_ = nullptr;
        }
        logicalLength_ = 0;
        laneStride_ = 0;
        laneBase_ = 0;
        viewCapacity_ = 0;
        refreshPointers();
    }

    void push_back(const Vec3& v) {
        ensureLength(logicalLength_ + 1);
        auto idx = logicalLength_ - 1;
        xPtr_[idx] = v.x;
        yPtr_[idx] = v.y;
        zPtr_[idx] = v.z;
    }
    void append(const Vec3& v) { push_back(v); }
    void append(const std::vector<Vec3>& values) {
        if (values.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            xPtr_[laneBase_ + old + i] = values[i].x;
            yPtr_[laneBase_ + old + i] = values[i].y;
            zPtr_[laneBase_ + old + i] = values[i].z;
        }
    }
    void append(const Vec3Array& rhs) {
        if (rhs.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + rhs.size());
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            auto v = rhs.at(i);
            xPtr_[laneBase_ + old + i] = v.x;
            yPtr_[laneBase_ + old + i] = v.y;
            zPtr_[laneBase_ + old + i] = v.z;
        }
    }

    void sliceAssign(std::size_t start, const Vec3Array& rhs) {
        sliceAssign(start, rhs.size(), rhs);
    }
    void sliceAssign(std::size_t start, std::size_t len, const Vec3Array& rhs) {
        assert(start + len <= size());
        auto copyLen = std::min(len, rhs.size());
        for (std::size_t i = 0; i < copyLen; ++i) {
            auto v = rhs.at(i);
            xPtr_[laneBase_ + start + i] = v.x;
            yPtr_[laneBase_ + start + i] = v.y;
            zPtr_[laneBase_ + start + i] = v.z;
        }
    }
    void sliceAssign(std::size_t start, std::size_t len, const Vec3& v) {
        assert(start + len <= size());
        for (std::size_t i = 0; i < len; ++i) {
            xPtr_[laneBase_ + start + i] = v.x;
            yPtr_[laneBase_ + start + i] = v.y;
            zPtr_[laneBase_ + start + i] = v.z;
        }
    }

    void assign(const std::vector<Vec3>& source) {
        ensureLength(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            xPtr_[i] = source[i].x;
            yPtr_[i] = source[i].y;
            zPtr_[i] = source[i].z;
        }
    }
    void operator=(const Vec3Array& rhs) { copyFrom(rhs); }
    void copyFrom(const Vec3Array& rhs) {
        ensureLength(rhs.size());
        if (rhs.size() == 0) return;
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            auto v = rhs.at(i);
            xPtr_[i] = v.x;
            yPtr_[i] = v.y;
            zPtr_[i] = v.z;
        }
    }

    Vec3 at(std::size_t i) const {
        assert(i < logicalLength_);
        return Vec3{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i], zPtr_[laneBase_ + i]};
    }
    Vec3View operator[](std::size_t i);
    Vec3ViewConst operator[](std::size_t i) const;

    void set(std::size_t i, const Vec3& v) {
        assert(i < logicalLength_);
        xPtr_[laneBase_ + i] = v.x;
        yPtr_[laneBase_ + i] = v.y;
        zPtr_[laneBase_ + i] = v.z;
    }

    std::vector<Vec3> toArray() const {
        std::vector<Vec3> out;
        out.reserve(logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            out.push_back(Vec3{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i], zPtr_[laneBase_ + i]});
        }
        return out;
    }

    Vec3Array dup() const {
        Vec3Array copy;
        copy.x.assign(x.begin() + static_cast<std::ptrdiff_t>(laneBase_), x.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.y.assign(y.begin() + static_cast<std::ptrdiff_t>(laneBase_), y.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.z.assign(z.begin() + static_cast<std::ptrdiff_t>(laneBase_), z.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.refreshPointers();
        copy.logicalLength_ = logicalLength_;
        copy.laneStride_ = logicalLength_;
        copy.laneBase_ = 0;
        copy.viewCapacity_ = logicalLength_;
        return copy;
    }

    void bindExternalStorage(Vec3Array& storage, std::size_t offset, std::size_t length) {
        if (length == 0 || storage.size() == 0) {
            clear();
            return;
        }
        assert(offset + length <= storage.logicalLength_);
        ownsStorage_ = false;
        xPtr_ = storage.x.data() + offset;
        yPtr_ = storage.y.data() + offset;
        zPtr_ = storage.z.data() + offset;
        laneStride_ = storage.logicalLength_;
        laneBase_ = 0;
        logicalLength_ = length;
        viewCapacity_ = length;
    }

    Vec3View front();
    Vec3View back();
    Vec3ViewConst front() const;
    Vec3ViewConst back() const;
};

struct Vec3View {
    Vec3Array* owner{nullptr};
    std::size_t index{0};
    float& x;
    float& y;
    float& z;

    Vec3View(Vec3Array& arr, std::size_t idx)
        : owner(&arr)
        , index(idx)
        , x(owner->xPtr_[owner->laneBase_ + idx])
        , y(owner->yPtr_[owner->laneBase_ + idx])
        , z(owner->zPtr_[owner->laneBase_ + idx]) {
        assert(owner && idx < owner->logicalLength_);
    }

    Vec3 toVector() const { return Vec3{x, y, z}; }
    operator Vec3() const { return toVector(); }
    void set(const Vec3& v) {
        x = v.x;
        y = v.y;
        z = v.z;
    }
};

struct Vec3ViewConst {
    std::size_t index{0};
    float x{0};
    float y{0};
    float z{0};

    Vec3ViewConst(const Vec3Array& arr, std::size_t idx) : index(idx) {
        assert(idx < arr.logicalLength_);
        x = arr.xPtr_[arr.laneBase_ + idx];
        y = arr.yPtr_[arr.laneBase_ + idx];
        z = arr.zPtr_[arr.laneBase_ + idx];
    }

    Vec3 toVector() const { return Vec3{x, y, z}; }
    operator Vec3() const { return toVector(); }
};

inline Vec3View Vec3Array::operator[](std::size_t i) {
    assert(i < logicalLength_);
    return Vec3View{*this, i};
}
inline Vec3ViewConst Vec3Array::operator[](std::size_t i) const {
    assert(i < logicalLength_);
    return Vec3ViewConst{*this, i};
}
inline Vec3View Vec3Array::front() { return Vec3View{*this, 0}; }
inline Vec3View Vec3Array::back() { return Vec3View{*this, logicalLength_ - 1}; }
inline Vec3ViewConst Vec3Array::front() const { return Vec3ViewConst{*this, 0}; }
inline Vec3ViewConst Vec3Array::back() const { return Vec3ViewConst{*this, logicalLength_ - 1}; }

// ---- Vec4Array (SoA) ----
struct Vec4View;
struct Vec4ViewConst;

struct Vec4Array {
    std::vector<float> x{};
    std::vector<float> y{};
    std::vector<float> z{};
    std::vector<float> w{};
    float* xPtr_{nullptr};
    float* yPtr_{nullptr};
    float* zPtr_{nullptr};
    float* wPtr_{nullptr};
    std::size_t logicalLength_{0};
    std::size_t laneStride_{0};
    std::size_t laneBase_{0};
    std::size_t viewCapacity_{0};
    bool ownsStorage_{true};

    Vec4Array() = default;
    explicit Vec4Array(std::size_t n) { ensureLength(n); }
    Vec4Array(std::initializer_list<Vec4> init) { assign(std::vector<Vec4>(init)); }

    void refreshPointers() {
        xPtr_ = x.empty() ? nullptr : x.data();
        yPtr_ = y.empty() ? nullptr : y.data();
        zPtr_ = z.empty() ? nullptr : z.data();
        wPtr_ = w.empty() ? nullptr : w.data();
    }

    std::size_t size() const { return logicalLength_; }
    bool empty() const { return logicalLength_ == 0; }
    void setLength(std::size_t len) { ensureLength(len); }
    void ensureLength(std::size_t len) {
        if (logicalLength_ == len) return;
        if (!ownsStorage_) {
            assert(len <= viewCapacity_);
            logicalLength_ = len;
            return;
        }
        x.resize(len);
        y.resize(len);
        z.resize(len);
        w.resize(len);
        refreshPointers();
        logicalLength_ = len;
        laneStride_ = len;
        laneBase_ = 0;
        viewCapacity_ = len;
    }
    void resize(std::size_t n) { ensureLength(n); }
    void clear() {
        if (ownsStorage_) {
            x.clear();
            y.clear();
            z.clear();
            w.clear();
            xPtr_ = yPtr_ = zPtr_ = wPtr_ = nullptr;
        } else {
            xPtr_ = yPtr_ = zPtr_ = wPtr_ = nullptr;
        }
        logicalLength_ = 0;
        laneStride_ = 0;
        laneBase_ = 0;
        viewCapacity_ = 0;
        refreshPointers();
    }

    void push_back(const Vec4& v) {
        ensureLength(logicalLength_ + 1);
        auto idx = logicalLength_ - 1;
        xPtr_[idx] = v.x;
        yPtr_[idx] = v.y;
        zPtr_[idx] = v.z;
        wPtr_[idx] = v.w;
    }
    void append(const Vec4& v) { push_back(v); }
    void append(const std::vector<Vec4>& values) {
        if (values.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            xPtr_[laneBase_ + old + i] = values[i].x;
            yPtr_[laneBase_ + old + i] = values[i].y;
            zPtr_[laneBase_ + old + i] = values[i].z;
            wPtr_[laneBase_ + old + i] = values[i].w;
        }
    }
    void append(const Vec4Array& rhs) {
        if (rhs.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + rhs.size());
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            auto v = rhs.at(i);
            xPtr_[laneBase_ + old + i] = v.x;
            yPtr_[laneBase_ + old + i] = v.y;
            zPtr_[laneBase_ + old + i] = v.z;
            wPtr_[laneBase_ + old + i] = v.w;
        }
    }

    void sliceAssign(std::size_t start, const Vec4Array& rhs) { sliceAssign(start, rhs.size(), rhs); }
    void sliceAssign(std::size_t start, std::size_t len, const Vec4Array& rhs) {
        assert(start + len <= size());
        auto copyLen = std::min(len, rhs.size());
        for (std::size_t i = 0; i < copyLen; ++i) {
            auto v = rhs.at(i);
            xPtr_[laneBase_ + start + i] = v.x;
            yPtr_[laneBase_ + start + i] = v.y;
            zPtr_[laneBase_ + start + i] = v.z;
            wPtr_[laneBase_ + start + i] = v.w;
        }
    }
    void sliceAssign(std::size_t start, std::size_t len, const Vec4& v) {
        assert(start + len <= size());
        for (std::size_t i = 0; i < len; ++i) {
            xPtr_[laneBase_ + start + i] = v.x;
            yPtr_[laneBase_ + start + i] = v.y;
            zPtr_[laneBase_ + start + i] = v.z;
            wPtr_[laneBase_ + start + i] = v.w;
        }
    }

    void assign(const std::vector<Vec4>& source) {
        ensureLength(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            xPtr_[i] = source[i].x;
            yPtr_[i] = source[i].y;
            zPtr_[i] = source[i].z;
            wPtr_[i] = source[i].w;
        }
    }
    void operator=(const Vec4Array& rhs) { copyFrom(rhs); }
    void copyFrom(const Vec4Array& rhs) {
        ensureLength(rhs.size());
        if (rhs.size() == 0) return;
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            auto v = rhs.at(i);
            xPtr_[i] = v.x;
            yPtr_[i] = v.y;
            zPtr_[i] = v.z;
            wPtr_[i] = v.w;
        }
    }

    Vec4 at(std::size_t i) const {
        assert(i < logicalLength_);
        return Vec4{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i], zPtr_[laneBase_ + i], wPtr_[laneBase_ + i]};
    }
    Vec4View operator[](std::size_t i);
    Vec4ViewConst operator[](std::size_t i) const;

    void set(std::size_t i, const Vec4& v) {
        assert(i < logicalLength_);
        xPtr_[laneBase_ + i] = v.x;
        yPtr_[laneBase_ + i] = v.y;
        zPtr_[laneBase_ + i] = v.z;
        wPtr_[laneBase_ + i] = v.w;
    }

    std::vector<Vec4> toArray() const {
        std::vector<Vec4> out;
        out.reserve(logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            out.push_back(Vec4{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i], zPtr_[laneBase_ + i], wPtr_[laneBase_ + i]});
        }
        return out;
    }

    Vec4Array dup() const {
        Vec4Array copy;
        copy.x.assign(x.begin() + static_cast<std::ptrdiff_t>(laneBase_), x.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.y.assign(y.begin() + static_cast<std::ptrdiff_t>(laneBase_), y.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.z.assign(z.begin() + static_cast<std::ptrdiff_t>(laneBase_), z.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.w.assign(w.begin() + static_cast<std::ptrdiff_t>(laneBase_), w.begin() + static_cast<std::ptrdiff_t>(laneBase_ + logicalLength_));
        copy.refreshPointers();
        copy.logicalLength_ = logicalLength_;
        copy.laneStride_ = logicalLength_;
        copy.laneBase_ = 0;
        copy.viewCapacity_ = logicalLength_;
        return copy;
    }

    void bindExternalStorage(Vec4Array& storage, std::size_t offset, std::size_t length) {
        if (length == 0 || storage.size() == 0) {
            clear();
            return;
        }
        assert(offset + length <= storage.logicalLength_);
        ownsStorage_ = false;
        xPtr_ = storage.x.data() + offset;
        yPtr_ = storage.y.data() + offset;
        zPtr_ = storage.z.data() + offset;
        wPtr_ = storage.w.data() + offset;
        laneStride_ = storage.logicalLength_;
        laneBase_ = 0;
        logicalLength_ = length;
        viewCapacity_ = length;
    }

    Vec4View front();
    Vec4View back();
    Vec4ViewConst front() const;
    Vec4ViewConst back() const;
};

struct Vec4View {
    Vec4Array* owner{nullptr};
    std::size_t index{0};
    float& x;
    float& y;
    float& z;
    float& w;

    Vec4View(Vec4Array& arr, std::size_t idx)
        : owner(&arr)
        , index(idx)
        , x(owner->xPtr_[owner->laneBase_ + idx])
        , y(owner->yPtr_[owner->laneBase_ + idx])
        , z(owner->zPtr_[owner->laneBase_ + idx])
        , w(owner->wPtr_[owner->laneBase_ + idx]) {
        assert(owner && idx < owner->logicalLength_);
    }

    Vec4 toVector() const { return Vec4{x, y, z, w}; }
    operator Vec4() const { return toVector(); }
    void set(const Vec4& v) {
        x = v.x;
        y = v.y;
        z = v.z;
        w = v.w;
    }
};

struct Vec4ViewConst {
    std::size_t index{0};
    float x{0};
    float y{0};
    float z{0};
    float w{0};

    Vec4ViewConst(const Vec4Array& arr, std::size_t idx) : index(idx) {
        assert(idx < arr.logicalLength_);
        x = arr.xPtr_[arr.laneBase_ + idx];
        y = arr.yPtr_[arr.laneBase_ + idx];
        z = arr.zPtr_[arr.laneBase_ + idx];
        w = arr.wPtr_[arr.laneBase_ + idx];
    }

    Vec4 toVector() const { return Vec4{x, y, z, w}; }
    operator Vec4() const { return toVector(); }
};

inline Vec4View Vec4Array::operator[](std::size_t i) {
    assert(i < logicalLength_);
    return Vec4View{*this, i};
}
inline Vec4ViewConst Vec4Array::operator[](std::size_t i) const {
    assert(i < logicalLength_);
    return Vec4ViewConst{*this, i};
}
inline Vec4View Vec4Array::front() { return Vec4View{*this, 0}; }
inline Vec4View Vec4Array::back() { return Vec4View{*this, logicalLength_ - 1}; }
inline Vec4ViewConst Vec4Array::front() const { return Vec4ViewConst{*this, 0}; }
inline Vec4ViewConst Vec4Array::back() const { return Vec4ViewConst{*this, logicalLength_ - 1}; }

} // namespace nicxlive::core::math
