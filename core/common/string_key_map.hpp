#pragma once
#include "resource_key_map.hpp"
#include "../compat/phobos_string_hash.hpp"
#include "../compat/retained_string.hpp"
#include "../compat/object_metadata.hpp"
#include <source_location>
#include <string_view>

namespace nicxlive::core::common {
struct StringKeyPolicy {
    struct Hash {
        std::size_t operator()(const d_runtime::RetainedString& key) const noexcept {
            return d_runtime::ngPhobosStringHash(key.view());
        }
    };
    struct Equal {
        bool operator()(const d_runtime::RetainedString& query,
            const d_runtime::RetainedString& stored) const noexcept {
            return query.view() == stored.view();
        }
    };
};

// The existing canonical AA owns every real entry and implements all table operations.
template<class Value> class StringKeyMap {
    using Storage = PolicyKeyMap<d_runtime::RetainedString, Value, StringKeyPolicy>;
    Storage entries_;
public:
    bool isNull() const noexcept { return entries_.isNull(); }
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }
    const void* identity() const noexcept { return entries_.identity(); }
    std::shared_ptr<const Value> findRetained(std::string_view key) const {
        return entries_.findRetained(d_runtime::RetainedString::fromBorrowedView(key));
    }
    const Value* find(std::string_view key) const { return findRetained(key).get(); }
    bool contains(std::string_view key) const {
        return entries_.contains(d_runtime::RetainedString::fromBorrowedView(key));
    }
    Value get(std::string_view key, std::source_location source = std::source_location::current()) const {
        const auto value = findRetained(key);
        if (!value) throw d_runtime::ngStaticError<d_runtime::RangeError>(source);
        return *value;
    }
    auto operator[](d_runtime::RetainedString key) { return entries_[key]; }
    void setRetained(d_runtime::RetainedString key, Value value) {
        auto destination = entries_[key];
        destination = std::move(value);
    }
    void set(std::string_view key, Value value) {
        const auto probe = d_runtime::RetainedString::fromBorrowedView(key);
        if (entries_.contains(probe)) {
            auto destination = entries_[probe];
            destination = std::move(value);
        } else setRetained(d_runtime::RetainedString(key), std::move(value));
    }
    bool remove(std::string_view key) {
        return entries_.erase(d_runtime::RetainedString::fromBorrowedView(key)) != 0;
    }
    void clear() { entries_.clear(); }
    void reset() { entries_ = Storage{}; }
    StringKeyMap duplicate() const {
        StringKeyMap result;
        const auto snapshot = entries_;
        for (const auto& entry : snapshot) result.setRetained(entry.first, Value(entry.second));
        return result;
    }
    template<class F> void forEach(F&& visitor) const {
        const auto snapshot = entries_;
        for (const auto& entry : snapshot) {
            const auto key = entry.first;
            const auto value = Value(entry.second);
            visitor(key.view(), value);
        }
    }
    template<class F> void forEachRetained(F&& visitor) const {
        const auto snapshot = entries_;
        for (const auto& entry : snapshot) {
            const auto key = entry.first;
            const auto value = Value(entry.second);
            visitor(key, value);
        }
    }
    // Source foreach(k, ref v) callers need the actual entry value. The
    // descriptor and iterator retain its real table and selected entry over
    // callbacks; existing value-copy enumeration APIs remain independent.
    template<class F> void forEachEntry(F&& visitor) {
        auto snapshot = entries_;
        for (auto& entry : snapshot) visitor(entry.first, entry.second);
    }
};
}

namespace nicxlive::core::d_runtime {

template<class Value> struct SourceTypeSpellings<common::StringKeyMap<Value>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return ngSourceAssociativeArrayTypeSpellings<RetainedString, Value>();
    }
};

} // namespace nicxlive::core::d_runtime
