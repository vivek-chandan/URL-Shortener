#pragma once

#include <unordered_map>
#include <list>
#include <mutex>

/**
 * @brief LRU Cache implementation using unordered_map + doubly linked list
 * @tparam K Key type
 * @tparam V Value type
 * 
 * Thread-safe LRU Cache with get/put operations and capacity management.
 */
template<typename K, typename V>
class LRUCache {
private:
    struct Node {
        K key;
        V value;
        Node(K k, V v) : key(k), value(v) {}
    };

    // Capacity of the cache
    size_t capacity_;
    
    // Map of key -> iterator to list node for O(1) access
    std::unordered_map<K, typename std::list<Node>::iterator> cache_map_;
    
    // Doubly linked list to maintain LRU order (most recent at back)
    std::list<Node> cache_list_;
    
    // Thread safety
    mutable std::mutex lock_;

public:
    /**
     * @brief Constructor
     * @param capacity Maximum number of items in cache
     */
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    /**
     * @brief Get value from cache
     * @param key The key to look up
     * @param value Reference to store the value
     * @return true if key found, false otherwise
     * 
     * Moves the accessed item to the end (most recent position).
     */
    bool get(const K& key, V& value) {
        std::lock_guard<std::mutex> guard(lock_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        
        // Move to end (mark as most recent)
        cache_list_.splice(cache_list_.end(), cache_list_, it->second);
        value = it->second->value;
        
        return true;
    }

    /**
     * @brief Put key-value pair into cache
     * @param key The key
     * @param value The value
     * 
     * If key exists, updates value and moves to end.
     * If cache is full, removes least recently used item.
     */
    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> guard(lock_);
        
        auto it = cache_map_.find(key);
        
        if (it != cache_map_.end()) {
            // Key exists, update value and move to end
            it->second->value = value;
            cache_list_.splice(cache_list_.end(), cache_list_, it->second);
            return;
        }
        
        // New key - check capacity
        if (cache_map_.size() >= capacity_) {
            // Remove least recently used (front of list)
            auto lru_key = cache_list_.front().key;
            cache_list_.pop_front();
            cache_map_.erase(lru_key);
        }
        
        // Add new item at end (most recent)
        cache_list_.emplace_back(key, value);
        cache_map_[key] = std::prev(cache_list_.end());
    }

    /**
     * @brief Clear all items from cache
     */
    void clear() {
        std::lock_guard<std::mutex> guard(lock_);
        cache_list_.clear();
        cache_map_.clear();
    }

    /**
     * @brief Get current size of cache
     * @return Number of items currently in cache
     */
    size_t size() const {
        std::lock_guard<std::mutex> guard(lock_);
        return cache_map_.size();
    }

    /**
     * @brief Check if key exists in cache
     * @param key The key to check
     * @return true if key exists, false otherwise
     */
    bool exists(const K& key) const {
        std::lock_guard<std::mutex> guard(lock_);
        return cache_map_.find(key) != cache_map_.end();
    }

    /**
     * @brief Remove specific key from cache
     * @param key The key to remove
     */
    void remove(const K& key) {
        std::lock_guard<std::mutex> guard(lock_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        }
    }
};
