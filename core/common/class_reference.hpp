#pragma once
#include <memory>

namespace nicxlive::core::common {
// An actual retaining class-reference slot. The legacy lock/expired spellings
// expose the same strong handle; they do not reintroduce weak ownership.
// Owners must explicitly detach cycles during normal lifecycle cleanup.
template<class T> class ClassReference {
    std::shared_ptr<T> value_;
public:
    ClassReference() = default;
    ClassReference(std::nullptr_t) noexcept {}
    ClassReference(const std::shared_ptr<T>& value) noexcept : value_(value) {}
    ClassReference(const ClassReference&) = default;
    ClassReference& operator=(const ClassReference&) = default;
    ClassReference& operator=(const std::shared_ptr<T>& value) noexcept {
        value_ = value;
        return *this;
    }
    ClassReference& operator=(std::nullptr_t) noexcept {
        value_.reset();
        return *this;
    }
    std::shared_ptr<T> lock() const noexcept { return value_; }
    bool expired() const noexcept { return !value_; }
    void reset() noexcept { value_.reset(); }
    T* get() const noexcept { return value_.get(); }
    explicit operator bool() const noexcept { return bool(value_); }
};
}
