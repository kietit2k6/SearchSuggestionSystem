#pragma once

#include "TrieNode.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "LRUCache.h"
#include "SearchTypes.h"

class Trie {
public:
    explicit Trie(std::size_t cacheCapacity = 128);
    ~Trie() = default;

    // ── Basic operations ──────────────────────────────────────────────────
    bool insert(const std::string& word);
    bool remove(const std::string& word);
    bool search(const std::string& word) const;
    bool startsWith(const std::string& prefix) const;

    // ── Batch insert (deferred rebuild for faster bulk loading) ───────────
    int  insertBatch(const std::vector<std::string>& words);
    // Insert without rebuilding suggestions — call rebuildAll() afterwards.
    int  insertBatchNoRebuild(const std::vector<std::string>& words);
    void rebuildAll();

    // ── Autocomplete (O(1) via pre-cached topSuggestions + LRU cache) ─────
    AutocompleteResult autocomplete(const std::string& prefix,
                                    int maxResults = 10) const;

    // ── Fuzzy search fallback ─────────────────────────────────────────────
    std::vector<FuzzyResult> fuzzySearch(const std::string& word,
                                         int maxErrors = 1) const;

    // ── Frequency management (anti-spam: rate-limited via timestamp) ───────
    // Returns false if rate-limited (< kMinFreqIntervalSecs since last call).
    bool incrementFrequency(const std::string& word, int64_t timestamp = 0);
    int  getFrequency(const std::string& word) const;

    // ── Combined search+increment (1 lock instead of 3 separate calls) ────
    struct SearchResult { bool found; bool incremented; int freqBefore; };
    SearchResult searchAndUpdate(const std::string& word, int64_t timestamp = 0);

    // ── Statistics ────────────────────────────────────────────────────────
    int getWordCount() const;
    int getNodeCount() const;
    std::vector<std::string> getAllWords() const;

    // ── Top-N most-searched words (for dashboard) ─────────────────────────
    std::vector<std::pair<std::string, int>> getTopSearched(int n) const;

    // ── Persistence ───────────────────────────────────────────────────────
    bool saveToDisk(const std::string& filename) const;
    bool saveToDiskAtomic(const std::string& filename) const;  // write→bak→rename
    bool loadFromDisk(const std::string& filename);

    // ── LCP helpers ───────────────────────────────────────────────────────
    std::string longestCommonPrefix() const;
    static std::string longestCommonPrefixDivideConquer(
            const std::vector<std::string>& words);

    // ── Anti-spam tunable ─────────────────────────────────────────────────
    static constexpr int kMinFreqIntervalSecs = 3;

private:
    // Stored suggestions per node (serves autocomplete(prefix, N≤K)).
    static constexpr int kMaxStoredSuggestions = 50;

    std::unique_ptr<TrieNode> root_;

    // SWMR: shared_lock for const methods, unique_lock for writes.
    // cache_ is thread-safe via its own internal mutex; declared mutable so
    // autocomplete() (const) can populate it.
    mutable std::shared_mutex mutex_;
    mutable LRUCache          cache_;

    int wordCount_ = 0;
    int nodeCount_ = 1;

    // ── Internal helpers ──────────────────────────────────────────────────
    const TrieNode* findNode(const std::string& prefix) const;
    TrieNode*       findNodeMutable(const std::string& prefix);

    TrieNode* insertWordNoLock(const std::string& normalizedWord);
    bool      deleteHelper(TrieNode* node, const std::string& word, int depth);

    void rebuildNodeSuggestions(TrieNode* node);
    void updateSuggestionsHelper(TrieNode* node, const std::string& word, size_t depth);
    void rebuildAllSuggestions(TrieNode* node);

    // UTF-8 aware case-fold (ASCII + Latin Extended-A/B + Vietnamese U+1E00-1EFF)
    static std::string normalize(const std::string& s);
    static uint32_t    unicodeLower(uint32_t cp) noexcept;

    // LCP divide & conquer internals
    static std::string lcpHelper(const std::vector<std::string>& words, int l, int r);
    static std::string commonPrefix(const std::string& a, const std::string& b);
};