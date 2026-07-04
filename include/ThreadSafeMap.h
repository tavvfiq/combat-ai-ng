#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace CombatAI
{
    // Thread-safe wrapper for std::unordered_map
    // Uses std::shared_mutex for read-write locking (read-heavy operations)
    template <typename Key, typename Value> class ThreadSafeMap
    {
      public:
        ThreadSafeMap() = default;
        ~ThreadSafeMap() = default;

        // Non-copyable, non-movable (for safety)
        ThreadSafeMap(const ThreadSafeMap &) = delete;
        ThreadSafeMap &operator=(const ThreadSafeMap &) = delete;
        ThreadSafeMap(ThreadSafeMap &&) = delete;
        ThreadSafeMap &operator=(ThreadSafeMap &&) = delete;

        // Thread-safe find - returns optional value
        std::optional<Value> Find(const Key &a_key) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_map.find(a_key);
            if (it != m_map.end()) {
                return std::optional<Value>(std::in_place, it->second);
            }
            return std::nullopt;
        }

        // Thread-safe check if key exists
        bool Contains(const Key &a_key) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_map.find(a_key) != m_map.end();
        }

        // Thread-safe insert/emplace - returns pair<success, value reference>
        template <typename... Args> std::pair<bool, Value *> Emplace(const Key &a_key, Args &&...args)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto [it, inserted] = m_map.emplace(a_key, std::forward<Args>(args)...);
            return {inserted, &it->second};
        }

        // Thread-safe insert
        std::pair<bool, Value *> Insert(const Key &a_key, const Value &a_value)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto [it, inserted] = m_map.insert({a_key, a_value});
            return {inserted, &it->second};
        }

        // Thread-safe erase
        size_t Erase(const Key &a_key)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            return m_map.erase(a_key);
        }

        // Thread-safe clear
        void Clear()
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_map.clear();
        }

        // Thread-safe size
        size_t Size() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_map.size();
        }

        // Thread-safe empty check
        bool Empty() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_map.empty();
        }

        // Thread-safe access with lock (for iteration)
        // Use this for safe iteration - lock is held during entire callback
        template <typename Func> void WithReadLock(Func &&a_func) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            a_func(m_map);
        }

        template <typename Func> void WithWriteLock(Func &&a_func)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            a_func(m_map);
        }

        // Thread-safe get value with default
        Value GetOrDefault(const Key &a_key, const Value &a_default) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_map.find(a_key);
            if (it != m_map.end()) {
                return it->second;
            }
            return a_default;
        }

        // Thread-safe read-modify-write.
        // Invokes a_func with a mutable reference to the value for a_key while the write
        // lock is held, so the entire read-modify-write is atomic. Returns true if the
        // key existed (and a_func was invoked), false otherwise.
        //
        // IMPORTANT: Do NOT call back into this same map from within a_func. The mutex is
        // not recursive, so re-entering would deadlock. Prefer this over the old
        // pointer-returning accessors: a raw pointer into the map is only valid while the
        // lock is held, and mutating through it after the lock was released is a data race
        // (and a use-after-free if another thread erases the entry).
        template <typename Func> bool Modify(const Key &a_key, Func &&a_func)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_map.find(a_key);
            if (it == m_map.end()) {
                return false;
            }
            a_func(it->second);
            return true;
        }

        // Thread-safe get-or-create + modify.
        // Default-constructs the value for a_key if it does not already exist, then invokes
        // a_func with a mutable reference while the write lock is held. Same non-reentrancy
        // rule as Modify().
        template <typename Func, typename = std::enable_if_t<std::is_default_constructible_v<Value>>>
        void ModifyOrCreate(const Key &a_key, Func &&a_func)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_map.try_emplace(a_key).first;
            a_func(it->second);
        }

      private:
        mutable std::shared_mutex m_mutex;
        std::unordered_map<Key, Value> m_map;
    };

    // Thread-safe wrapper for std::unordered_set
    template <typename Key> class ThreadSafeSet
    {
      public:
        ThreadSafeSet() = default;
        ~ThreadSafeSet() = default;

        // Non-copyable, non-movable (for safety)
        ThreadSafeSet(const ThreadSafeSet &) = delete;
        ThreadSafeSet &operator=(const ThreadSafeSet &) = delete;
        ThreadSafeSet(ThreadSafeSet &&) = delete;
        ThreadSafeSet &operator=(ThreadSafeSet &&) = delete;

        // Thread-safe insert - returns true if inserted, false if already exists
        bool Insert(const Key &a_key)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            return m_set.insert(a_key).second;
        }

        // Thread-safe erase
        size_t Erase(const Key &a_key)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            return m_set.erase(a_key);
        }

        // Thread-safe contains
        bool Contains(const Key &a_key) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_set.find(a_key) != m_set.end();
        }

        // Thread-safe clear
        void Clear()
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_set.clear();
        }

        // Thread-safe size
        size_t Size() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_set.size();
        }

        // Thread-safe empty check
        bool Empty() const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_set.empty();
        }

        // Thread-safe iteration
        template <typename Func> void WithReadLock(Func &&a_func) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            a_func(m_set);
        }

        template <typename Func> void WithWriteLock(Func &&a_func)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            a_func(m_set);
        }

      private:
        mutable std::shared_mutex m_mutex;
        std::unordered_set<Key> m_set;
    };
} // namespace CombatAI
