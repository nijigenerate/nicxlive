#pragma once
#include <concepts>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include "animation.hpp"
#include "../common/string_key_map.hpp"

namespace nicxlive::core {
class AnimationMap {
    using Key = d_runtime::RetainedString;
    using Storage = common::PolicyKeyMap<Key, Animation, common::StringKeyPolicy>;
    using Entry = Storage::iterator::value_type;
    Storage entries_;

    template<bool Constant> class Iterator {
        friend class AnimationMap;
        template<bool> friend class Iterator;
        Storage::iterator position_;
        explicit Iterator(Storage::iterator position) : position_(std::move(position)) {}
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Entry;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Constant, const Entry&, Entry&>;
        using pointer = std::conditional_t<Constant, const Entry*, Entry*>;
        Iterator() = default;
        template<bool Other> requires (Constant && !Other)
        Iterator(const Iterator<Other>& other) : position_(other.position_) {}
        reference operator*() const { return *position_; }
        pointer operator->() const { return std::addressof(operator*()); }
        Iterator& operator++() { ++position_; return *this; }
        Iterator operator++(int) { auto previous = *this; ++*this; return previous; }
        friend bool operator==(const Iterator& left, const Iterator& right) {
            return left.position_ == right.position_;
        }
        friend bool operator!=(const Iterator& left, const Iterator& right) { return !(left == right); }
    };
public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;
    bool isNull() const noexcept { return entries_.isNull(); }
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }
    const void* identity() const noexcept { return entries_.identity(); }
    auto keys() const { return entries_.keys(); }
    iterator begin() { return iterator(entries_.begin()); }
    iterator end() { return iterator(entries_.end()); }
    const_iterator begin() const { auto snapshot = entries_; return const_iterator(snapshot.begin()); }
    const_iterator end() const { auto snapshot = entries_; return const_iterator(snapshot.end()); }
    iterator find(const Key& key) { return iterator(entries_.find(key)); }
    const_iterator find(const Key& key) const {
        auto snapshot = entries_;
        return const_iterator(snapshot.find(key));
    }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    iterator find(const K& key) { return find(Key::fromBorrowedView(std::string_view(key))); }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    const_iterator find(const K& key) const { return find(Key::fromBorrowedView(std::string_view(key))); }
    Animation get(const Key& key) const { return entries_.at(key); }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    Animation get(const K& key) const { return get(Key::fromBorrowedView(std::string_view(key))); }
    Animation& operator[](Key key) {
        auto destination = entries_[key];
        return static_cast<Animation&>(destination);
    }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    Animation& operator[](const K& key) {
        const auto view = std::string_view(key);
        if (const auto found = find(view); found != end()) return found->second;
        return (*this)[Key(view)];
    }
    std::size_t erase(const Key& key) { return entries_.erase(key); }
    template<class K> requires std::convertible_to<const K&, std::string_view>
    std::size_t erase(const K& key) { return erase(Key::fromBorrowedView(std::string_view(key))); }
    iterator erase(iterator entry) {
        const auto key = entry->first;
        auto next = entry;
        ++next;
        std::optional<Key> nextKey;
        if (next != end()) nextKey = next->first;
        entries_.erase(key);
        return nextKey ? find(*nextKey) : end();
    }
    // The alias owns the actual canonical value cell, not the whole AA table.
    std::shared_ptr<Animation> retain(iterator entry) const { return entry.position_.retainValue(); }
    void clear() { entries_.clear(); }
    void reset() { entries_ = Storage{}; }
    void initialize() { entries_.initialize(); }
    AnimationMap duplicate() const {
        AnimationMap result;
        const auto snapshot = entries_;
        for (const auto& entry : snapshot) result[entry.first] = entry.second;
        return result;
    }
};
}

namespace nicxlive::core::serde {
// fghj.serialization.deserializeValue(T[string]): commit after successful
// value decoding, with an independent immutable key copy at every insertion.
inline ReadError deserializeValue(TypedJsonView data, AnimationMap& value) {
    if (data.kind() == JsonKind::Null) { value.reset(); return {}; }
    if (data.kind() != JsonKind::Object) return unexpectedJsonKind(data);
    for (const auto& [name, input] : data.byKeyValue()) {
        Animation item;
        if (auto error = item.deserializeFromFghj(input)) return error;
        // The selected druntime idup returns null for a zero-length array.
        const auto key = name.empty() ? d_runtime::RetainedString{}
            : d_runtime::RetainedString(name.view());
        value[key] = item;
    }
    return {};
}
}
