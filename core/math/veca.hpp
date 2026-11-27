#pragma once

#include "types.hpp"
#include "../common/shared_array.hpp"
#include "../compat/assert_error.hpp"
#include "../compat/array_range_error.hpp"
#include "../compat/native_access_fault.hpp"
#include "../compat/system_error.hpp"
#include <cstring>

#include <cassert>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

namespace nicxlive::core::math {
namespace detail {
[[noreturn]] inline void ngFailVecSliceCopy(bool overlap, std::size_t first, std::size_t second) {
    d_runtime::RetainedString message;
    try {
        std::string text = overlap ? "Overlapping arrays in copy: " : "Array lengths don't match for copy: ";
        text += std::to_string(first);
        text += overlap ? " byte(s) overlap of " : " != ";
        text += std::to_string(second);
        message = d_runtime::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    }
#ifdef NDEBUG
    std::abort();
#else
    throw d_runtime::ngStaticError<d_runtime::AssertError>(std::move(message),
        d_runtime::RetainedString::fromStaticLiteral("core/internal/util/array.d"), overlap ? 68 : 50);
#endif
}

// Checked LDC POD slice assignment is distinct from the source opAssign memmove.
inline void ngCopyVecLaneSource(common::SharedArray<float> destination,
    const common::SharedArray<float> source) {
    if (source.size() != destination.size())
        ngFailVecSliceCopy(false, source.size(), destination.size());
    const auto from = reinterpret_cast<std::uintptr_t>(source.data());
    const auto to = reinterpret_cast<std::uintptr_t>(destination.data());
    const auto distance = from > to ? from - to : to - from;
    const auto bytes = source.size() * sizeof(float);
    if (distance < bytes) ngFailVecSliceCopy(true, bytes - distance, bytes);
    if (bytes) std::memcpy(destination.data(), source.data(), bytes);
}

inline void ngFillVecLane(common::SharedArray<float> lane, float value) {
    auto* destination = lane.data();
    for (std::size_t i = 0; i < lane.size(); ++i) destination[i] = value;
}

template<class Array, class Vector> inline void ngFillVecArray(Array& left, Vector right) {
    ngFillVecLane(left.x, right.x);
    ngFillVecLane(left.y, right.y);
    if constexpr (requires { left.z; }) ngFillVecLane(left.z, right.z);
    if constexpr (requires { left.w; }) ngFillVecLane(left.w, right.w);
}

inline void ngRequireVecArithmeticLength(bool matches, bool arrayExpression = false) {
#ifndef NDEBUG
    if (!matches) {
        if (arrayExpression)
            throw d_runtime::ngStaticError<d_runtime::AssertError>(
                d_runtime::RetainedString::fromStaticLiteral("Mismatched array lengths for vector operation"),
                d_runtime::RetainedString::fromStaticLiteral("core/internal/array/operations.d"), 45);
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Mismatched vector lengths"),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 181);
    }
#else
    (void)matches;
    (void)arrayExpression;
#endif
}

template<char Op> inline float ngVecArithmetic(float left, float right) {
    static_assert(Op == '+' || Op == '-' || Op == '*' || Op == '/');
    if constexpr (Op == '+') return left + right;
    else if constexpr (Op == '-') return left - right;
    else if constexpr (Op == '*') return left * right;
    else return left / right;
}

template<char Op>
inline void ngApplyVecLane(common::SharedArray<float> left, const common::SharedArray<float> right) {
    ngRequireVecArithmeticLength(left.size() == right.size(), true);
    auto* destination = left.data();
    const auto* source = right.data();
    std::size_t i = 0;
    if (((reinterpret_cast<std::uintptr_t>(destination) |
            reinterpret_cast<std::uintptr_t>(source)) & 15) == 0) {
        for (; i + 4 <= left.size(); i += 4) {
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
            const auto a = _mm_load_ps(destination + i);
            const auto b = _mm_load_ps(source + i);
            __m128 result;
            if constexpr (Op == '+') result = _mm_add_ps(a, b);
            else if constexpr (Op == '-') result = _mm_sub_ps(a, b);
            else if constexpr (Op == '*') result = _mm_mul_ps(a, b);
            else result = _mm_div_ps(a, b);
            _mm_store_ps(destination + i, result);
#else
            // Capture the entire Source vector before writing any of its cells.
            std::array<float, 4> a, b, result;
            std::memcpy(a.data(), destination + i, sizeof(a));
            std::memcpy(b.data(), source + i, sizeof(b));
            for (std::size_t j = 0; j < 4; ++j) result[j] = ngVecArithmetic<Op>(a[j], b[j]);
            std::memcpy(destination + i, result.data(), sizeof(result));
#endif
        }
    }
    // The selected LDC array-expression runtime uses a scalar fallback.
    for (; i < left.size(); ++i) destination[i] = ngVecArithmetic<Op>(destination[i], source[i]);
}

template<char Op> inline void ngApplyVecLaneScalar(common::SharedArray<float> left, float scalar) {
    auto* destination = left.data();
    for (std::size_t i = 0; i < left.size(); ++i)
        destination[i] = ngVecArithmetic<Op>(destination[i], scalar);
}

template<char Op, class Array> inline void ngApplyVecArray(Array& left, Array right) {
    ngRequireVecArithmeticLength(left.size() == right.size());
    ngApplyVecLane<Op>(left.x, right.x);
    ngApplyVecLane<Op>(left.y, right.y);
    if constexpr (requires { left.z; }) ngApplyVecLane<Op>(left.z, right.z);
    if constexpr (requires { left.w; }) ngApplyVecLane<Op>(left.w, right.w);
}

template<char Op, class Array, class Vector> inline void ngApplyVecConstant(Array& left, Vector right) {
    ngApplyVecLaneScalar<Op>(left.x, right.x);
    ngApplyVecLaneScalar<Op>(left.y, right.y);
    if constexpr (requires { left.z; }) ngApplyVecLaneScalar<Op>(left.z, right.z);
    if constexpr (requires { left.w; }) ngApplyVecLaneScalar<Op>(left.w, right.w);
}

template<char Op, class Array> inline void ngApplyVecScalar(Array& left, float scalar) {
    ngApplyVecLaneScalar<Op>(left.x, scalar);
    ngApplyVecLaneScalar<Op>(left.y, scalar);
    if constexpr (requires { left.z; }) ngApplyVecLaneScalar<Op>(left.z, scalar);
    if constexpr (requires { left.w; }) ngApplyVecLaneScalar<Op>(left.w, scalar);
}

inline common::SharedArray<float> ngAllocateVecBacking(std::size_t count,
    std::source_location source = std::source_location::current()) {
    constexpr std::size_t alignment = 16;
    constexpr auto nativeAlignment = std::align_val_t{alignment};
    try {
        // Source snap reserves padding and deliberately leaves float cells uninitialized.
        auto* storage = ::operator new(count * sizeof(float) + alignment - 1, nativeAlignment);
        std::shared_ptr<void> owner(storage, [](void* value) noexcept {
            ::operator delete(value, std::align_val_t{alignment});
        });
        return common::SharedArray<float>::retainedView(std::move(owner),
            static_cast<float*>(storage), count);
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(source);
    }
}

// Only passive vector values are constructed inside the allocation boundary.
template<class V>
inline std::vector<V> ngAllocateVecArray(std::size_t count,
    std::source_location source = std::source_location::current()) {
    static_assert(std::is_same_v<V, Vec2> || std::is_same_v<V, Vec3> || std::is_same_v<V, Vec4>);
    try {
        return std::vector<V>(count);
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(source);
    }
}

template<class V>
inline void ngResizeVecArray(std::vector<V>& values, std::size_t count,
    std::source_location source = std::source_location::current()) {
    static_assert(std::is_same_v<V, Vec2> || std::is_same_v<V, Vec3> || std::is_same_v<V, Vec4>);
    try {
        values.resize(count);
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(source);
    }
}

template<class V>
inline common::SharedArray<V> ngDuplicateVecArraySource(common::SharedArray<V> values,
    std::source_location source = std::source_location::current()) {
    static_assert(std::is_same_v<V, Vec2> || std::is_same_v<V, Vec3> || std::is_same_v<V, Vec4>);
    try {
        return values.duplicate();
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(source);
    }
}

inline void ngAssertVec2SourceIndex(bool condition, std::size_t line) {
#ifndef NDEBUG
    if (!condition) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        d_runtime::RetainedString::fromStaticLiteral("veca index out of range"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), line);
#else
    (void)condition;
    (void)line;
#endif
}
} // namespace detail


// SoA Vec2 array (D: veca!(float,2)) - SIMD 部分を除き写経ベースで実装
struct Vec2View;
struct Vec2ViewConst;
struct RawStorage {
    const float* ptr{nullptr};
    std::size_t length{0};
    std::shared_ptr<void> owner{};
};

struct Vec2Array {
    static constexpr std::size_t kSimdAlignment = 16;
    // lanes (shared to allow views)
    common::SharedArray<float> x{};
    common::SharedArray<float> y{};
    float* xPtr_{nullptr};
    float* yPtr_{nullptr};
    std::size_t logicalLength_{0};
    std::size_t laneStride_{0};
    std::size_t laneBase_{0};
    std::size_t viewCapacity_{0};
    bool ownsStorage_{true};
    std::size_t alignment_{kSimdAlignment};
    common::SharedArray<float> sourceBacking_{};
    mutable std::vector<float> backing_{};

    Vec2Array() = default;
    // Copy the real lane descriptors, not the native upload scratch cache.
    Vec2Array(const Vec2Array& rhs)
        : x(rhs.x), y(rhs.y), xPtr_(rhs.xPtr_), yPtr_(rhs.yPtr_),
          logicalLength_(rhs.logicalLength_), laneStride_(rhs.laneStride_),
          laneBase_(rhs.laneBase_), viewCapacity_(rhs.viewCapacity_),
          ownsStorage_(rhs.ownsStorage_), alignment_(rhs.alignment_), sourceBacking_(rhs.sourceBacking_) {}
    Vec2Array& operator=(const Vec2Array& rhs) {
        if (this == &rhs) return *this;
        copyFrom(rhs);
        return *this;
    }
    Vec2Array(Vec2Array&& rhs) : Vec2Array(static_cast<const Vec2Array&>(rhs)) {}
    Vec2Array& operator=(Vec2Array&& rhs) {
        if (this != &rhs) copyFrom(rhs);
        return *this;
    }
    explicit Vec2Array(std::size_t n) { ensureLength(n); }
    explicit Vec2Array(common::SharedArray<Vec2> values) { assign(values); }
    Vec2Array(std::initializer_list<Vec2> init) { assign(std::vector<Vec2>(init)); }
    explicit Vec2Array(const std::vector<Vec2>& values) { assign(values); }
    explicit Vec2Array(const Vec2& v) {
        const auto value = v;
        ensureLength(1);
        set(0, value);
    }

    void refreshPointers() {
        if (!ownsStorage_) return;
        xPtr_ = x.empty() ? nullptr : x.data();
        yPtr_ = y.empty() ? nullptr : y.data();
    }

    void rebindSourceLanes() {
        if (!logicalLength_ || sourceBacking_.empty()) {
            x = {};
            y = {};
            xPtr_ = yPtr_ = nullptr;
            return;
        }
        const auto stride = laneStride_ ? laneStride_ : logicalLength_;
        x = sourceBacking_.slice(laneBase_, laneBase_ + logicalLength_);
        xPtr_ = sourceBacking_.data();
        y = sourceBacking_.slice(stride + laneBase_, stride + laneBase_ + logicalLength_);
        yPtr_ = sourceBacking_.data() + stride;
    }

    // length (logical)
    std::size_t size() const { return logicalLength_; }
    bool empty() const { return logicalLength_ == 0; }
    void setLength(std::size_t len) { ensureLength(len); }
    void ensureLength(std::size_t len) {
        if (logicalLength_ == len) return;
        if (!ownsStorage_) {
#ifndef NDEBUG
            if(len > viewCapacity_)
                throw d_runtime::AssertError("veca view length exceeds capacity", "nijilive/math/veca.d", 125);
#endif
            logicalLength_ = len;
            rebindSourceLanes();
            return;
        }
        // Independent D veca reallocates on every owned size change.
        const auto copyLength = std::min(len, logicalLength_);
        auto nextBacking = len ? detail::ngAllocateVecBacking(len * 2) : sourceBacking_;
        if (!len) nextBacking.resize(0);
        auto nextX = len ? nextBacking.slice(0, len) : common::SharedArray<float>{};
        auto nextY = len ? nextBacking.slice(len, len * 2) : common::SharedArray<float>{};
        if (copyLength) std::copy_n(xPtr_ + laneBase_, copyLength, nextX.data());
        if (copyLength) std::copy_n(yPtr_ + laneBase_, copyLength, nextY.data());
        x = nextX;
        y = nextY;
        sourceBacking_ = nextBacking;
        refreshPointers();
        logicalLength_ = len;
        laneStride_ = len;
        laneBase_ = 0;
        viewCapacity_ = len;
    }
    void resize(std::size_t n) { ensureLength(n); }
    void clear() {
        logicalLength_ = 0;
        if (ownsStorage_) {
            x.resize(0);
            y.resize(0);
            sourceBacking_.resize(0);
        } else {
            x.clear();
            y.clear();
            sourceBacking_.clear();
        }
        xPtr_ = nullptr;
        yPtr_ = nullptr;
        laneStride_ = laneBase_ = viewCapacity_ = 0;
    }

    // append AoS
    void push_back(const Vec2& v) {
        const auto value = v;
        ensureLength(logicalLength_ + 1);
        set(logicalLength_ - 1, value);
    }
    void append(const Vec2& v) { push_back(v); }
    void append(const common::SharedArray<Vec2> values) {
        const auto oldLength = logicalLength_;
        ensureLength(oldLength + values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto value = values[i];
            set(oldLength + i, value);
        }
    }
    void append(std::initializer_list<Vec2> values) { append(std::vector<Vec2>(values)); }
    void append(const std::vector<Vec2>& values) {
        if (values.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            xPtr_[laneBase_ + old + i] = values[i].x;
            yPtr_[laneBase_ + old + i] = values[i].y;
        }
    }
    void append(Vec2Array rhs) {
        if (rhs.empty()) return;
        auto old = logicalLength_;
        ensureLength(old + rhs.size());
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            xPtr_[laneBase_ + old + i] = rhs.at(i).x;
            yPtr_[laneBase_ + old + i] = rhs.at(i).y;
        }
    }

    // slice assign helpers (D: opSliceAssign)
    void sliceAssign(Vec2Array rhs) {
        ensureLength(rhs.size());
        detail::ngCopyVecLaneSource(x, rhs.x);
        detail::ngCopyVecLaneSource(y, rhs.y);
    }
    void sliceAssign(common::SharedArray<Vec2> values) { assign(values); }
    void sliceAssign(Vec2 value) { detail::ngFillVecArray(*this, value); }

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
            xPtr_[laneBase_ + i] = source[i].x;
            yPtr_[laneBase_ + i] = source[i].y;
        }
    }
    void assign(std::initializer_list<Vec2> values) { assign(std::vector<Vec2>(values)); }
    void assign(const common::SharedArray<Vec2> source) {
        ensureLength(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            const auto value = source[i];
            set(i, value);
        }
    }
    void copyFrom(const Vec2Array& rhs) {
        // Copy the descriptor before resizing so overlapping owners remain alive.
        const Vec2Array source(rhs);
        ensureLength(source.size());
        if (source.empty()) return;
        std::memmove(xPtr_ + laneBase_, source.xPtr_ + source.laneBase_, source.size() * sizeof(float));
        std::memmove(yPtr_ + laneBase_, source.yPtr_ + source.laneBase_, source.size() * sizeof(float));
    }

    Vec2Array& operator+=(Vec2Array rhs) {
        detail::ngApplyVecArray<'+'>(*this, rhs);
        return *this;
    }
    Vec2Array& operator-=(Vec2Array rhs) {
        detail::ngApplyVecArray<'-'>(*this, rhs);
        return *this;
    }
    Vec2Array& operator*=(Vec2Array rhs) {
        detail::ngApplyVecArray<'*'>(*this, rhs);
        return *this;
    }
    Vec2Array& operator/=(Vec2Array rhs) {
        detail::ngApplyVecArray<'/'>(*this, rhs);
        return *this;
    }
    Vec2Array& operator+=(Vec2 rhs) {
        detail::ngApplyVecConstant<'+'>(*this, rhs);
        return *this;
    }
    Vec2Array& operator-=(Vec2 rhs) {
        detail::ngApplyVecConstant<'-'>(*this, rhs);
        return *this;
    }
    Vec2Array& operator*=(Vec2 rhs) {
        detail::ngApplyVecConstant<'*'>(*this, rhs);
        return *this;
    }
    Vec2Array& operator/=(Vec2 rhs) {
        detail::ngApplyVecConstant<'/'>(*this, rhs);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec2Array& operator+=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'+'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec2Array& operator-=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'-'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec2Array& operator*=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'*'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec2Array& operator/=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'/'>(*this, scalar);
        return *this;
    }

    // element access (const view)
    // Source binary ~ duplicates this operand before appending the captured RHS.
    Vec2Array concat(Vec2Array rhs) const {
        auto result = dup();
        result.append(rhs);
        return result;
    }
    Vec2Array concat(Vec2 value) const {
        auto result = dup();
        result.append(value);
        return result;
    }
    // Source right-hand ~ appends the prefix before reading this descriptor.
    Vec2Array prepend(Vec2 value) const {
        Vec2Array result;
        result.append(value);
        result.append(*this);
        return result;
    }

    Vec2 at(std::size_t i) const {
        detail::ngAssertVec2SourceIndex(i < logicalLength_, 144);
        return Vec2{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i]};
    }
    Vec2View operator[](std::size_t i);
    Vec2ViewConst operator[](std::size_t i) const;

    // mutable view helpers
    Vec2 get(std::size_t i) const { return at(i); }
    void set(std::size_t i, const Vec2& v) {
        detail::ngAssertVec2SourceIndex(i < logicalLength_, 139);
        xPtr_[laneBase_ + i] = v.x;
        yPtr_[laneBase_ + i] = v.y;
    }

    const float* dataX() const { return xPtr_ ? xPtr_ + laneBase_ : nullptr; }
    const float* dataY() const { return yPtr_ ? yPtr_ + laneBase_ : nullptr; }
    float* dataXMutable() { return xPtr_ ? xPtr_ + laneBase_ : nullptr; }
    float* dataYMutable() { return yPtr_ ? yPtr_ + laneBase_ : nullptr; }
    float& xAt(std::size_t i) {
        assert(i < logicalLength_);
        return xPtr_[laneBase_ + i];
    }
    float& yAt(std::size_t i) {
        assert(i < logicalLength_);
        return yPtr_[laneBase_ + i];
    }
    float xAt(std::size_t i) const {
        assert(i < logicalLength_);
        return xPtr_[laneBase_ + i];
    }
    float yAt(std::size_t i) const {
        assert(i < logicalLength_);
        return yPtr_[laneBase_ + i];
    }
    void fill(const Vec2& v) {
        const auto value = v;
        detail::ngFillVecArray(*this, value);
    }
    std::size_t length() const { return logicalLength_; }

    std::vector<Vec2> toArray() const {
        auto out = detail::ngAllocateVecArray<Vec2>(logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            out[i] = Vec2{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i]};
        }
        return out;
    }
    void toArrayInto(std::vector<Vec2>& target) const {
        detail::ngResizeVecArray(target, logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            target[i] = Vec2{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i]};
        }
    }

    // Iterate the actual source lanes with a retained descriptor snapshot.
    // No AoS mirror, copied position buffer or write-back cache is introduced.
    struct ConstIterator {
        using iterator_category = std::input_iterator_tag;
        using value_type = Vec2;
        using difference_type = std::ptrdiff_t;
        using reference = Vec2;
        using pointer = void;
        common::SharedArray<float> xOwner, yOwner;
        const float* xData{};
        const float* yData{};
        std::size_t base{}, position{};
        Vec2 operator*() const { return {xData[base+position], yData[base+position]}; }
        ConstIterator& operator++() { ++position; return *this; }
        ConstIterator operator++(int) { auto previous=*this; ++*this; return previous; }
        bool operator==(const ConstIterator& other) const {
            return xData==other.xData && yData==other.yData && base==other.base && position==other.position;
        }
        bool operator!=(const ConstIterator& other) const { return !(*this==other); }
    };
    ConstIterator begin() const { return {x, y, xPtr_, yPtr_, laneBase_, 0}; }
    ConstIterator end() const { return {x, y, xPtr_, yPtr_, laneBase_, logicalLength_}; }
    Vec2Array duplicate() const { return dup(); }


    Vec2Array dup() const {
        Vec2Array copy;
        copy.ensureLength(logicalLength_);
        const float* srcX = dataX();
        const float* srcY = dataY();
        if (logicalLength_ != 0 && srcX && srcY) {
            std::copy_n(srcX, logicalLength_, copy.xPtr_);
            std::copy_n(srcY, logicalLength_, copy.yPtr_);
        }
        copy.logicalLength_ = logicalLength_;
        copy.laneStride_ = logicalLength_;
        copy.laneBase_ = 0;
        copy.viewCapacity_ = logicalLength_;
        return copy;
    }

    RawStorage rawStorage() const {
#ifndef NDEBUG
        if (!(laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0)))
            throw d_runtime::ngStaticError<d_runtime::AssertError>(
                d_runtime::RetainedString::fromStaticLiteral(
                    "rawStorage is only available for owned contiguous buffers"),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 325);
#endif
        return {sourceBacking_.data(), sourceBacking_.size(), sourceBacking_.retentionOwner()};
    }

    std::size_t alignment() const { return alignment_; }
    const std::vector<float>& backing() const {
        // This native-only snapshot is separate from Source rawStorage.
        auto n = size();
        backing_.resize(n * 2);
        if (n == 0 || !xPtr_ || !yPtr_) {
            return backing_;
        }
        for (std::size_t i = 0; i < n; ++i) {
            backing_[i] = xPtr_[laneBase_ + i];
            backing_[n + i] = yPtr_[laneBase_ + i];
        }
        return backing_;
    }

    // lane view (only for owned contiguous)
    common::SharedArray<float>& lane(std::size_t component) {
        assert(ownsStorage_ && laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0));
        if (component == 0) return x;
        assert(component == 1);
        return y;
    }
    const common::SharedArray<float>& lane(std::size_t component) const {
        assert(ownsStorage_ && laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0));
        if (component == 0) return x;
        assert(component == 1);
        return y;
    }
    std::array<common::SharedArray<float>*, 2> lanes() {
        assert(ownsStorage_ && laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0));
        return {&x, &y};
    }
    std::array<const common::SharedArray<float>*, 2> lanes() const {
        assert(ownsStorage_ && laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0));
        return {&x, &y};
    }

    // bind external storage (view, non-owning)
    void bindExternalStorage(Vec2Array& storage, std::size_t offset, std::size_t length) {
        const Vec2Array source(storage);
        if (!length || source.sourceBacking_.empty()) {
            ownsStorage_ = false;
            sourceBacking_ = {};
            logicalLength_ = 0;
            viewCapacity_ = 0;
            laneStride_ = storage.logicalLength_;
            laneBase_ = offset;
            rebindSourceLanes();
            return;
        }
        ownsStorage_ = false;
        sourceBacking_ = source.sourceBacking_;
        laneStride_ = storage.logicalLength_;
        laneBase_ = offset;
        logicalLength_ = length;
        viewCapacity_ = length;
        rebindSourceLanes();
    }

    // front/back views
    Vec2View front();
    Vec2View back();
    Vec2ViewConst front() const;
    Vec2ViewConst back() const;

    // Source lowering for an opApply argument immediately converted to Vec2.
    // The loop bound is captured once, but each value reads the live lanes.
    // This does not replace the general mutable vecv/ref-property protocol.
    template<class Callback> int forEachValue(Callback&& dg) {
        const auto initialLength = logicalLength_;
        for (std::size_t index = 0; index < initialLength; ++index) {
            const auto xLength = ownsStorage_ ? x.size() : logicalLength_;
            if (index >= xLength) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(index, xLength,
                d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 467);
            const float valueX = (ownsStorage_ ? x.data() : xPtr_)[laneBase_ + index];
            const auto yLength = ownsStorage_ ? y.size() : logicalLength_;
            if (index >= yLength) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(index, yLength,
                d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 467);
            const float valueY = (ownsStorage_ ? y.data() : yPtr_)[laneBase_ + index];
            const int result = std::invoke(dg, index, Vec2{valueX, valueY});
            if (result) return result;
        }
        return 0;
    }

    // foreach (apply) helpers
    int forEach(const std::function<int(Vec2View)>& dg);
    int forEach(const std::function<int(std::size_t, Vec2View)>& dg);
    int forEach(const std::function<int(Vec2ViewConst)>& dg) const;
    int forEach(const std::function<int(std::size_t, Vec2ViewConst)>& dg) const;

private:
    friend struct Vec2View;
    friend struct Vec2ViewConst;
};

namespace detail {
// Source vecv views borrow the lane-descriptor object, not an old scalar copy.
inline const float& readVec2ViewLane(const Vec2Array* owner, std::size_t index,
        std::size_t lane, std::size_t sourceLine) {
    const auto array = d_runtime::ngRequireObject(owner);
    if (lane >= 2) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(lane, 2,
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), sourceLine);
    const auto& descriptor = lane == 0 ? array->x : array->y;
    const auto length = descriptor.size();
    if (index >= length) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(index, length,
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), sourceLine);
    // Current descriptors already include the backing lane and view offsets.
    const auto data = descriptor.data();
    return d_runtime::ngRequireObject(data)[index];
}
inline float& writeVec2ViewLane(Vec2Array* owner, std::size_t index,
        std::size_t lane, std::size_t sourceLine) {
    return const_cast<float&>(readVec2ViewLane(owner, index, lane, sourceLine));
}
inline void assertVec2ViewBinding(const void* owner, bool readOnly) {
#ifndef NDEBUG
    if (!owner) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        readOnly ? d_runtime::RetainedString::fromStaticLiteral("vecvConst is not bound to storage")
                 : d_runtime::RetainedString::fromStaticLiteral("vecv is not bound to storage"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), readOnly ? 534 : 460);
#else
    (void)owner;
    (void)readOnly;
#endif
}
}

struct Vec2View {
    Vec2Array* owner{nullptr};
    std::size_t index{0};
    // Native property cells read/write the current genuine lane descriptor.
    struct Component {
        Vec2Array* owner{nullptr};
        std::size_t index{0}, lane{0};
        float& reference() const {
            detail::assertVec2ViewBinding(owner, false);
            return detail::writeVec2ViewLane(owner, index, lane, 461);
        }
        operator float&() const { return reference(); }
        float& operator=(float value) const { return reference() = value; }
        float& operator=(const Component& value) const {
            auto& destination = reference();
            const float scalar = value.reference();
            return destination = scalar;
        }
        float* operator&() const { return std::addressof(reference()); }
    };
    Component x{}, y{};

    Vec2View() = default;
    Vec2View(Vec2Array& arr, std::size_t idx)
        : owner(&arr), index(idx), x{&arr, idx, 0}, y{&arr, idx, 1} {}
    Vec2View(Vec2Array& arr, std::size_t idx, Vec2 initial) : Vec2View(arr, idx) { set(initial); }
    Vec2View(const Vec2View&) = default;
    float& component(std::size_t lane) {
        detail::assertVec2ViewBinding(owner, false);
        return detail::writeVec2ViewLane(owner, index, lane, 461);
    }
    Vec2 toVector() const {
        const float valueX = detail::readVec2ViewLane(owner, index, 0, 467);
        const float valueY = detail::readVec2ViewLane(owner, index, 1, 467);
        return Vec2{valueX, valueY};
    }
    operator Vec2() const { return toVector(); }
    void set(const Vec2& value) {
        const auto captured = value;
        detail::writeVec2ViewLane(owner, index, 0, 475) = captured.x;
        detail::writeVec2ViewLane(owner, index, 1, 475) = captured.y;
    }
    Vec2View& operator=(const Vec2& value) { set(value); return *this; }
    Vec2View& operator=(const Vec2View& value) {
        Vec2View source(value);
        auto& destinationX = detail::writeVec2ViewLane(owner, index, 0, 480);
        destinationX = source.component(0);
        auto& destinationY = detail::writeVec2ViewLane(owner, index, 1, 480);
        destinationY = source.component(1);
        return *this;
    }
};

struct Vec2ViewConst {
    const Vec2Array* owner{nullptr};
    std::size_t index{0};
    struct Component {
        const Vec2Array* owner{nullptr};
        std::size_t index{0}, lane{0};
        operator float() const {
            detail::assertVec2ViewBinding(owner, true);
            return detail::readVec2ViewLane(owner, index, lane, 535);
        }
    };
    Component x{}, y{};
    Vec2ViewConst() = default;
    Vec2ViewConst(const Vec2Array& arr, std::size_t idx)
        : owner(&arr), index(idx), x{&arr, idx, 0}, y{&arr, idx, 1} {}
    float component(std::size_t lane) const {
        detail::assertVec2ViewBinding(owner, true);
        return detail::readVec2ViewLane(owner, index, lane, 535);
    }
    Vec2 toVector() const {
        const float valueX = detail::readVec2ViewLane(owner, index, 0, 541);
        const float valueY = detail::readVec2ViewLane(owner, index, 1, 541);
        return Vec2{valueX, valueY};
    }
    operator Vec2() const { return toVector(); }
};

inline Vec2View Vec2Array::operator[](std::size_t i) {
    detail::ngAssertVec2SourceIndex(i < logicalLength_, 139);
    return Vec2View{*this, i};
}
inline Vec2ViewConst Vec2Array::operator[](std::size_t i) const {
    detail::ngAssertVec2SourceIndex(i < logicalLength_, 144);
    return Vec2ViewConst{*this, i};
}
inline Vec2View Vec2Array::front() { return (*this)[0]; }
inline Vec2View Vec2Array::back() { return (*this)[logicalLength_ - 1]; }
inline Vec2ViewConst Vec2Array::front() const { return (*this)[0]; }
inline Vec2ViewConst Vec2Array::back() const { return (*this)[logicalLength_ - 1]; }

inline int Vec2Array::forEach(const std::function<int(Vec2View)>& dg) {
    const auto initialLength = logicalLength_;
    for (std::size_t i = 0; i < initialLength; ++i) {
        Vec2View v{*this, i};
        if (!dg) d_runtime::ngFailNullAccess(false);
        int res = dg(v);
        if (res) return res;
    }
    return 0;
}
inline int Vec2Array::forEach(const std::function<int(std::size_t, Vec2View)>& dg) {
    const auto initialLength = logicalLength_;
    for (std::size_t i = 0; i < initialLength; ++i) {
        Vec2View v{*this, i};
        if (!dg) d_runtime::ngFailNullAccess(false);
        int res = dg(i, v);
        if (res) return res;
    }
    return 0;
}
inline int Vec2Array::forEach(const std::function<int(Vec2ViewConst)>& dg) const {
    const auto initialLength = logicalLength_;
    for (std::size_t i = 0; i < initialLength; ++i) {
        Vec2ViewConst v{*this, i};
        if (!dg) d_runtime::ngFailNullAccess(false);
        int res = dg(v);
        if (res) return res;
    }
    return 0;
}
inline int Vec2Array::forEach(const std::function<int(std::size_t, Vec2ViewConst)>& dg) const {
    const auto initialLength = logicalLength_;
    for (std::size_t i = 0; i < initialLength; ++i) {
        Vec2ViewConst v{*this, i};
        if (!dg) d_runtime::ngFailNullAccess(false);
        int res = dg(i, v);
        if (res) return res;
    }
    return 0;
}

// ---- Vec3Array (SoA) ----
struct Vec3View;
struct Vec3ViewConst;

struct Vec3Array {
    common::SharedArray<float> x{};
    common::SharedArray<float> y{};
    common::SharedArray<float> z{};
    float* xPtr_{nullptr};
    float* yPtr_{nullptr};
    float* zPtr_{nullptr};
    std::size_t logicalLength_{0};
    std::size_t laneStride_{0};
    std::size_t laneBase_{0};
    std::size_t viewCapacity_{0};
    bool ownsStorage_{true};
    common::SharedArray<float> sourceBacking_{};
    mutable std::vector<float> rawCache_{};

    Vec3Array() = default;
    // Source value construction copies lane descriptors, not native scratch.
    Vec3Array(const Vec3Array& rhs)
        : x(rhs.x), y(rhs.y), z(rhs.z), xPtr_(rhs.xPtr_), yPtr_(rhs.yPtr_), zPtr_(rhs.zPtr_),
          logicalLength_(rhs.logicalLength_), laneStride_(rhs.laneStride_),
          laneBase_(rhs.laneBase_), viewCapacity_(rhs.viewCapacity_), ownsStorage_(rhs.ownsStorage_),
          sourceBacking_(rhs.sourceBacking_) {}
    Vec3Array& operator=(const Vec3Array& rhs) {
        if (this == &rhs) return *this;
        copyFrom(rhs);
        return *this;
    }
    Vec3Array(Vec3Array&& rhs) : Vec3Array(static_cast<const Vec3Array&>(rhs)) {}
    Vec3Array& operator=(Vec3Array&& rhs) {
        if (this != &rhs) copyFrom(rhs);
        return *this;
    }
    explicit Vec3Array(std::size_t n) { ensureLength(n); }
    explicit Vec3Array(Vec3 value) {
        ensureLength(1);
        set(0, value);
    }
    explicit Vec3Array(common::SharedArray<Vec3> values) { assign(values); }
    Vec3Array(std::initializer_list<Vec3> init) { assign(std::vector<Vec3>(init)); }

    void refreshPointers() {
        if (!ownsStorage_) return;
        xPtr_ = x.empty() ? nullptr : x.data();
        yPtr_ = y.empty() ? nullptr : y.data();
        zPtr_ = z.empty() ? nullptr : z.data();
    }

    void rebindSourceLanes() {
        if (!logicalLength_ || sourceBacking_.empty()) {
            x = {};
            y = {};
            z = {};
            xPtr_ = yPtr_ = zPtr_ = nullptr;
            return;
        }
        const auto stride = laneStride_ ? laneStride_ : logicalLength_;
        x = sourceBacking_.slice(laneBase_, laneBase_ + logicalLength_);
        xPtr_ = sourceBacking_.data();
        y = sourceBacking_.slice(stride + laneBase_, stride + laneBase_ + logicalLength_);
        yPtr_ = sourceBacking_.data() + stride;
        z = sourceBacking_.slice(stride * 2 + laneBase_, stride * 2 + laneBase_ + logicalLength_);
        zPtr_ = sourceBacking_.data() + stride * 2;
    }

    std::size_t size() const { return logicalLength_; }
    bool empty() const { return logicalLength_ == 0; }
    void setLength(std::size_t len) { ensureLength(len); }
    void ensureLength(std::size_t len) {
        if (logicalLength_ == len) return;
        if (!ownsStorage_) {
            assert(len <= viewCapacity_);
            logicalLength_ = len;
            rebindSourceLanes();
            return;
        }
        // Independent D veca reallocates on every owned size change.
        const auto copyLength = std::min(len, logicalLength_);
        auto nextBacking = len ? detail::ngAllocateVecBacking(len * 3) : sourceBacking_;
        if (!len) nextBacking.resize(0);
        auto nextX = len ? nextBacking.slice(0, len) : common::SharedArray<float>{};
        auto nextY = len ? nextBacking.slice(len, len * 2) : common::SharedArray<float>{};
        auto nextZ = len ? nextBacking.slice(len * 2, len * 3) : common::SharedArray<float>{};
        if (copyLength) std::copy_n(xPtr_ + laneBase_, copyLength, nextX.data());
        if (copyLength) std::copy_n(yPtr_ + laneBase_, copyLength, nextY.data());
        if (copyLength) std::copy_n(zPtr_ + laneBase_, copyLength, nextZ.data());
        x = nextX;
        y = nextY;
        z = nextZ;
        sourceBacking_ = nextBacking;
        refreshPointers();
        logicalLength_ = len;
        laneStride_ = len;
        laneBase_ = 0;
        viewCapacity_ = len;
    }
    void resize(std::size_t n) { ensureLength(n); }
    void clear() {
        logicalLength_ = 0;
        if (ownsStorage_) {
            x.resize(0);
            y.resize(0);
            z.resize(0);
            sourceBacking_.resize(0);
        } else {
            x.clear();
            y.clear();
            z.clear();
            sourceBacking_.clear();
        }
        xPtr_ = nullptr;
        yPtr_ = nullptr;
        zPtr_ = nullptr;
        laneStride_ = laneBase_ = viewCapacity_ = 0;
    }

    void push_back(const Vec3& v) {
        const auto value = v;
        ensureLength(logicalLength_ + 1);
        set(logicalLength_ - 1, value);
    }
    void append(const Vec3& v) { push_back(v); }
    void append(const common::SharedArray<Vec3> values) {
        const auto oldLength = logicalLength_;
        ensureLength(oldLength + values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto value = values[i];
            set(oldLength + i, value);
        }
    }
    void append(std::initializer_list<Vec3> values) { append(std::vector<Vec3>(values)); }
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
    void append(Vec3Array rhs) {
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

    void sliceAssign(Vec3Array rhs) {
        ensureLength(rhs.size());
        detail::ngCopyVecLaneSource(x, rhs.x);
        detail::ngCopyVecLaneSource(y, rhs.y);
        detail::ngCopyVecLaneSource(z, rhs.z);
    }
    void sliceAssign(common::SharedArray<Vec3> values) { assign(values); }
    void sliceAssign(Vec3 value) { detail::ngFillVecArray(*this, value); }
    void fill(const Vec3& value) { detail::ngFillVecArray(*this, Vec3(value)); }

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
            xPtr_[laneBase_ + i] = source[i].x;
            yPtr_[laneBase_ + i] = source[i].y;
            zPtr_[laneBase_ + i] = source[i].z;
        }
    }
    void assign(std::initializer_list<Vec3> values) { assign(std::vector<Vec3>(values)); }
    void assign(const common::SharedArray<Vec3> source) {
        ensureLength(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            const auto value = source[i];
            set(i, value);
        }
    }
    void copyFrom(const Vec3Array& rhs) {
        // Copy the descriptor before resizing so overlapping owners remain alive.
        const Vec3Array source(rhs);
        ensureLength(source.size());
        if (source.empty()) return;
        std::memmove(xPtr_ + laneBase_, source.xPtr_ + source.laneBase_, source.size() * sizeof(float));
        std::memmove(yPtr_ + laneBase_, source.yPtr_ + source.laneBase_, source.size() * sizeof(float));
        std::memmove(zPtr_ + laneBase_, source.zPtr_ + source.laneBase_, source.size() * sizeof(float));
    }

    Vec3Array& operator+=(Vec3Array rhs) {
        detail::ngApplyVecArray<'+'>(*this, rhs);
        return *this;
    }
    Vec3Array& operator-=(Vec3Array rhs) {
        detail::ngApplyVecArray<'-'>(*this, rhs);
        return *this;
    }
    Vec3Array& operator*=(Vec3Array rhs) {
        detail::ngApplyVecArray<'*'>(*this, rhs);
        return *this;
    }
    Vec3Array& operator/=(Vec3Array rhs) {
        detail::ngApplyVecArray<'/'>(*this, rhs);
        return *this;
    }
    Vec3Array& operator+=(Vec3 rhs) {
        detail::ngApplyVecConstant<'+'>(*this, rhs);
        return *this;
    }
    Vec3Array& operator-=(Vec3 rhs) {
        detail::ngApplyVecConstant<'-'>(*this, rhs);
        return *this;
    }
    Vec3Array& operator*=(Vec3 rhs) {
        detail::ngApplyVecConstant<'*'>(*this, rhs);
        return *this;
    }
    Vec3Array& operator/=(Vec3 rhs) {
        detail::ngApplyVecConstant<'/'>(*this, rhs);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec3Array& operator+=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'+'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec3Array& operator-=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'-'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec3Array& operator*=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'*'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec3Array& operator/=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'/'>(*this, scalar);
        return *this;
    }

    // Source binary ~ duplicates this operand before appending the captured RHS.
    Vec3Array concat(Vec3Array rhs) const {
        auto result = dup();
        result.append(rhs);
        return result;
    }
    Vec3Array concat(Vec3 value) const {
        auto result = dup();
        result.append(value);
        return result;
    }
    // Source right-hand ~ appends the prefix before reading this descriptor.
    Vec3Array prepend(Vec3 value) const {
        Vec3Array result;
        result.append(value);
        result.append(*this);
        return result;
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
        auto out = detail::ngAllocateVecArray<Vec3>(logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            out[i] = Vec3{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i], zPtr_[laneBase_ + i]};
        }
        return out;
    }

    void toArrayInto(std::vector<Vec3>& target) const {
        detail::ngResizeVecArray(target, logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            target[i] = Vec3{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i], zPtr_[laneBase_ + i]};
        }
    }

    Vec3Array dup() const {
        Vec3Array copy;
        copy.ensureLength(logicalLength_);
        if (logicalLength_ != 0 && xPtr_ && yPtr_ && zPtr_) {
            std::copy_n(xPtr_ + laneBase_, logicalLength_, copy.xPtr_);
            std::copy_n(yPtr_ + laneBase_, logicalLength_, copy.yPtr_);
            std::copy_n(zPtr_ + laneBase_, logicalLength_, copy.zPtr_);
        }
        copy.logicalLength_ = logicalLength_;
        copy.laneStride_ = logicalLength_;
        copy.laneBase_ = 0;
        copy.viewCapacity_ = logicalLength_;
        return copy;
    }

    void bindExternalStorage(Vec3Array& storage, std::size_t offset, std::size_t length) {
        const Vec3Array source(storage);
        if (!length || source.sourceBacking_.empty()) {
            ownsStorage_ = false;
            sourceBacking_ = {};
            logicalLength_ = 0;
            viewCapacity_ = 0;
            laneStride_ = storage.logicalLength_;
            laneBase_ = offset;
            rebindSourceLanes();
            return;
        }
        ownsStorage_ = false;
        sourceBacking_ = source.sourceBacking_;
        laneStride_ = storage.logicalLength_;
        laneBase_ = offset;
        logicalLength_ = length;
        viewCapacity_ = length;
        rebindSourceLanes();
    }

    RawStorage rawStorage() const {
#ifndef NDEBUG
        if (!(laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0)))
            throw d_runtime::ngStaticError<d_runtime::AssertError>(
                d_runtime::RetainedString::fromStaticLiteral(
                    "rawStorage is only available for owned contiguous buffers"),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 325);
#endif
        return {sourceBacking_.data(), sourceBacking_.size(), sourceBacking_.retentionOwner()};
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
    common::SharedArray<float> x{};
    common::SharedArray<float> y{};
    common::SharedArray<float> z{};
    common::SharedArray<float> w{};
    float* xPtr_{nullptr};
    float* yPtr_{nullptr};
    float* zPtr_{nullptr};
    float* wPtr_{nullptr};
    std::size_t logicalLength_{0};
    std::size_t laneStride_{0};
    std::size_t laneBase_{0};
    std::size_t viewCapacity_{0};
    bool ownsStorage_{true};
    common::SharedArray<float> sourceBacking_{};

    Vec4Array() = default;
    Vec4Array(const Vec4Array&) = default;
    Vec4Array& operator=(const Vec4Array& rhs) {
        if (this == &rhs) return *this;
        copyFrom(rhs);
        return *this;
    }
    Vec4Array(Vec4Array&& rhs) : Vec4Array(static_cast<const Vec4Array&>(rhs)) {}
    Vec4Array& operator=(Vec4Array&& rhs) {
        if (this != &rhs) copyFrom(rhs);
        return *this;
    }
    explicit Vec4Array(std::size_t n) { ensureLength(n); }
    explicit Vec4Array(Vec4 value) {
        ensureLength(1);
        set(0, value);
    }
    explicit Vec4Array(common::SharedArray<Vec4> values) { assign(values); }
    Vec4Array(std::initializer_list<Vec4> init) { assign(std::vector<Vec4>(init)); }

    void refreshPointers() {
        if (!ownsStorage_) return;
        xPtr_ = x.empty() ? nullptr : x.data();
        yPtr_ = y.empty() ? nullptr : y.data();
        zPtr_ = z.empty() ? nullptr : z.data();
        wPtr_ = w.empty() ? nullptr : w.data();
    }

    void rebindSourceLanes() {
        if (!logicalLength_ || sourceBacking_.empty()) {
            x = {};
            y = {};
            z = {};
            w = {};
            xPtr_ = yPtr_ = zPtr_ = wPtr_ = nullptr;
            return;
        }
        const auto stride = laneStride_ ? laneStride_ : logicalLength_;
        x = sourceBacking_.slice(laneBase_, laneBase_ + logicalLength_);
        y = sourceBacking_.slice(stride + laneBase_, stride + laneBase_ + logicalLength_);
        z = sourceBacking_.slice(stride * 2 + laneBase_, stride * 2 + laneBase_ + logicalLength_);
        w = sourceBacking_.slice(stride * 3 + laneBase_, stride * 3 + laneBase_ + logicalLength_);
        xPtr_ = sourceBacking_.data();
        yPtr_ = sourceBacking_.data() + stride;
        zPtr_ = sourceBacking_.data() + stride * 2;
        wPtr_ = sourceBacking_.data() + stride * 3;
    }

    std::size_t size() const { return logicalLength_; }
    bool empty() const { return logicalLength_ == 0; }
    void setLength(std::size_t len) { ensureLength(len); }
    void ensureLength(std::size_t len) {
        if (logicalLength_ == len) return;
        if (!ownsStorage_) {
            assert(len <= viewCapacity_);
            logicalLength_ = len;
            rebindSourceLanes();
            return;
        }
        const auto copyLength = std::min(len, logicalLength_);
        auto nextBacking = len ? detail::ngAllocateVecBacking(len * 4) : sourceBacking_;
        if (!len) nextBacking.resize(0);
        auto nextX = len ? nextBacking.slice(0, len) : common::SharedArray<float>{};
        auto nextY = len ? nextBacking.slice(len, len * 2) : common::SharedArray<float>{};
        auto nextZ = len ? nextBacking.slice(len * 2, len * 3) : common::SharedArray<float>{};
        auto nextW = len ? nextBacking.slice(len * 3, len * 4) : common::SharedArray<float>{};
        if (copyLength) std::copy_n(xPtr_ + laneBase_, copyLength, nextX.data());
        if (copyLength) std::copy_n(yPtr_ + laneBase_, copyLength, nextY.data());
        if (copyLength) std::copy_n(zPtr_ + laneBase_, copyLength, nextZ.data());
        if (copyLength) std::copy_n(wPtr_ + laneBase_, copyLength, nextW.data());
        x = nextX;
        y = nextY;
        z = nextZ;
        w = nextW;
        sourceBacking_ = nextBacking;
        refreshPointers();
        logicalLength_ = len;
        laneStride_ = len;
        laneBase_ = 0;
        viewCapacity_ = len;
    }
    void resize(std::size_t n) { ensureLength(n); }
    void clear() {
        logicalLength_ = 0;
        if (ownsStorage_) {
            x.resize(0);
            y.resize(0);
            z.resize(0);
            w.resize(0);
            sourceBacking_.resize(0);
        } else {
            x.clear();
            y.clear();
            z.clear();
            w.clear();
            sourceBacking_.clear();
        }
        xPtr_ = yPtr_ = zPtr_ = wPtr_ = nullptr;
        laneStride_ = 0;
        laneBase_ = 0;
        viewCapacity_ = 0;
    }

    void push_back(const Vec4& v) {
        const auto value = v;
        ensureLength(logicalLength_ + 1);
        set(logicalLength_ - 1, value);
    }
    void append(const Vec4& v) { push_back(v); }
    void append(const common::SharedArray<Vec4> values) {
        const auto oldLength = logicalLength_;
        ensureLength(oldLength + values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto value = values[i];
            set(oldLength + i, value);
        }
    }
    void append(std::initializer_list<Vec4> values) { append(std::vector<Vec4>(values)); }
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
    void append(Vec4Array rhs) {
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

    void sliceAssign(Vec4Array rhs) {
        ensureLength(rhs.size());
        detail::ngCopyVecLaneSource(x, rhs.x);
        detail::ngCopyVecLaneSource(y, rhs.y);
        detail::ngCopyVecLaneSource(z, rhs.z);
        detail::ngCopyVecLaneSource(w, rhs.w);
    }
    void sliceAssign(common::SharedArray<Vec4> values) { assign(values); }
    void sliceAssign(Vec4 value) { detail::ngFillVecArray(*this, value); }
    void fill(const Vec4& value) { detail::ngFillVecArray(*this, Vec4(value)); }

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
            xPtr_[laneBase_ + i] = source[i].x;
            yPtr_[laneBase_ + i] = source[i].y;
            zPtr_[laneBase_ + i] = source[i].z;
            wPtr_[laneBase_ + i] = source[i].w;
        }
    }
    void assign(std::initializer_list<Vec4> values) { assign(std::vector<Vec4>(values)); }
    void assign(const common::SharedArray<Vec4> source) {
        ensureLength(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            const auto value = source[i];
            set(i, value);
        }
    }
    void copyFrom(const Vec4Array& rhs) {
        const Vec4Array source(rhs);
        ensureLength(source.size());
        if (source.empty()) return;
        std::memmove(xPtr_ + laneBase_, source.xPtr_ + source.laneBase_, source.size() * sizeof(float));
        std::memmove(yPtr_ + laneBase_, source.yPtr_ + source.laneBase_, source.size() * sizeof(float));
        std::memmove(zPtr_ + laneBase_, source.zPtr_ + source.laneBase_, source.size() * sizeof(float));
        std::memmove(wPtr_ + laneBase_, source.wPtr_ + source.laneBase_, source.size() * sizeof(float));
    }

    Vec4Array& operator+=(Vec4Array rhs) {
        detail::ngApplyVecArray<'+'>(*this, rhs);
        return *this;
    }
    Vec4Array& operator-=(Vec4Array rhs) {
        detail::ngApplyVecArray<'-'>(*this, rhs);
        return *this;
    }
    Vec4Array& operator*=(Vec4Array rhs) {
        detail::ngApplyVecArray<'*'>(*this, rhs);
        return *this;
    }
    Vec4Array& operator/=(Vec4Array rhs) {
        detail::ngApplyVecArray<'/'>(*this, rhs);
        return *this;
    }
    Vec4Array& operator+=(Vec4 rhs) {
        detail::ngApplyVecConstant<'+'>(*this, rhs);
        return *this;
    }
    Vec4Array& operator-=(Vec4 rhs) {
        detail::ngApplyVecConstant<'-'>(*this, rhs);
        return *this;
    }
    Vec4Array& operator*=(Vec4 rhs) {
        detail::ngApplyVecConstant<'*'>(*this, rhs);
        return *this;
    }
    Vec4Array& operator/=(Vec4 rhs) {
        detail::ngApplyVecConstant<'/'>(*this, rhs);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec4Array& operator+=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'+'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec4Array& operator-=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'-'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec4Array& operator*=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'*'>(*this, scalar);
        return *this;
    }
    template<class U> requires requires(U value) { static_cast<float>(value); }
    Vec4Array& operator/=(U rhs) {
        const auto scalar = static_cast<float>(rhs);
        detail::ngApplyVecScalar<'/'>(*this, scalar);
        return *this;
    }

    // Source binary ~ duplicates this operand before appending the captured RHS.
    Vec4Array concat(Vec4Array rhs) const {
        auto result = dup();
        result.append(rhs);
        return result;
    }
    Vec4Array concat(Vec4 value) const {
        auto result = dup();
        result.append(value);
        return result;
    }
    // Source right-hand ~ appends the prefix before reading this descriptor.
    Vec4Array prepend(Vec4 value) const {
        Vec4Array result;
        result.append(value);
        result.append(*this);
        return result;
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
        auto out = detail::ngAllocateVecArray<Vec4>(logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            out[i] = Vec4{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i], zPtr_[laneBase_ + i], wPtr_[laneBase_ + i]};
        }
        return out;
    }

    void toArrayInto(std::vector<Vec4>& target) const {
        detail::ngResizeVecArray(target, logicalLength_);
        for (std::size_t i = 0; i < logicalLength_; ++i) {
            target[i] = Vec4{xPtr_[laneBase_ + i], yPtr_[laneBase_ + i],
                zPtr_[laneBase_ + i], wPtr_[laneBase_ + i]};
        }
    }

    Vec4Array dup() const {
        Vec4Array copy;
        copy.ensureLength(logicalLength_);
        if (logicalLength_ != 0 && xPtr_ && yPtr_ && zPtr_ && wPtr_) {
            std::copy_n(xPtr_ + laneBase_, logicalLength_, copy.xPtr_);
            std::copy_n(yPtr_ + laneBase_, logicalLength_, copy.yPtr_);
            std::copy_n(zPtr_ + laneBase_, logicalLength_, copy.zPtr_);
            std::copy_n(wPtr_ + laneBase_, logicalLength_, copy.wPtr_);
        }
        copy.logicalLength_ = logicalLength_;
        copy.laneStride_ = logicalLength_;
        copy.laneBase_ = 0;
        copy.viewCapacity_ = logicalLength_;
        return copy;
    }

    void bindExternalStorage(Vec4Array& storage, std::size_t offset, std::size_t length) {
        const Vec4Array source(storage);
        if (!length || source.sourceBacking_.empty()) {
            ownsStorage_ = false;
            sourceBacking_ = {};
            logicalLength_ = 0;
            viewCapacity_ = 0;
            laneStride_ = storage.logicalLength_;
            laneBase_ = offset;
            rebindSourceLanes();
            return;
        }
        ownsStorage_ = false;
        sourceBacking_ = source.sourceBacking_;
        laneStride_ = storage.logicalLength_;
        laneBase_ = offset;
        logicalLength_ = length;
        viewCapacity_ = length;
        rebindSourceLanes();
    }

    RawStorage rawStorage() const {
#ifndef NDEBUG
        if (!(laneBase_ == 0 && (laneStride_ == logicalLength_ || laneStride_ == 0)))
            throw d_runtime::ngStaticError<d_runtime::AssertError>(
                d_runtime::RetainedString::fromStaticLiteral(
                    "rawStorage is only available for owned contiguous buffers"),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 325);
#endif
        return {sourceBacking_.data(), sourceBacking_.size(), sourceBacking_.retentionOwner()};
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

inline Vec2Array ngVecaFromVectorsSource(common::SharedArray<Vec2> data) {
    auto values = detail::ngDuplicateVecArraySource(data);
    return Vec2Array(values);
}
inline Vec3Array ngVecaFromVectorsSource(common::SharedArray<Vec3> data) {
    auto values = detail::ngDuplicateVecArraySource(data);
    return Vec3Array(values);
}
inline Vec4Array ngVecaFromVectorsSource(common::SharedArray<Vec4> data) {
    auto values = detail::ngDuplicateVecArraySource(data);
    return Vec4Array(values);
}

inline std::vector<Vec2> ngToVectorArraySource(Vec2Array storage) { return storage.toArray(); }
inline std::vector<Vec3> ngToVectorArraySource(Vec3Array storage) { return storage.toArray(); }
inline std::vector<Vec4> ngToVectorArraySource(Vec4Array storage) { return storage.toArray(); }

} // namespace nicxlive::core::math
