#pragma once

#include "../compat/array_range_error.hpp"
#include "../compat/assert_error.hpp"
#include "../compat/system_error.hpp"
#include <initializer_list>
#include <concepts>
#include <algorithm>
#include <memory>
#include <new>
#include <limits>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nicxlive::core::common {
namespace detail {
template<class> inline constexpr bool isSharedArraySharedPointer = false;
template<class U> inline constexpr bool isSharedArraySharedPointer<std::shared_ptr<U>> = true;
template<class Range> using SharedArrayRangeElement =
    std::remove_cv_t<std::remove_pointer_t<decltype(std::declval<const Range&>().data())>>;
}

// Shallow-copy array descriptors with independent length and ordinary native backing.
// Native capacity is not part of the public descriptor contract.
template<class T> class SharedArray {
public:
    SharedArray() = default;
    SharedArray(const SharedArray&) = default;
    SharedArray& operator=(const SharedArray&) = default;
    // Source array assignment retains both descriptors, including rvalue boundaries.
    SharedArray(SharedArray&& other) noexcept : SharedArray(static_cast<const SharedArray&>(other)) {}
    SharedArray& operator=(SharedArray&& other) noexcept {
        return *this = static_cast<const SharedArray&>(other);
    }
    SharedArray(std::size_t count, const T& value) {
        if (count) { storage_ = allocateStorage(count, value); length_ = count; }
    }
    T& front() { return (*this)[0]; }
    const T& front() const { return (*this)[0]; }
    void assign(std::size_t count, const T& value) { *this = SharedArray(count, value); }
    T* erase(T* position) {
        const auto index = static_cast<std::size_t>(position - begin());
        if (index >= length_) throw d_runtime::ArrayIndexError(index, length_);
        for (auto i = index; i + 1 < length_; ++i) (*this)[i] = (*this)[i + 1];
        --length_;
        return data() + index;
    }
    T* erase(T* first, T* last) {
        if (first == last) return first;
        const auto start = static_cast<std::size_t>(first - begin());
        const auto finish = static_cast<std::size_t>(last - begin());
        if (start > finish || finish > length_) throw d_runtime::ArraySliceError(start, finish, length_);
        const auto count = finish - start;
        for (auto i = start; i + count < length_; ++i) (*this)[i] = (*this)[i + count];
        length_ -= count;
        return start ? data() + start : data();
    }
    T* insert(T* position, const T& value) {
        const auto index = empty() ? 0 : static_cast<std::size_t>(position - begin());
        if (index > length_) throw d_runtime::ArraySliceError(0, index, length_);
        T copy = value;
        push_back(T{});
        for (auto i = length_ - 1; i > index; --i) (*this)[i] = (*this)[i - 1];
        (*this)[index] = std::move(copy);
        return data() + index;
    }

    // std.array.insertInPlace source order (Andrei Alexandrescu/Jonathan M Davis,
    // Boost Software License 1.0; licenses/phobos-locale-case.txt).
    void insertValue(std::size_t position, T value) {
        const auto oldLength = length_;
        resize(oldLength + 1);
        if (position > oldLength) throw d_runtime::ArraySliceError(position, oldLength, length_);
        for (auto i = oldLength; i > position; --i) (*this)[i] = (*this)[i - 1];
        (*this)[position] = std::move(value);
    }
    void insertRange(std::size_t position, SharedArray values) {
        const auto count = values.size();
        if (!count) return;
        const auto oldLength = length_;
        resize(oldLength + count);
        if (position > oldLength) throw d_runtime::ArraySliceError(position, oldLength, length_);
        for (auto i = oldLength; i > position; --i) (*this)[i + count - 1] = (*this)[i - 1];
        for (std::size_t i = 0; i < count; ++i) {
            auto value = values[i];
            (*this)[position + i] = std::move(value);
        }
    }

    SharedArray removed(std::size_t position) const {
        // std.algorithm.remove(stable) advances two retained descriptors before
        // moving the remaining values; malformed positions fail at those slices.
#ifndef NDEBUG
        if (position > length_) throw d_runtime::AssertError(
            "range is smaller than amount of items to pop", __FILE__, __LINE__);
#endif
        auto source = slice(position, length_);
        auto destination = source;
#ifndef NDEBUG
        if (source.size() < 1) throw d_runtime::AssertError(
            "range is smaller than amount of items to pop", __FILE__, __LINE__);
#endif
        source = source.slice(1, source.size());
        auto result = slice(0, length_ - 1);
        for (std::size_t i = 0; i < source.size(); ++i) destination[i] = source[i];
        return result;
    }

    SharedArray(std::initializer_list<T> values)
        : storage_(values.size() ? allocateStorage(values) : nullptr), length_(values.size()) {}
    explicit SharedArray(std::vector<T> values)
        : storage_(allocateStorage(std::move(values))), length_(storage_->size()) {}
    // Adopt the real existing vector allocation; the Source descriptor keeps its own length.
    explicit SharedArray(std::shared_ptr<std::vector<T>> values)
        : storage_(std::move(values)), length_(storage_ ? storage_->size() : 0) {}
    // The caller guarantees process-static storage, as for an imported D array.
    static SharedArray staticView(T* values, std::size_t length) noexcept {
        SharedArray result;
        result.externalData_ = values;
        result.length_ = length;
        return result;
    }
    // Retain actual typed cells from another ordinary array descriptor.
    // Shrinking/slicing keeps those cells; growth uses the existing vector model.
    static SharedArray retainedView(std::shared_ptr<void> owner, T* values, std::size_t length) {
        if (length && (!owner || !values))
            throw std::invalid_argument("Retained array requires backing");
        SharedArray result;
        result.externalOwner_ = std::move(owner);
        result.externalData_ = values;
        result.length_ = length;
        return result;
    }
    std::size_t size() const { return length_; }
    bool empty() const { return length_ == 0; }
    bool isNull() const { return !storage_ && !externalOwner_ && !externalData_; }
    T* data() {
        auto* base = storage_ ? storage_->data() : externalData_;
        return offset_ ? base + offset_ : base;
    }
    const T* data() const {
        const auto* base = storage_ ? storage_->data() : externalData_;
        return offset_ ? base + offset_ : base;
    }
    T* begin() { return data(); }
    const T* begin() const { return data(); }
    T* end() { return length_ ? data() + length_ : data(); }
    const T* end() const { return length_ ? data() + length_ : data(); }
    T& operator[](std::size_t index) {
        if (index >= length_) throw d_runtime::ArrayIndexError(index, length_);
        return data()[index];
    }
    const T& operator[](std::size_t index) const {
        if (index >= length_) throw d_runtime::ArrayIndexError(index, length_);
        return data()[index];
    }
    T& at(std::size_t index) { return (*this)[index]; }
    const T& at(std::size_t index) const { return (*this)[index]; }
    SharedArray duplicate() const {
        if (!length_) return {};
        return SharedArray(std::vector<T>(begin(), end()));
    }
    SharedArray slice(std::size_t first, std::size_t last) const {
        if (first > last || last > length_) throw d_runtime::ArraySliceError(first, last, length_);
        auto result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        return result;
    }
    std::shared_ptr<void> retentionOwner() const noexcept {
        return storage_ ? std::shared_ptr<void>(storage_) : externalOwner_;
    }
    // A void owner may retain a different buffer type; never downcast it.
    std::shared_ptr<std::vector<T>> vectorBacking() const noexcept { return storage_; }
    std::shared_ptr<T> retainPointer(std::size_t index) {
        return std::shared_ptr<T>(retentionOwner(), std::addressof((*this)[index]));
    }
    std::shared_ptr<const T> retainPointer(std::size_t index) const {
        return std::shared_ptr<const T>(retentionOwner(), std::addressof((*this)[index]));
    }
    void clear() {
        storage_.reset(); externalOwner_.reset(); externalData_ = nullptr;
        length_ = offset_ = 0;
    }
    void push_back(T value) {
        if (externalOwner_ || externalData_) *this = duplicate();
        if (storage_ && (offset_ + length_ != storage_->size() || storage_->size() == storage_->capacity()))
            *this = duplicate();
        if (!storage_) storage_ = allocateStorage();
        storage_->push_back(std::move(value));
        ++length_;
    }
    void appendRange(SharedArray values) { appendRangeImpl(values); }
    template<class Range>
        requires (!std::is_same_v<std::remove_cvref_t<Range>, SharedArray> &&
            requires (const Range& values) {
                { values.size() } -> std::convertible_to<std::size_t>;
                requires detail::isSharedArraySharedPointer<decltype(values.retentionOwner())>;
                requires std::is_pointer_v<decltype(values.data())>;
                requires (std::is_same_v<detail::SharedArrayRangeElement<Range>, T> ||
                    (detail::isSharedArraySharedPointer<T> &&
                     detail::isSharedArraySharedPointer<detail::SharedArrayRangeElement<Range>> &&
                     std::is_convertible_v<decltype(values.data()[0]), T>));
            })
    void appendRange(Range values) { appendRangeImpl(values); }

    void resize(std::size_t count) {
        if (count <= length_) { length_ = count; return; }
        while (length_ < count) {
            if constexpr (std::is_floating_point_v<T>) push_back(std::numeric_limits<T>::quiet_NaN());
            else push_back(T{});
        }
    }
private:
    // Array range append: Digital Mars 2000-2019, BSL-1.0.
    // Source: core/internal/array/appending.d70-110; full BSL in
    // core/licenses/druntime-array-errors.txt. Existing ordinary owners retain both inputs.
    template<class Range> void appendRangeImpl(Range values) {
        const auto count = values.size();
        if (!count) return;
        const auto oldLength = length_;
        if (count > (std::numeric_limits<std::size_t>::max)() - oldLength)
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
                d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        const auto newLength = oldLength + count;
        const auto previous = *this;
        const bool reuse = storage_ && offset_ + length_ == storage_->size()
            && count <= storage_->capacity() - storage_->size();
        SharedArray target;
        try {
            if (reuse) {
                storage_->resize(storage_->size() + count);
                target = *this;
                target.length_ = newLength;
            } else {
                target = SharedArray(std::vector<T>(newLength));
            }
        } catch (const std::bad_alloc&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
                d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
                d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
        if (!reuse)
            for (std::size_t i = 0; i < oldLength; ++i) target.data()[i] = previous.data()[i];
        *this = target;
        for (std::size_t i = 0; i < count; ++i) target.data()[oldLength + i] = values.data()[i];
    }

    template<class... Args> static std::shared_ptr<std::vector<T>> allocateStorage(Args&&... arguments) {
        return std::make_shared<std::vector<T>>(std::forward<Args>(arguments)...);
    }
    std::shared_ptr<std::vector<T>> storage_;
    std::shared_ptr<void> externalOwner_;
    T* externalData_ = nullptr;
    std::size_t length_ = 0;
    std::size_t offset_ = 0;
};
// std::vector<bool> cannot provide the bool references and contiguous storage of D bool[].
template<> class SharedArray<bool> {
public:
    SharedArray() = default;
    SharedArray(const SharedArray&) = default;
    SharedArray& operator=(const SharedArray&) = default;
    // Rvalue transport still copies the real source array descriptor.
    SharedArray(SharedArray&& other) noexcept : SharedArray(static_cast<const SharedArray&>(other)) {}
    SharedArray& operator=(SharedArray&& other) noexcept {
        return *this = static_cast<const SharedArray&>(other);
    }
    SharedArray(std::initializer_list<bool> values) {
        if (values.size()) { allocate(values.size()); std::copy(values.begin(), values.end(), data()); }
    }
    explicit SharedArray(std::vector<bool> values) {
        allocate(values.size());
        for (std::size_t i = 0; i < values.size(); ++i) data()[i] = values[i];
    }
    SharedArray(std::size_t count, bool value) {
        if (count) { allocate(count); std::fill(begin(), end(), value); }
    }
    std::size_t size() const { return length_; }
    bool empty() const { return length_ == 0; }
    bool isNull() const { return !storage_; }
    bool* data() { return storage_ ? storage_.get() + offset_ : nullptr; }
    const bool* data() const { return storage_ ? storage_.get() + offset_ : nullptr; }
    bool* begin() { return data(); }
    const bool* begin() const { return data(); }
    bool* end() { return length_ ? data() + length_ : data(); }
    const bool* end() const { return length_ ? data() + length_ : data(); }
    bool& operator[](std::size_t i) { check(i); return data()[i]; }
    const bool& operator[](std::size_t i) const { check(i); return data()[i]; }
    bool& front() { return (*this)[0]; }
    const bool& front() const { return (*this)[0]; }
    SharedArray duplicate() const {
        if (!length_) return {};
        SharedArray result(length_, false);
        std::copy(begin(), end(), result.begin());
        return result;
    }
    SharedArray slice(std::size_t first, std::size_t last) const {
        if (first > last || last > length_) throw d_runtime::ArraySliceError(first, last, length_);
        auto result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        // Append capacity is not part of the D-compatible public contract.
        // A bool subview detaches on growth instead of writing past its extent.
        result.capacity_ = result.length_;
        return result;
    }
    std::shared_ptr<bool> retainPointer(std::size_t index) {
        return std::shared_ptr<bool>(storage_, std::addressof((*this)[index]));
    }
    std::shared_ptr<const bool> retainPointer(std::size_t index) const {
        return std::shared_ptr<const bool>(storage_, std::addressof((*this)[index]));
    }
    void clear() { storage_.reset(); length_ = capacity_ = offset_ = 0; }
    void resize(std::size_t count) {
        if (count <= length_) { length_ = count; return; }
        if (length_ < capacity_) *this = duplicate();
        if (count > capacity_) {
            SharedArray replacement(count, false);
            if (length_) std::copy(begin(), end(), replacement.begin());
            *this = std::move(replacement);
        } else {
            std::fill(data() + length_, data() + count, false);
            length_ = count;
        }
    }
    void push_back(bool value) { const auto i = length_; resize(i + 1); data()[i] = value; }
    void assign(std::size_t count, bool value) { *this = SharedArray(count, value); }
    bool* erase(bool* position) {
        const auto index = static_cast<std::size_t>(position - begin()); check(index);
        for (auto i = index; i + 1 < length_; ++i) data()[i] = data()[i + 1];
        --length_;
        return data() + index;
    }
    bool* insert(bool* position, bool value) {
        const auto index = empty() ? 0 : static_cast<std::size_t>(position - begin());
        if (index > length_) throw d_runtime::ArraySliceError(0, index, length_);
        push_back(false);
        for (auto i = length_ - 1; i > index; --i) data()[i] = data()[i - 1];
        data()[index] = value;
        return data() + index;
    }
    void insertValue(std::size_t position, bool value) {
        const auto oldLength = length_;
        resize(oldLength + 1);
        if (position > oldLength) throw d_runtime::ArraySliceError(position, oldLength, length_);
        for (auto i = oldLength; i > position; --i) (*this)[i] = (*this)[i - 1];
        (*this)[position] = std::move(value);
    }
    void insertRange(std::size_t position, SharedArray values) {
        const auto count = values.size();
        if (!count) return;
        const auto oldLength = length_;
        resize(oldLength + count);
        if (position > oldLength) throw d_runtime::ArraySliceError(position, oldLength, length_);
        for (auto i = oldLength; i > position; --i) (*this)[i + count - 1] = (*this)[i - 1];
        for (std::size_t i = 0; i < count; ++i) {
            bool value = values[i];
            (*this)[position + i] = std::move(value);
        }
    }

    SharedArray removed(std::size_t position) const {
        // std.algorithm.remove(stable) advances two retained descriptors before
        // moving the remaining values; malformed positions fail at those slices.
#ifndef NDEBUG
        if (position > length_) throw d_runtime::AssertError(
            "range is smaller than amount of items to pop", __FILE__, __LINE__);
#endif
        auto source = slice(position, length_);
        auto destination = source;
#ifndef NDEBUG
        if (source.size() < 1) throw d_runtime::AssertError(
            "range is smaller than amount of items to pop", __FILE__, __LINE__);
#endif
        source = source.slice(1, source.size());
        auto result = slice(0, length_ - 1);
        for (std::size_t i = 0; i < source.size(); ++i) destination[i] = source[i];
        return result;
    }

private:
    void check(std::size_t i) const { if (i >= length_) throw d_runtime::ArrayIndexError(i, length_); }
    void allocate(std::size_t count) {
        // Guard the actual contiguous allocation even if the shared control-block
        // construction fails. The alias owns that same buffer through its value.
        std::unique_ptr<bool[]> values(new bool[count ? count : 1]{});
        auto owner = std::make_shared<std::unique_ptr<bool[]>>(std::move(values));
        auto* elements = owner->get();
        storage_ = std::shared_ptr<bool[]>(std::move(owner), elements);
        length_ = capacity_ = count;
        offset_ = 0;
    }
    std::shared_ptr<bool[]> storage_;
    std::size_t length_ = 0;
    std::size_t capacity_ = 0;
    std::size_t offset_ = 0;
};

} // namespace nicxlive::core::common
