#include "../include/LRUCache.h"

// Constructor
LRUCache::LRUCache(std::size_t capacity)
    : capacity_(capacity)
{
}

// Lấy dữ liệu từ cache
std::optional<AutocompleteResult> LRUCache::get(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);

    // Không tìm thấy
    if (it == map_.end())
        return std::nullopt;

    // Đưa phần tử vừa truy cập lên đầu danh sách
    list_.splice(list_.begin(), list_, it->second);

    return it->second->second;
}

// Thêm hoặc cập nhật dữ liệu
void LRUCache::put(const std::string& key,
                   const AutocompleteResult& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);

    // Nếu khóa đã tồn tại
    if (it != map_.end())
    {
        it->second->second = value;

        // Đưa lên đầu danh sách
        list_.splice(list_.begin(), list_, it->second);

        return;
    }

    // Nếu cache đã đầy
    if (list_.size() >= capacity_)
    {
        // Lấy phần tử ít được sử dụng gần đây nhất
        const auto& last = list_.back();

        map_.erase(last.first);
        list_.pop_back();
    }

    // Thêm phần tử mới vào đầu
    list_.push_front({key, value});
    map_[key] = list_.begin();
}

// Xóa toàn bộ cache
void LRUCache::invalidate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    list_.clear();
    map_.clear();
}

// Lấy kích thước hiện tại của cache
std::size_t LRUCache::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return map_.size();
}
