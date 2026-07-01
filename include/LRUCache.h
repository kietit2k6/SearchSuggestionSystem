#pragma once

#include "SearchTypes.h"
#include <string>
#include <optional>
#include <unordered_map>
#include <list>
using namespace std;
class LRUCache {
public:
    explicit LRUCache(size_t capacity = 128);

    // nullopt = cache miss
    optional<AutocompleteResult> get(const string& key);

    // Chèn / cập nhật; tự đẩy phần tử cũ nhất ra nếu đầy
    void put(const string& key, const AutocompleteResult& value);

    // Xóa toàn bộ cache — gọi sau mỗi insert / remove
    void invalidate();

    size_t size() const { return map_.size(); }

private:
    using Entry = pair<string, AutocompleteResult>;

    size_t              capacity_;
    list<Entry>         list_;  
    unordered_map<string,
        list<Entry>::iterator> map_;
};
