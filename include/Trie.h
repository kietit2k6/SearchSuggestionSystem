

#include "TrieNode.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>          
#include <shared_mutex>    
#include <optional>        
#include <unordered_map>
#include <list>


#include "SearchTypes.h"
#include "LRUCache.h"

class Trie {
public:
    explicit Trie(std::size_t cacheCapacity = 128);
    ~Trie() = default;  

    // Thao tác cơ bản
    bool insert(const std::string& word);
    bool remove(const std::string& word);
    bool search(const std::string& word) const;
    bool startsWith(const std::string& prefix) const;

    // Batch insert 
    int insertBatch(const std::vector<std::string>& words);

    //Dùng LRU cache: gọi lặp với cùng prefix 
    AutocompleteResult autocomplete(const std::string& prefix,
                                    int maxResults = 10) const;

    std::vector<FuzzyResult> fuzzySearch(const std::string& word,
                                         int maxErrors = 1) const;

    // Longest Common Prefix
    std::string longestCommonPrefix() const;
    static std::string longestCommonPrefixDivideConquer(
            const std::vector<std::string>& words);

    //  Thống kê 
    int getWordCount() const;
    int getNodeCount() const;

    //duyệt trie khi được gọi, không cache riêng
    std::vector<std::string> getAllWords() const;

    //Quản lý frequency 
    void incrementFrequency(const std::string& word);
    int  getFrequency(const std::string& word) const;

    //  Lưu cả cấu trúc cây lẫn frequency để không mất dữ liệu
    //  khi restart ứng dụng.
    bool saveToDisk(const std::string& filename) const;
    bool loadFromDisk(const std::string& filename);

private:
    std::unique_ptr<TrieNode> root_;

    // SWMR: shared_lock cho const methods, unique_lock cho write
    mutable std::shared_mutex mutex_;
    mutable LRUCache          cache_;

    int wordCount_ = 0;
    int nodeCount_ = 1;   

    //  Internal helpers 

    // Observer pointer 
    const TrieNode* findNode(const std::string& prefix) const;

    // Min-heap
    void collectTopK(const TrieNode*       node,
                     const std::string&    current,
                     std::vector<WordEntry>& results,
                     int                   maxResults) const;

    void fuzzySearchHelper(const TrieNode*           node,
                           char                      ch,
                           const std::string&        target,
                           std::string&              current,
                           std::vector<int>&         prevRow,
                           std::vector<FuzzyResult>& results,
                           int                       maxErrors) const;

    bool deleteHelper(TrieNode* node, const std::string& word, int depth);

    // LCP divide & conquer
    static std::string lcpHelper(const std::vector<std::string>& words,
                                  int left, int right);
    static std::string commonPrefix(const std::string& a,
                                    const std::string& b);
    //chuyển hoá chuỗi
    static std::string normalize(const std::string& s);
};