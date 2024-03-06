#ifndef STORAGE_INCLUDE_BUFFER_LRU_CACHE_H
#define STORAGE_INCLUDE_BUFFER_LRU_CACHE_H

#include "error.h"
#include "log.h"
#include "noncopyable.h"
#include "tl/expected.hpp"
#include <cstdint>
#include <iterator>
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
// FIXME: no unit test
// template <typename Key, typename Value> /*  FIXME: key comparator */
// class LRUCache : public NonCopyable {
// public:
//     /**
//      * Create a new LRUCache.
//      * @param the maximum number of entries the LRUCache will be
//      * required to store
//      */
//     explicit LRUCache(const uint32_t max_size)
//         : max_size_(max_size), cur_size_(0) {}
//
//     /**
//      * Destroys the LRUReplacer.
//      */
//     ~LRUCache() { clear(); }
//
//     // if found, @param value assigned to the found value and return true;
//     // else, return false.
//     bool get(const Key &key, Value &value) const {
//         auto it = map_.find(key);
//         if (it == map_.end())
//             return false;
//
//         value = it->second->second;
//         touch(key);
//         return true;
//     }
//
//     // if exists, touch and update the content.
//     // if not exists, ensure enough space and insert the entry.
//     // if there is no enough space, just return false.
//     bool put(const Key &key, const Value &value) {
//         auto it = map_.find(key);
//         if (it != map_.end()) {
//             it->second->second = value;
//             touch(key);
//             return true;
//         }
//
//         if (is_full()) {
//             bool ok = victim();
//             if (!ok)
//                 return false;
//         }
//
//         list_.emplace_front(key, value);
//         // NOTE: map insert require @std::pair(key, value)
//         map_.insert(std::make_pair(key, list_.begin()));
//         cur_size_++;
//         return true;
//     }
//
//     // if exists, remove the entry and return true;
//     // else, return false.
//     bool remove(const Key &key) {
//         auto it = map_.find(key);
//         if (it != map_.end())
//             return false;
//
//         list_.erase(*it->second);
//         map_.erase(it);
//
//         cur_size_--;
//     }
//
//     bool exists(const Key &key) const { return map_.find(key) != map_.end();
//     }
//
//     // void pin(const Key &key);
//     //
//     // void unpin(const Key &key);
//
//     uint32_t size() const { return cur_size_; }
//     uint32_t max_size() const { return max_size_; }
//
//     bool is_empty() const { return size() == 0; }
//     bool is_full() const { return size() == max_size(); }
//
// private:
//     // Remove the victim entry as defined by the replacement policy.
//     // return true if success; else return false.
//     virtual bool victim() {
//         auto last_entry = list_.end() - 1;
//         auto it = map_.find(last_entry->first);
//         map_.erase(it);
//         list_.pop_front();
//
//         cur_size_--;
//         return true;
//     }
//
//     // "use"/touch the entry.
//     virtual void touch(const Key &key) {
//         auto it = map_.find(key);
//         if (it == map_.end())
//             return;
//         list_.splice(list_.begin(), list_, it->second);
//     }
//
//     void clear() {
//         list_.clear();
//         map_.clear();
//     }
//
//     using Entry = std::pair<Key, Value>;
//     using CacheList = std::list<Entry>;
//     using CacheMap = std::unordered_map<Key, typename CacheList::iterator>;
//
//     // struct ListNode {
//     //     Key key;
//     //     Value value;
//     //     ListNode *prev, *next;
//     //     ListNode(const Key &key, const Value &value)
//     //         : key(key), value(value), prev(nullptr), next(nullptr) {}
//     // };
//     //
//     // ListNode *head_, *tail_;
//
//     CacheList list_;
//     CacheMap map_;
//     uint32_t max_size_;
//     uint32_t cur_size_;
// };

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

        value = it->second->entry.value;
        touch(key);

        return ErrorCode::Success;
    }

    // if exists, touch and update the content.
    // if not exists, ensure enough space and insert the entry.
    // if there is no enough space, return CacheNoMoreVictim error.
    ErrorCode put(const Key &key, const Value &value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->entry.value = value;
            touch(key);
            return ErrorCode::Success;
        }

        if (is_full()) {
            auto result = victim();
            if (!result)
                return result.error();
        }

        auto node = list_.push_front(key, value);
        map_.insert({key, node});
        cur_size_++;
        return ErrorCode::Success;
    }

    // if exists, remove the entry and return true;
    // else, return CacheKeyNotFound error.
    ErrorCode remove(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return ErrorCode::CacheEntryNotFound;

        auto node = list_.remove(it->second);
        map_.erase(it);
        delete node;

        cur_size_--;

        return ErrorCode::Success;
    }

    bool exists(const Key &key) const { return map_.find(key) != map_.end(); }

    // if key not exists or not pinned, return false;
    // else, return true;
    bool is_pinned(const Key &key) const {
        auto it = map_.find(key);
        if (it == map_.end() || !it->second->entry->is_pinned())
            return false;

        return true;
    }

    // if not found, return KeyNotFound error.
    ErrorCode pin(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return ErrorCode::KeyNotFound;
        }
        it->second->entry.pin_count++;
        return ErrorCode::Success;
    }

    // if not found, return KeyNotFound error; if not pinned, return
    // KeyNotPinned error.
    ErrorCode unpin(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return ErrorCode::KeyNotFound;
        if (!it->second->entry.is_pinned())
            return ErrorCode::KeyNotPinned;

        it->second->entry.pin_count--;
        return ErrorCode::Success;
    }

    uint32_t size() const { return cur_size_; }
    uint32_t max_size() const { return max_size_; }

    bool is_empty() const { return size() == 0; }
    bool is_full() const { return size() == max_size(); }

    // Remove the victim entry as defined by the replacement policy.
    // return victim's value if success; else return CacheNoMoreVictim error.
    tl::expected<Value, ErrorCode> victim() {
        // auto last_entry = list_.rbegin();
        // while (last_entry != list_.rend()) {
        //     if (last_entry->is_pinned()) {
        //         last_entry++;
        //         continue;
        //     }
        //     auto it = map_.find(last_entry->key);
        //     assert(it != map_.end());
        //     assert(it->second->key == last_entry->key);
        //     assert(it->second->value == last_entry->value);
        //     auto value = last_entry->value;
        //     // assert(std::next(last_entry).base() == it->second);
        //     list_.erase(it->second);
        //     map_.erase(it);
        //
        //     cur_size_--;
        //     return value;
        // }
        if (!list_.empty()) {
            auto node = list_.pop_back();
            // std::cerr << node->entry.key << ": " << node->entry.value
            //           << std::endl;
            map_.erase(node->entry.key);
            cur_size_--;

            return node->entry.value;
        }
        return tl::unexpected(ErrorCode::CacheNoMoreVictim);
    }

private:
    // "use"/touch the entry.
    void touch(const Key &key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return;

        //  TODO: reduce copy
        list_.move_front(it->second);
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

        EntryWithPin() {}
    };

    struct ListNode {
        EntryWithPin entry;
        ListNode *prev, *next;

        ListNode(const Key &key, const Value &value)
            : entry(key, value), prev(nullptr), next(nullptr) {}

        ListNode() : prev(nullptr), next(nullptr) {}
    };

    struct List {
        using iterator = ListNode *;
        ListNode *head, *tail;

        List() : head(new ListNode()), tail(new ListNode()) {
            head->next = tail;
            tail->prev = head;
        }

        ListNode *push_front(const Key &key, const Value &value) {
            auto node = new ListNode(key, value);

            head->next->prev = node;

            node->next = head->next;
            node->prev = head;

            head->next = node;
            return node;
        }

        ListNode *push_back(const Key &key, const Value &value) {
            auto node = new ListNode(key, value);
            tail->prev->next = node;

            node->prev = tail->prev;
            node->next = tail;

            tail->prev = node;
        }

        ListNode *remove(ListNode *node) {
            node->prev->next = node->next;
            node->next->prev = node->prev;

            return node;
        }

        void move_front(ListNode *node) {
            node->prev->next = node->next;
            node->next->prev = node->prev;

            head->next->prev = node;

            node->prev = head;
            node->next = head->next;

            head->next = node;
        }

        ListNode *pop_back() {
            if (empty())
                return nullptr;

            auto node = tail->prev;
            return remove(node);
        }

        bool empty() { return tail->prev == head; }

        void clear() {
            ListNode *node = head;
            while (node != tail) {
                ListNode *next = node->next;
                delete node;
                node = next;
            }
        }
    };

    // using CacheList = std::list<EntryWithPin>;
    using CacheList = List;
    using CacheMap = std::unordered_map<Key, typename CacheList::iterator>;

    CacheList list_;
    CacheMap map_;
    uint32_t max_size_;
    uint32_t cur_size_;
};
} // namespace storage

#endif // !STORAGE_INCLUDE_BUFFER_LRU_CACHE_H
