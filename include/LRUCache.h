#pragma once

#include "SearchTypes.h"
#include <string>
#include <optional>
#include <unordered_map>
#include <list>

#include <mutex>

class LRUCache {
public:
    explicit LRUCache(std::size_t capacity = 128);

    // nullopt = cache miss
    std::optional<AutocompleteResult> get(const std::string& key);

    // Chèn / cập nhật; tự đẩy phần tử cũ nhất ra nếu đầy
    void put(const std::string& key, const AutocompleteResult& value);

    // Xóa toàn bộ cache — gọi sau mỗi insert / remove
    void invalidate();

    std::size_t size() const;

private:
    using Entry = std::pair<std::string, AutocompleteResult>;

    std::size_t              capacity_;
    std::list<Entry>         list_;  
    std::unordered_map<std::string,
        std::list<Entry>::iterator> map_;
    mutable std::mutex       mutex_;
};
