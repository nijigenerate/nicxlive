#pragma once

#include "common/shared_array.hpp"
#include "common/string_key_map.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace nicxlive::core {

// The actual ubyte[][string] descriptor used by Puppet and its binary writer.
class PuppetExtData {
    using Key = d_runtime::RetainedString;
public:
    using Bytes = common::SharedArray<std::uint8_t>;
private:
    using Storage = common::PolicyKeyMap<Key, Bytes, common::StringKeyPolicy>;
    Storage entries_;
public:
    using iterator = Storage::iterator;
    using const_iterator = Storage::const_iterator;
    class MappedReference {
        Storage::MappedReference value_;
        friend class PuppetExtData;
        explicit MappedReference(Storage::MappedReference value) : value_(std::move(value)) {}
    public:
        operator Bytes&() const { return value_; }
        MappedReference& operator=(Bytes value) { value_ = std::move(value); return *this; }
        MappedReference& operator=(std::vector<std::uint8_t> value) {
            return *this = Bytes(std::move(value));
        }
        MappedReference& operator=(const MappedReference& other) {
            return *this = Bytes(static_cast<Bytes&>(other.value_));
        }
    };
    PuppetExtData() = default;
    PuppetExtData(const PuppetExtData&) = default;
    PuppetExtData& operator=(const PuppetExtData&) = default;
    PuppetExtData(PuppetExtData&& other) noexcept : entries_(other.entries_) {}
    PuppetExtData& operator=(PuppetExtData&& other) noexcept {
        entries_ = other.entries_;
        return *this;
    }
    bool isNull() const noexcept { return entries_.isNull(); }
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }
    const void* identity() const noexcept { return entries_.identity(); }
    iterator begin() { return entries_.begin(); }
    iterator end() { return entries_.end(); }
    const_iterator begin() const { return entries_.begin(); }
    const_iterator end() const { return entries_.end(); }
    iterator find(const Key& key) { return entries_.find(key); }
    const_iterator find(const Key& key) const { return entries_.find(key); }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    iterator find(const K& key) {
        return find(Key::fromBorrowedView(std::string_view(key)));
    }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    const_iterator find(const K& key) const {
        return find(Key::fromBorrowedView(std::string_view(key)));
    }
    MappedReference operator[](Key key) { return MappedReference(entries_[key]); }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    MappedReference operator[](const K& key) { return (*this)[Key(std::string_view(key))]; }
    std::size_t erase(const Key& key) { return entries_.erase(key); }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    std::size_t erase(const K& key) {
        return erase(Key::fromBorrowedView(std::string_view(key)));
    }
    void clear() { entries_.clear(); }
    void reset() { entries_ = Storage{}; }
};

} // namespace nicxlive::core
