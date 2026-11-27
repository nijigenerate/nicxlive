#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nicxlive::core::common { class NullableString; }

namespace nicxlive::core::d_runtime {

// D string descriptors retain immutable bytes and distinguish null from empty.
// Byte validation belongs to consumers, not assignment of a string descriptor.
class RetainedString {
public:
    struct StaticLiteralTag {
        explicit constexpr StaticLiteralTag() noexcept = default;
    };
    // The caller guarantees static lifetime, as with fromStaticLiteral below.
    template<std::size_t N>
    constexpr RetainedString(StaticLiteralTag, const char (&text)[N]) noexcept
        : length_(N - 1), literalData_(text) {
        static_assert(N > 0);
    }
    RetainedString() noexcept = default;
    RetainedString(const RetainedString&) = default;
    RetainedString(RetainedString&& other) noexcept : RetainedString(static_cast<const RetainedString&>(other)) {}
    RetainedString& operator=(const RetainedString& other) noexcept {
        if (this == &other) return *this;
        auto previous = std::move(storage_);
        storage_ = other.storage_;
        offset_ = other.offset_;
        length_ = other.length_;
        literalEmpty_ = other.literalEmpty_;
        literalData_ = other.literalData_;
        return *this;
    }
    RetainedString& operator=(RetainedString&& other) noexcept {
        return *this = static_cast<const RetainedString&>(other);
    }
    RetainedString(std::nullptr_t) noexcept {}
    RetainedString(const ::nicxlive::core::common::NullableString& text) noexcept;
    ::nicxlive::core::common::NullableString toNullableString() const;
    RetainedString(const char* text) : RetainedString(text ? std::string_view(text) : std::string_view{}) {}
    RetainedString(std::string_view text);
    RetainedString(std::string text);

    static RetainedString retainedView(std::shared_ptr<const void> owner,
        const char* bytes, std::size_t length) {
        if (!owner || !bytes) throw std::invalid_argument("Retained string requires an owner and bytes");
        RetainedString result;
        // A null alias retains the actual external descriptor's control block.
        // Its pointee is never used as a std::string; data() uses the byte view.
        result.storage_ = std::shared_ptr<const std::string>(std::move(owner), nullptr);
        result.literalData_ = bytes;
        result.length_ = length;
        return result;
    }

    // Empty D literals have static storage and require no allocation on assignment.
    static RetainedString emptyLiteral() noexcept {
        RetainedString result;
        result.literalEmpty_ = true;
        return result;
    }

    // The caller must supply static-lifetime bytes, such as a string literal.
    template<std::size_t N> static RetainedString fromStaticLiteral(const char (&text)[N]) noexcept {
        static_assert(N > 0);
        RetainedString result;
        result.literalData_ = text;
        result.length_ = N - 1;
        return result;
    }

    // The caller guarantees static lifetime for every byte of this view.
    // Preserve null and nonnull empty views without allocating an owner.
    // Borrow the actual backing bytes without an owner or allocation. The
    // caller guarantees every use remains within that backing's lifetime;
    // staticError uses this only for its actual OS-thread-local message array.
    static RetainedString fromBorrowedView(std::string_view text) noexcept {
        RetainedString result;
        result.literalData_ = text.data();
        result.length_ = text.size();
        return result;
    }

    static RetainedString fromStaticView(std::string_view text) noexcept {
        RetainedString result;
        result.literalData_ = text.data();
        result.length_ = text.size();
        return result;
    }

    bool isNull() const noexcept { return !storage_ && !literalEmpty_ && !literalData_; }
    bool empty() const noexcept { return size() == 0; }
    // A null alias may retain a real owner, so pointer truth is insufficient.
    // Empty ownership denotes static bytes; no byte copy is requested.
    std::shared_ptr<const void> retentionOwner() const noexcept {
        if (storage_.use_count() == 0) return {};
        return std::shared_ptr<const void>(storage_, data());
    }
    std::size_t size() const noexcept { return (storage_ || literalData_) ? length_ : 0; }
    const char* data() const noexcept {
        if (storage_) return storage_->data() + offset_;
        if (literalData_) return literalData_ + offset_;
        return literalEmpty_ ? emptyLiteralBytes_ : nullptr;
    }
    std::string_view view() const noexcept {
        if (storage_) return std::string_view(storage_->data() + offset_, length_);
        if (literalData_) return std::string_view(literalData_ + offset_, length_);
        return literalEmpty_ ? std::string_view(emptyLiteralBytes_, 0) : std::string_view{};
    }
    // A view retains the original immutable bytes; its end need not be C-terminated.
    RetainedString subslice(std::size_t first, std::size_t last) const {
        if (first > last || last > size()) raiseSliceError(first, last, size());
        RetainedString result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        return result;
    }
    void reset() noexcept {
        auto previous = std::move(storage_);
        offset_ = 0; length_ = 0; literalEmpty_ = false; literalData_ = nullptr;
    }
    friend bool operator==(const RetainedString& left, const RetainedString& right) noexcept {
        return left.view() == right.view();
    }

private:
    [[noreturn]] static void raiseSliceError(std::size_t first, std::size_t last, std::size_t length);
    inline static constexpr char emptyLiteralBytes_[1] = {'\0'};
    std::shared_ptr<const std::string> storage_;
    std::size_t offset_{};
    std::size_t length_{};
    bool literalEmpty_ = false;
    const char* literalData_ = nullptr;
};

} // namespace nicxlive::core::d_runtime
