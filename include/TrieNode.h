#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct TrieNode {
    char ch;                                     
    std::unordered_map<char, TrieNode*> children; 
    bool isEndOfWord = false;                    
    int frequency = 0;                            
    std::string word;                             

    // Bộ nhớ đệm lưu sẵn Top từ khóa hot nhất đi qua nút này
    std::vector<std::pair<int, std::string>> topSuggestions;

    explicit TrieNode(char c = '\0') : ch(c) {}

    // Hàm hủy đệ quy tự động giải phóng các nút con
    ~TrieNode() {
        for (auto& [key, child] : children) {
            delete child;
        }
    }
};