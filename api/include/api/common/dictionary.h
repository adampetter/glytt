#pragma once

#include <unordered_map>
#include <string>

template <typename TKey, typename TValue>
class Dictionary
{
private:
    std::unordered_map<TKey, TValue> items;

public:
    Dictionary() {}
    ~Dictionary() {}

    void Add(const TKey &key, const TValue &value)
    {
        items[key] = value;
    }

    bool ContainsKey(const TKey &key)
    {
        return items.find(key) != items.end();
    }

    TValue &operator[](const TKey &key)
    {
        return items[key];
    }

    void Remove(const TKey &key)
    {
        items.erase(key);
    }

    int Count()
    {
        return items.size();
    }

    void Clear()
    {
        items.clear();
    }
};