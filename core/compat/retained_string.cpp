#include "retained_string.hpp"
#include "../common/nullable_string.hpp"
#include "static_error.hpp"
#include "system_error.hpp"

#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace {
template<class Build>
auto allocateStringBytes(Build&& build) {
    using nicxlive::core::d_runtime::ngStaticError;
    using nicxlive::core::d_runtime::OutOfMemoryError;
    try {
        return std::forward<Build>(build)();
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}

template<class Text>
std::shared_ptr<const std::string> retainOwnedBytes(Text&& text) {
    return allocateStringBytes([&] {
        return std::make_shared<const std::string>(std::forward<Text>(text));
    });
}
}

namespace nicxlive::core::d_runtime {
void RetainedString::raiseSliceError(std::size_t first, std::size_t last, std::size_t length) {
    throw ngStaticError<ArraySliceError>(first, last, length, std::source_location::current());
}

RetainedString::RetainedString(std::string_view text) {
    if (text.data()) {
        storage_ = retainOwnedBytes(text);
        length_ = storage_->size();
    }
}

RetainedString::RetainedString(std::string text)
    : storage_(retainOwnedBytes(std::move(text))), length_(storage_->size()) {}

RetainedString::RetainedString(const common::NullableString& text) noexcept {
    if (text.isNull()) return;
    storage_ = std::shared_ptr<const std::string>(text.retentionOwner(), nullptr);
    literalData_ = text.data();
    length_ = text.size();
}
common::NullableString RetainedString::toNullableString() const {
    if (isNull()) return {};
    if (auto owner = retentionOwner())
        return common::NullableString::retainedView(std::move(owner), data(), size());
    return common::NullableString::fromStaticView(view());
}
} // namespace nicxlive::core::d_runtime

namespace nicxlive::core::common {
void NullableString::raiseIndexError(std::size_t index, std::size_t length) {
    throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(index, length, std::source_location::current());
}

void NullableString::raiseSliceError(std::size_t first, std::size_t last, std::size_t length) {
    throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(first, last, length, std::source_location::current());
}

NullableString::NullableString(std::string value)
    : storage_(retainOwnedBytes(std::move(value))), length_(storage_->size()) {}

NullableString::NullableString(std::string_view value) {
    if (value.data()) {
        storage_ = retainOwnedBytes(value);
        length_ = storage_->size();
    }
}

std::string NullableString::substr(std::size_t first, std::size_t count) const {
    const auto bytes = view().substr(first, count);
    return allocateStringBytes([&] { return std::string(bytes); });
}

const std::string& NullableString::string() const {
    if (storage_ && offset_ == 0 && length_ == storage_->size()) return *storage_;
    if (!terminated_) terminated_ = retainOwnedBytes(view());
    return *terminated_;
}

std::string NullableString::concatenate(std::string_view left, std::string_view right) {
    return allocateStringBytes([&] {
        auto result = std::string(left);
        result.append(right);
        return result;
    });
}

std::string NullableString::concatenate(std::string_view left, const NullableString& right) {
    return allocateStringBytes([&] {
        auto result = std::string(left);
        result.append(right.view());
        return result;
    });
}
} // namespace nicxlive::core::common
