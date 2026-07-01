#pragma once

#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
struct TrieNode {
    char ch;                                     
    unordered_map<char, TrieNode*> children; 
    bool isEndOfWord = false;                    
    int frequency = 0;                            
    string word;                             

    // Bộ nhớ đệm lưu sẵn Top từ khóa hot nhất đi qua nút này
    vector<pair<int, string>> topSuggestions;

    explicit TrieNode(char c = '\0') : ch(c) {}

    // Hàm hủy đệ quy tự động giải phóng các nút con
    ~TrieNode() {
        for (auto& [key, child] : children) {
            delete child;
        }
    }
};