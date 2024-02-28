#ifndef STORAGE_INCLUDE_BUFFER_LRU_CACHE_H
#define STORAGE_INCLUDE_BUFFER_LRU_CACHE_H

#include "error.h"
#include "noncopyable.h"
#include "tl/expected.hpp"
#include <cstdint>
#include <list>
#include <unordered_map>
#include <utility>

namespace storage {
/**
 * Replacer is an abstract class that tracks page usage.
 */
// template <typename Key, typename Value> class Cache {
// public:
//     Cache() = default;
//     virtual ~Cache() = default;
//
//     Value &get(const Key &key) const = 0;
//     bool put(const Key &key, const Value &value) = 0;
//     virtual bool &victim(const Key &key) = 0;
//
//     // Pins a entry, indicating that it should not be victimized until it is
//     // unpinned.
//     virtual void pin(const Key &key) = 0;
//
//     // Unpins a entry, indicating that it can now be victimized.
//     virtual void unpin(const Key &key) = 0;
// };

/**
 * LRUReplacer implements the Least Recently Used replacement policy.
 * FIXME: Cache interface?; thread safe methods
 */
template <typename Key, typename Value> /*  FIXME: key comparator */
class LRUCache : public NonCopyable {
public:
    /**
     * Create a new LRUCache.
     * @param the maximum number of entries the LRUCache will be
     * required to store
     */
    explicit LRUCache(const uint32_t max_size)
        : max_size_(max_size), cur_size_(0) {}

    /**
     * Destroys the LRUReplacer.
     */
    ~LRUCache() { clear(); }

    // if found, @param value assigned to the found value and return true;
    // else, return false.
    bool get(const Key &key, Value &value) const {
        auto it = map_.find(key);
        if (it == map_.end())
            return false;

        value = it->second->second;
        touch(key);
        return true;
    }

    // if exists, touch and update the content.
    // if not exists, ensure enough space and insert the entry.
    // if there is no enough space, just return false.
    bool put(const Key &key, const Value &value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            touch(key);
            return true;
        }

        if (is_full()) {
            bool ok = victim();
            if (!ok)
                return false;
        }

        list_.emplace_front(key, value);
        // NOTE: map insert require @std::pair(key, value)
        map_.insert(std::make_pair(key, list_.begin()));
        cur_size_++;
        return true;
    }

    // if exists, remove the entry and return true;
    // else, return false.
    bool remove(const Key &key) {
        auto it = map_.find(key);
        if (it != map_.end())
            return false;

        list_.erase(*it->second);
        map_.erase(it);

        cur_size_--;
    }

    bool exists(const Key &key) const { return map_.find(key) != map_.end(); }

    // void pin(const Key &key);
    //
    // void unpin(const Key &key);

    uint32_t size() const { return cur_size_; }
    uint32_t max_size() const { return max_size_; }

    bool is_empty() const { return size() == 0; }
    bool is_full() const { return size() == max_size(); }

private:
    // Remove the victim entry as defined by the replacement policy.
    // return true if success; else return false.
    virtual bool victim() {
        auto last_entry = list_.end() - 1;
        auto it = map_.find(last_entry->first);
        map_.erase(it);
        list_.pop_front();

        cur_size_--;
        return true;
    }

    // "use"/touch the entry.
    virtual void touch(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return;
        list_.splice(list_.begin(), list_, it->second);
    }

    void clear() {
        list_.clear();
        map_.clear();
    }

    using Entry = std::pair<Key, Value>;
    using CacheList = std::list<Entry>;
    using CacheMap = std::unordered_map<Key, typename CacheList::iterator>;

    // struct ListNode {
    //     Key key;
    //     Value value;
    //     ListNode *prev, *next;
    //     ListNode(const Key &key, const Value &value)
    //         : key(key), value(value), prev(nullptr), next(nullptr) {}
    // };
    //
    // ListNode *head_, *tail_;

    CacheList list_;
    CacheMap map_;
    uint32_t max_size_;
    uint32_t cur_size_;
};

template <typename Key, typename Value>
class LRUCacheWithPin : public NonCopyable {
public:
    /**
     * Create a new LRUCache.
     * @param the maximum number of entries the LRUCache will be
     * required to store
     */
    explicit LRUCacheWithPin(const uint32_t max_size)
        : max_size_(max_size), cur_size_(0) {}

    /**
     * Destroys the LRUReplacer.
     */
    ~LRUCacheWithPin() { clear(); }

    // if found, @param value assigned to the found value and return true;
    // else, return CacheEntryNotFound error.
    ErrorCode get(const Key &key, Value &value) {
        auto it = map_.find(key);
        if (it == map_.end())
            return ErrorCode::CacheEntryNotFound;

        value = it->second->value;
        touch(key);

        return ErrorCode::Success;
    }

    // if exists, touch and update the content.
    // if not exists, ensure enough space and insert the entry.
    // if there is no enough space, return CacheNoMoreVictim error.
    ErrorCode put(const Key &key, const Value &value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->value = value;
            touch(key);
            return ErrorCode::Success;
        }

        if (is_full()) {
            auto result = victim();
            if (!result)
                return result.error();
        }

        list_.emplace_front(key, value);
        map_.insert(std::make_pair(key, list_.begin()));
        cur_size_++;
        return ErrorCode::Success;
    }

    // if exists, remove the entry and return true;
    // else, return CacheKeyNotFound error.
    ErrorCode remove(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return ErrorCode::CacheEntryNotFound;

        list_.erase(it->second);
        map_.erase(it);

        cur_size_--;

        return ErrorCode::Success;
    }

    bool exists(const Key &key) const { return map_.find(key) != map_.end(); }

    // if key not exists or not pinned, return false;
    // else, return true;
    bool is_pinned(const Key &key) const {
        auto it = map_.find(key);
        if (it == map_.end() || !it->second->is_pinned())
            return false;

        return true;
    }

    // if not found, return KeyNotFound error.
    ErrorCode pin(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return ErrorCode::KeyNotFound;
        }
        it->second->pin_count++;
        return ErrorCode::Success;
    }

    // if not found, return KeyNotFound error; if not pinned, return
    // KeyNotPinned error.
    ErrorCode unpin(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return ErrorCode::KeyNotFound;
        if (!it->second->is_pinned())
            return ErrorCode::KeyNotPinned;

        it->second->pin_count--;
        return ErrorCode::Success;
    }

    uint32_t size() const { return cur_size_; }
    uint32_t max_size() const { return max_size_; }

    bool is_empty() const { return size() == 0; }
    bool is_full() const { return size() == max_size(); }

    // Remove the victim entry as defined by the replacement policy.
    // return victim's value if success; else return CacheNoMoreVictim error.
    tl::expected<Value, ErrorCode> victim() {
        auto last_entry = list_.rbegin();
        while (last_entry != list_.rend()) {
            if (last_entry->is_pinned()) {
                last_entry--;
                continue;
            }
            auto it = map_.find(last_entry->key);
            Value value = last_entry->value;
            // NOTE: list::erase require @const_iterator(forward), use
            // reverse_iterator::base() to get the forward iterator
            list_.erase(last_entry.base());
            map_.erase(it);

            cur_size_--;
            return value;
        }
        return tl::unexpected(ErrorCode::CacheNoMoreVictim);
    }

private:
    // "use"/touch the entry.
    void touch(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return;
        list_.splice(list_.begin(), list_, it->second);
    }

    void clear() {
        list_.clear();
        map_.clear();
    }

    struct EntryWithPin {
        Key key;
        Value value;
        int pin_count;

        bool is_pinned() const { return pin_count > 0; }
        explicit EntryWithPin(const Key &key, const Value &value)
            : key(key), value(value), pin_count(0) {}
    };

    using CacheList = std::list<EntryWithPin>;
    using CacheMap = std::unordered_map<Key, typename CacheList::iterator>;

    CacheList list_;
    CacheMap map_;
    uint32_t max_size_;
    uint32_t cur_size_;
};
} // namespace storage

#endif // !STORAGE_INCLUDE_BUFFER_LRU_CACHE_H
