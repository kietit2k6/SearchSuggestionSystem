#include "TrieNode.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <unordered_map>
#include <list>

#include "SearchTypes.h"
#include "LRUCache.h"
using namespace std;
class Trie
{
public:
  explicit Trie(size_t cacheCapacity = 128);
  ~Trie() = default;

  // Thao tác cơ bản
  bool insert(const string &word);
  bool remove(const string &word);
  bool search(const string &word) const;
  bool startsWith(const string &prefix) const;

  // Batch insert
  int insertBatch(const vector<string> &words); //

  // Dùng LRU cache: gọi lặp với cùng prefix
  AutocompleteResult autocomplete(const string &prefix, int maxResults = 10) const; //

  vector<FuzzyResult> fuzzySearch(const string &word,
                                       int maxErrors = 1) const;

  // Longest Common Prefix
  string longestCommonPrefix() const;
  static string longestCommonPrefixDivideConquer(
      const vector<string> &words);

  //  Thống kê
  int getWordCount() const;
  int getNodeCount() const;

  // duyệt trie khi được gọi, không cache riêng
  vector<string> getAllWords() const;

  // Quản lý frequency
  void incrementFrequency(const string &word);
  int getFrequency(const string &word) const;

  //  Lưu cả cấu trúc cây lẫn frequency để không mất dữ liệu
  //  khi restart ứng dụng.
  bool saveToDisk(const string &filename) const;
  bool loadFromDisk(const string &filename);

private:
  unique_ptr<TrieNode> root_;

  // SWMR: shared_lock cho const methods, unique_lock cho write
  mutable shared_mutex mutex_;
  mutable LRUCache cache_;

  int wordCount_ = 0;
  int nodeCount_ = 1;

  //  Internal helpers

  // Observer pointer
  const TrieNode *findNode(const string &prefix) const;

  // Min-heap
  void collectTopK(const TrieNode *node, 
            const string &current, 
            vector<WordEntry> &results, 
            int maxResults) const; //

  void fuzzySearchHelper(const TrieNode *node,
                         char ch,
                         const string &target,
                         string &current,
                         vector<int> &prevRow,
                         vector<FuzzyResult> &results,
                         int maxErrors) const;

  bool deleteHelper(TrieNode *node, const string &word, int depth);

  // LCP divide & conquer
  static string lcpHelper(const vector<string> &words,
                               int left, int right);
  static string commonPrefix(const string &a,
                                  const string &b);
  // chuyển hoá chuỗi
  static string normalize(const string &s);
};