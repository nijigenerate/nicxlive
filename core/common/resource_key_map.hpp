#pragma once
// Normal Resource-key associative-array storage adapted from rt/aaA.d v2.111.0.
// Copyright Digital Mars 2000-2015; Boost Software License 1.0.
// Hash mixing, quadratic probing, growth, deletion and bucket enumeration follow
// that source. C++ owns real entries/handles; GC collection timing is not claimed.
#include "../compat/native_access_fault.hpp"
#include "../compat/range_error.hpp"
#include "../compat/system_error.hpp"
#include "../compat/source_allocator.hpp"
#include "shared_array.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nicxlive::core::common {
template<class K, class V, class Hash, class Equal> class ResourceKeyMap {
    using Entry = std::pair<const K, V>;
    template<class T> using Allocation = d_runtime::SourceAllocator<T>;
    static constexpr std::size_t filled = std::size_t{1} << (sizeof(std::size_t) * 8 - 1);
    struct Bucket {
        std::size_t hash = 0;
        std::shared_ptr<Entry> entry;
        bool isFilled() const { return (hash & filled) != 0; }
    };
    struct Storage {
        using Buckets = std::vector<Bucket, Allocation<Bucket>>;
        static Buckets allocateBuckets(std::size_t count) {
            try {
                return Buckets(count);
            } catch (const std::length_error&) {
                throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
            }
        }
        Buckets buckets = allocateBuckets(8);
        std::uint32_t used = 0, deleted = 0, firstUsed = 8;
        std::size_t length() const { return used - deleted; }
        std::size_t insertSlot(std::size_t hash) const {
            const auto mask = buckets.size() - 1;
            for (std::size_t i = hash & mask, step = 1;; ++step) {
                if (!buckets[i].isFilled()) return i;
                i = (i + step) & mask;
            }
        }
        std::size_t lookup(std::size_t hash, const K& key) const {
            const auto mask = buckets.size() - 1;
            for (std::size_t i = hash & mask, step = 1;; ++step) {
                // The pinned runtime calls equals(query, stored), only for an
                // identical mixed hash, with no pointer/null identity shortcut.
                const auto entry = buckets[i].entry;
                if (buckets[i].hash == hash && Equal{}(key, entry->first)) return i;
                if (buckets[i].hash == 0) return buckets.size();
                i = (i + step) & mask;
            }
        }
        void resize(std::size_t size) {
            // Keep the published buckets until replacement allocation succeeds.
            auto replacement = allocateBuckets(size);
            auto previous = std::move(buckets);
            buckets = std::move(replacement);
            for (std::size_t i = firstUsed; i < previous.size(); ++i) {
                if (!previous[i].isFilled()) continue;
                buckets[insertSlot(previous[i].hash)] = std::move(previous[i]);
            }
            firstUsed = 0;
            used -= deleted;
            deleted = 0;
        }
    };
    std::shared_ptr<Storage> storage_;
    static std::size_t mixedHash(const K& key) {
        auto hash = Hash{}(key);
        hash ^= hash >> 13;
        hash *= std::size_t{0x5bd1e995};
        hash ^= hash >> 15;
        return hash | filled;
    }
    std::shared_ptr<Storage> ensureStorage() {
        if (!storage_) storage_ = std::allocate_shared<Storage>(Allocation<Storage>{});
        return storage_;
    }
    static std::shared_ptr<Entry> getOrInsert(const std::shared_ptr<Storage>& storage, const K& key) {
        const auto hash = mixedHash(key);
        if (const auto found = storage->lookup(hash, key); found != storage->buckets.size())
            return storage->buckets[found].entry;
        auto index = storage->insertSlot(hash);
        if (storage->buckets[index].hash == 1) --storage->deleted;
        else if (++storage->used * 5 > storage->buckets.size() * 4) {
            const auto size = storage->length() * 8 < 4 * storage->buckets.size()
                ? storage->buckets.size() : 4 * storage->buckets.size();
            storage->resize(size);
            index = storage->insertSlot(hash);
        }
        storage->firstUsed = std::min(storage->firstUsed, static_cast<std::uint32_t>(index));
        auto& bucket = storage->buckets[index];
        bucket.hash = hash;
        bucket.entry = std::allocate_shared<Entry>(Allocation<Entry>{}, key, V{});
        return bucket.entry;
    }
public:
    class MappedReference {
        std::shared_ptr<Entry> entry_;
    public:
        explicit MappedReference(std::shared_ptr<Entry> entry) : entry_(std::move(entry)) {}
        operator V&() const { return entry_->second; }
        MappedReference& operator=(V value) { entry_->second = std::move(value); return *this; }
        MappedReference& operator=(const MappedReference& other) { return *this = V(other.entry_->second); }
        template<class... A> decltype(auto) append(A&&... arguments) {
            return entry_->second.append(std::forward<A>(arguments)...);
        }
        auto operator->() const requires requires(V& value) { value.operator->(); } {
            return entry_->second.operator->();
        }
        explicit operator bool() const requires requires(V& value) { bool(value); } {
            return bool(entry_->second);
        }
    };
    template<bool Const> class Iterator {
        friend class ResourceKeyMap;
        std::shared_ptr<Storage> storage_;
        std::size_t index_ = 0;
        mutable std::shared_ptr<Entry> retained_;
        Iterator(std::shared_ptr<Storage> storage, std::size_t index)
            : storage_(std::move(storage)), index_(index) { advance(); }
        void advance() {
            while (storage_ && index_ < storage_->buckets.size() && !storage_->buckets[index_].isFilled()) ++index_;
        }
        bool isEnd() const { return !storage_ || index_ >= storage_->buckets.size(); }
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Entry;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, const Entry&, Entry&>;
        using pointer = std::conditional_t<Const, const Entry*, Entry*>;
        Iterator() = default;
        reference operator*() const {
            retained_ = storage_->buckets[index_].entry;
            return *d_runtime::ngRequireObject(retained_);
        }
        pointer operator->() const { return std::addressof(operator*()); }
        auto retainValue() const {
            using Value = std::conditional_t<Const, const V, V>;
            if (!retained_ && isEnd()) return std::shared_ptr<Value>{};
            const auto entry = d_runtime::ngRequireObject(
                retained_ ? retained_ : storage_->buckets[index_].entry);
            return std::shared_ptr<Value>(entry, std::addressof(entry->second));
        }
        Iterator& operator++() { retained_.reset(); ++index_; advance(); return *this; }
        Iterator operator++(int) { auto previous = *this; ++*this; return previous; }
        friend bool operator==(const Iterator& left, const Iterator& right) {
            if (left.isEnd() || right.isEnd()) return left.isEnd() && right.isEnd();
            return left.storage_ == right.storage_ && left.index_ == right.index_;
        }
    };
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;
    ResourceKeyMap() = default;
    ResourceKeyMap(const ResourceKeyMap&) = default;
    ResourceKeyMap& operator=(const ResourceKeyMap&) = default;
    ResourceKeyMap(ResourceKeyMap&& other) noexcept : storage_(other.storage_) {}
    ResourceKeyMap& operator=(ResourceKeyMap&& other) noexcept { storage_ = other.storage_; return *this; }
    void initialize() { (void)ensureStorage(); }
    bool isNull() const noexcept { return !storage_; }
    const void* identity() const noexcept { return storage_.get(); }
    std::size_t size() const noexcept { return storage_ ? storage_->length() : 0; }
    bool empty() const noexcept { return size() == 0; }
    iterator begin() { return {storage_, storage_ ? storage_->firstUsed : 0}; }
    iterator end() { return {storage_, storage_ ? storage_->buckets.size() : 0}; }
    const_iterator begin() const { return {storage_, storage_ ? storage_->firstUsed : 0}; }
    const_iterator end() const { return {storage_, storage_ ? storage_->buckets.size() : 0}; }
    iterator find(const K& key) {
        const auto storage = storage_;
        if (!storage || !storage->length()) return {};
        return {storage, storage->lookup(mixedHash(key), key)};
    }
    const_iterator find(const K& key) const {
        const auto storage = storage_;
        if (!storage || !storage->length()) return {};
        return {storage, storage->lookup(mixedHash(key), key)};
    }
    bool contains(const K& key) const { return find(key) != end(); }
    std::shared_ptr<const V> findRetained(const K& key) const {
        const auto storage = storage_;
        if (!storage || !storage->length()) return {};
        const auto index = storage->lookup(mixedHash(key), key);
        if (index == storage->buckets.size()) return {};
        const auto entry = d_runtime::ngRequireObject(storage->buckets[index].entry);
        return std::shared_ptr<const V>(entry, std::addressof(entry->second));
    }
    // Materialize the real descriptor values in the canonical bucket order.
    SharedArray<K> keys() const {
        const auto snapshot = storage_;
        if (!snapshot || !snapshot->length()) return {};
        SharedArray<K> result(snapshot->length(), K{});
        std::size_t index = 0;
        const auto last = const_iterator(snapshot, snapshot->buckets.size());
        for (auto cursor = const_iterator(snapshot, snapshot->firstUsed); cursor != last; ++cursor)
            result[index++] = cursor->first;
        return result;
    }
    SharedArray<V> values() const {
        const auto snapshot = storage_;
        if (!snapshot || !snapshot->length()) return {};
        SharedArray<V> result(snapshot->length(), V{});
        std::size_t index = 0;
        const auto last = const_iterator(snapshot, snapshot->buckets.size());
        for (auto cursor = const_iterator(snapshot, snapshot->firstUsed); cursor != last; ++cursor)
            result[index++] = cursor->second;
        return result;
    }
    MappedReference operator[](const K& key) {
        const auto storage = ensureStorage();
        return MappedReference(getOrInsert(storage, key));
    }
    V at(const K& key) const {
        const auto found = find(key);
        if (found == end()) throw d_runtime::ngStaticError<d_runtime::RangeError>(
            std::source_location::current());
        // Current source read contexts copy the class/slice descriptor. Writes
        // use operator[]'s retained actual value-cell reference instead.
        return found->second;
    }
    void clear() {
        const auto storage = storage_;
        if (!storage || !storage->length()) return;
        for (std::size_t i = storage->firstUsed; i < storage->buckets.size(); ++i) storage->buckets[i] = {};
        storage->used = storage->deleted = 0;
        storage->firstUsed = static_cast<std::uint32_t>(storage->buckets.size());
    }
    std::size_t erase(const K& key) {
        const auto storage = storage_;
        if (!storage || !storage->length()) return 0;
        const auto index = storage->lookup(mixedHash(key), key);
        if (index == storage->buckets.size()) return 0;
        auto retained = std::move(storage->buckets[index].entry);
        storage->buckets[index].hash = 1;
        ++storage->deleted;
        if (storage->length() * 8 < storage->buckets.size() && storage->buckets.size() > 8)
            storage->resize(storage->buckets.size() / 4);
        return 1;
    }
};
// A policy supplies only the actual key protocol. All storage/probing/growth/
// deletion/enumeration and retained Entry/MappedReference behavior stay above.
template<class K,class V,class Policy>
using PolicyKeyMap=ResourceKeyMap<K,V,typename Policy::Hash,typename Policy::Equal>;
}
