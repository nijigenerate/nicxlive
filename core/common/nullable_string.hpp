#pragma once

#include "../compat/array_range_error.hpp"
#include <cstddef>
#include <compare>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nicxlive::core::common {

// Immutable retained byte descriptor. Nullness is independent of byte length.
// Legacy string conversions are explicit value boundaries, not the stored state.
class NullableString {
public:
    NullableString() noexcept = default;
    NullableString(const NullableString&) = default;
    NullableString& operator=(const NullableString&) = default;
    NullableString(NullableString&& other) noexcept
        : NullableString(static_cast<const NullableString&>(other)) {}
    NullableString& operator=(NullableString&& other) noexcept {
        return *this = static_cast<const NullableString&>(other);
    }
    NullableString(std::nullptr_t) noexcept {}
    NullableString(const char* value)
        : NullableString(value ? std::string_view(value) : std::string_view{}) {}
    NullableString(std::string value);
    NullableString(std::string_view value);
    template<std::size_t N> static NullableString fromStaticLiteral(const char (&value)[N]) noexcept {
        static_assert(N > 0);
        NullableString result;
        result.bytes_ = value;
        result.length_ = N - 1;
        return result;
    }
    // Caller guarantees static lifetime for the complete counted view.
    static NullableString fromStaticView(std::string_view value) noexcept {
        NullableString result;
        result.bytes_ = value.data();
        result.length_ = value.size();
        return result;
    }
    static NullableString retainedView(std::shared_ptr<const void> owner, const char* bytes, std::size_t length) {
        if (!owner || !bytes) throw std::invalid_argument("Retained string requires an owner and bytes");
        NullableString result;
        result.owner_ = std::move(owner);
        result.bytes_ = bytes;
        result.length_ = length;
        return result;
    }
    bool isNull() const noexcept { return !storage_ && !bytes_; }
    bool empty() const noexcept { return length_ == 0; }
    std::size_t size() const noexcept { return length_; }
    std::size_t length() const noexcept { return length_; }
    const char* data() const noexcept {
        const auto base = storage_ ? storage_->data() : bytes_;
        return base ? base + offset_ : nullptr;
    }
    // Share the real immutable allocation when bridging another descriptor type.
    // Empty ownership denotes a static literal, not an instruction to clone bytes.
    std::shared_ptr<const void> retentionOwner() const noexcept {
        return storage_ ? std::shared_ptr<const void>(storage_) : owner_;
    }
    std::string_view view() const noexcept {
        return isNull() ? std::string_view{} : std::string_view(data(), length_);
    }
    const char* begin() const noexcept { return data(); }
    const char* end() const noexcept { return length_ ? data() + length_ : data(); }
    const char& operator[](std::size_t index) const {
        if (index >= length_) raiseIndexError(index, length_);
        return data()[index];
    }
    NullableString slice(std::size_t first, std::size_t last) const {
        if (first > last || last > length_) raiseSliceError(first, last, length_);
        auto result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        result.terminated_.reset();
        return result;
    }
    std::size_t find(char value, std::size_t start = 0) const noexcept { return view().find(value, start); }
    std::size_t find(std::string_view value, std::size_t start = 0) const noexcept {
        return view().find(value, start);
    }
    std::size_t find_last_of(std::string_view value) const noexcept { return view().find_last_of(value); }
    std::size_t find_last_of(char value) const noexcept { return view().find_last_of(value); }
    bool starts_with(std::string_view value) const noexcept { return view().starts_with(value); }
    bool ends_with(std::string_view value) const noexcept { return view().ends_with(value); }
    std::string substr(std::size_t first = 0, std::size_t count = std::string::npos) const;
    void resize(std::size_t length) {
        // Legacy truncation adapter denotes the immutable prefix slice [0 .. length].
        if (length > length_) raiseSliceError(0, length, length_);
        length_ = length;
        terminated_.reset();
    }
    void clear() noexcept { length_ = 0; terminated_.reset(); }
    void reset() noexcept { *this = NullableString{}; }
    const std::string& string() const;
    const char* c_str() const { return string().c_str(); }
    operator std::string_view() const noexcept { return view(); }
    operator const std::string&() const { return string(); }
    friend bool operator==(const NullableString& a, const NullableString& b) noexcept {
        return a.view() == b.view();
    }
    friend auto operator<=>(const NullableString& a, const NullableString& b) noexcept {
        return a.view() <=> b.view();
    }
    friend bool operator==(const NullableString& a, std::string_view b) noexcept { return a.view() == b; }
    friend bool operator==(std::string_view a, const NullableString& b) noexcept { return a == b.view(); }
    friend bool operator==(const NullableString& a, const std::string& b) noexcept { return a.view() == b; }
    friend bool operator==(const NullableString& a, const char* b) noexcept {
        return a.view() == (b ? std::string_view(b) : std::string_view{});
    }
    friend std::string operator+(const NullableString& a, std::string_view b) {
        return concatenate(a.view(), b);
    }
    friend std::string operator+(std::string_view a, const NullableString& b) {
        return concatenate(a, b);
    }
    friend std::ostream& operator<<(std::ostream& out, const NullableString& value) {
        return out.write(value.data() ? value.data() : "", static_cast<std::streamsize>(value.size()));
    }
private:
    [[noreturn]] static void raiseIndexError(std::size_t index, std::size_t length);
    [[noreturn]] static void raiseSliceError(std::size_t first, std::size_t last, std::size_t length);
    static std::string concatenate(std::string_view left, std::string_view right);
    static std::string concatenate(std::string_view left, const NullableString& right);
    std::shared_ptr<const std::string> storage_;
    std::shared_ptr<const void> owner_;
    const char* bytes_ = nullptr;
    std::size_t offset_ = 0, length_ = 0;
    mutable std::shared_ptr<const std::string> terminated_;
};

} // namespace nicxlive::core::common
