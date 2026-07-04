#include "Trie.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>      // std::rename, std::remove
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <tuple>
#include <unordered_map>

// ═══════════════════════════════════════════════════════════════════════════
// Unicode helpers
// ═══════════════════════════════════════════════════════════════════════════

// Lowercase mapping for Unicode codepoints.
// Covers: Basic Latin (A-Z), Latin-1 Supplement (À-Þ),
//         Latin Extended-A (Ā-ž), Latin Extended Additional (Ḁ-ỿ) which
//         contains all Vietnamese precomposed characters (U+1E00-U+1EFF).
uint32_t Trie::unicodeLower(uint32_t cp) noexcept {
    if (cp >= 0x41 && cp <= 0x5A) return cp + 0x20;          // A-Z
    if (cp >= 0xC0 && cp <= 0xD6) return cp + 0x20;          // À-Ö
    if (cp >= 0xD8 && cp <= 0xDE) return cp + 0x20;          // Ø-Þ
    // Latin Extended-A (0x100-0x17E): mostly even=upper, odd=lower
    if (cp >= 0x100 && cp <= 0x12E && !(cp & 1)) return cp + 1;
    if (cp == 0x130) return 0x69;                             // İ → i
    if (cp >= 0x132 && cp <= 0x136 && !(cp & 1)) return cp + 1;
    if (cp >= 0x139 && cp <= 0x148 &&  (cp & 1)) return cp + 1;
    if (cp >= 0x14A && cp <= 0x177 && !(cp & 1)) return cp + 1;
    if (cp == 0x178) return 0xFF;                             // Ÿ → ÿ
    if (cp >= 0x179 && cp <= 0x17E &&  (cp & 1)) return cp + 1;
    // Latin Extended Additional U+1E00-U+1EFF (Vietnamese precomposed):
    // even codepoint = uppercase, odd = lowercase for virtually all pairs.
    if (cp >= 0x1E00 && cp <= 0x1EFE && !(cp & 1)) return cp + 1;
    return cp;
}

// UTF-8 aware case-fold to lowercase.
// Handles 1-, 2- and 3-byte sequences; passes 4-byte sequences through.
std::string Trie::normalize(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    const auto* p   = reinterpret_cast<const unsigned char*>(s.data());
    const auto* end = p + s.size();

    while (p < end) {
        uint32_t cp     = 0;
        int      seqLen = 1;

        if (*p < 0x80) {
            cp     = *p;
            seqLen = 1;
        } else if ((*p & 0xE0) == 0xC0 && p + 1 < end && (p[1] & 0xC0) == 0x80) {
            cp     = (static_cast<uint32_t>(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            seqLen = 2;
        } else if ((*p & 0xF0) == 0xE0 && p + 2 < end
                   && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp     = (static_cast<uint32_t>(p[0] & 0x0F) << 12)
                   | (static_cast<uint32_t>(p[1] & 0x3F) <<  6)
                   | (p[2] & 0x3F);
            seqLen = 3;
        } else if ((*p & 0xF8) == 0xF0 && p + 3 < end) {
            // 4-byte — pass through unchanged
            for (int i = 0; i < 4; ++i) result += static_cast<char>(p[i]);
            p += 4;
            continue;
        } else {
            // Invalid / lone continuation — copy raw byte
            result += static_cast<char>(*p++);
            continue;
        }

        cp = unicodeLower(cp);

        // Re-encode as UTF-8
        if (cp < 0x80) {
            result += static_cast<char>(cp);
        } else if (cp < 0x800) {
            result += static_cast<char>(0xC0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        }

        p += seqLen;
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════

Trie::Trie(std::size_t cacheCapacity)
    : root_(std::make_unique<TrieNode>()), cache_(cacheCapacity) {}

// ═══════════════════════════════════════════════════════════════════════════
// Internal node traversal
// ═══════════════════════════════════════════════════════════════════════════

const TrieNode* Trie::findNode(const std::string& prefix) const {
    const TrieNode* node = root_.get();
    for (char ch : prefix) {
        int idx = TrieNode::alphaIdx(ch);
        if (idx >= 0) {
            if (!node->alpha[idx]) return nullptr;
            node = node->alpha[idx].get();
        } else {
            auto it = node->fallback.find(ch);
            if (it == node->fallback.end()) return nullptr;
            node = it->second.get();
        }
    }
    return node;
}

TrieNode* Trie::findNodeMutable(const std::string& prefix) {
    TrieNode* node = root_.get();
    for (char ch : prefix) {
        int idx = TrieNode::alphaIdx(ch);
        if (idx >= 0) {
            if (!node->alpha[idx]) return nullptr;
            node = node->alpha[idx].get();
        } else {
            auto it = node->fallback.find(ch);
            if (it == node->fallback.end()) return nullptr;
            node = it->second.get();
        }
    }
    return node;
}

// ═══════════════════════════════════════════════════════════════════════════
// Insert (no lock — caller holds unique_lock)
// ═══════════════════════════════════════════════════════════════════════════

TrieNode* Trie::insertWordNoLock(const std::string& normalizedWord) {
    if (normalizedWord.empty()) return nullptr;

    TrieNode* node = root_.get();
    for (char ch : normalizedWord) {
        int idx = TrieNode::alphaIdx(ch);
        TrieNode* nextNode = nullptr;
        if (idx >= 0) {
            auto& slot = node->alpha[idx];
            if (!slot) {
                slot = std::make_unique<TrieNode>();
                nodeCount_++;
            }
            nextNode = slot.get();
        } else {
            auto& slot = node->fallback[ch];
            if (!slot) {
                slot = std::make_unique<TrieNode>();
                nodeCount_++;
            }
            nextNode = slot.get();
        }
        node = nextNode;
    }

    if (!node->isEndOfWord) {
        node->isEndOfWord   = true;
        node->word          = normalizedWord;
        // Start at frequency 1 so the word participates in ranking immediately
        // and the UI shows "Freq: 0 \u2192 1" on first submission.
        node->frequency     = 1;
        node->lastAccessTime = static_cast<int64_t>(std::time(nullptr));
        wordCount_++;
    }
    return node;
}

// ═══════════════════════════════════════════════════════════════════════════
// Public mutation API
// ═══════════════════════════════════════════════════════════════════════════

bool Trie::insert(const std::string& word) {
    if (word.empty()) return false;
    std::string norm = normalize(word);

    std::unique_lock<std::shared_mutex> lock(mutex_);
    int prev = wordCount_;
    insertWordNoLock(norm);

    if (wordCount_ > prev) {
        cache_.invalidate();
        updateSuggestionsHelper(root_.get(), norm, 0);
        return true;
    }
    return false;
}

bool Trie::search(const std::string& word) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const TrieNode* node = findNode(normalize(word));
    return node && node->isEndOfWord;
}

bool Trie::startsWith(const std::string& prefix) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return findNode(normalize(prefix)) != nullptr;
}

bool Trie::remove(const std::string& word) {
    if (word.empty()) return false;
    std::string norm = normalize(word);

    std::unique_lock<std::shared_mutex> lock(mutex_);
    int prev = wordCount_;
    deleteHelper(root_.get(), norm, 0);

    if (wordCount_ < prev) {
        cache_.invalidate();
        updateSuggestionsHelper(root_.get(), norm, 0);
        return true;
    }
    return false;
}

bool Trie::deleteHelper(TrieNode* node, const std::string& word, int depth) {
    if (depth == static_cast<int>(word.size())) {
        if (!node->isEndOfWord) return false;
        node->isEndOfWord = false;
        wordCount_--;
        // Return true if this node can be pruned (no children, not a word)
        for (const auto& slot : node->alpha) if (slot) return false;
        if (!node->fallback.empty()) return false;
        return true;
    }
    char ch = word[depth];
    int idx = TrieNode::alphaIdx(ch);
    if (idx >= 0) {
        if (!node->alpha[idx]) return false;
        TrieNode* child = node->alpha[idx].get();
        if (deleteHelper(child, word, depth + 1)) {
            node->alpha[idx].reset();
            nodeCount_--;
            if (!node->isEndOfWord) {
                for (const auto& slot : node->alpha) if (slot) return false;
                if (!node->fallback.empty()) return false;
                return true;
            }
        }
    } else {
        auto it = node->fallback.find(ch);
        if (it == node->fallback.end()) return false;
        TrieNode* child = it->second.get();
        if (deleteHelper(child, word, depth + 1)) {
            node->fallback.erase(it);
            nodeCount_--;
            if (!node->isEndOfWord) {
                for (const auto& slot : node->alpha) if (slot) return false;
                if (!node->fallback.empty()) return false;
                return true;
            }
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Batch insert
// ═══════════════════════════════════════════════════════════════════════════

int Trie::insertBatch(const std::vector<std::string>& words) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    cache_.invalidate();

    int added = 0, prev = wordCount_;
    for (const auto& w : words) {
        if (w.empty()) continue;
        int before = wordCount_;
        insertWordNoLock(normalize(w));
        if (wordCount_ > before) added++;
    }
    if (wordCount_ > prev) rebuildAllSuggestions(root_.get());
    return added;
}

int Trie::insertBatchNoRebuild(const std::vector<std::string>& words) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    cache_.invalidate();

    int added = 0;
    for (const auto& w : words) {
        if (w.empty()) continue;
        int before = wordCount_;
        insertWordNoLock(normalize(w));
        if (wordCount_ > before) added++;
    }
    return added;
}

void Trie::rebuildAll() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    rebuildAllSuggestions(root_.get());
}

// ═══════════════════════════════════════════════════════════════════════════
// Frequency management
// ═══════════════════════════════════════════════════════════════════════════

bool Trie::incrementFrequency(const std::string& word, int64_t timestamp) {
    std::string norm = normalize(word);
    std::unique_lock<std::shared_mutex> lock(mutex_);

    TrieNode* node = findNodeMutable(norm);
    if (!node || !node->isEndOfWord) return false;

    // Anti-spam: reject if called too recently for the same word
    int64_t now = (timestamp > 0) ? timestamp
                                  : static_cast<int64_t>(std::time(nullptr));
    if (node->lastAccessTime > 0 && (now - node->lastAccessTime) < kMinFreqIntervalSecs)
        return false;   // rate-limited

    cache_.invalidate();
    node->frequency++;
    node->lastAccessTime = now;
    updateSuggestionsHelper(root_.get(), norm, 0);
    return true;
}

int Trie::getFrequency(const std::string& word) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const TrieNode* node = findNode(normalize(word));
    return (node && node->isEndOfWord) ? node->frequency : 0;
}

int64_t Trie::getLastAccessTime(const std::string& word) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const TrieNode* node = findNode(normalize(word));
    return (node && node->isEndOfWord) ? node->lastAccessTime : 0;
}


// Combined search + conditional increment — 1 lock acquisition instead of 3.
Trie::SearchResult Trie::searchAndUpdate(const std::string& word, int64_t timestamp) {
    std::string norm = normalize(word);
    int64_t now = (timestamp > 0) ? timestamp
                                  : static_cast<int64_t>(std::time(nullptr));

    std::unique_lock<std::shared_mutex> lock(mutex_);

    TrieNode* node = findNodeMutable(norm);
    if (!node || !node->isEndOfWord) {
        return {false, false, 0};
    }

    int freqBefore         = node->frequency;
    int64_t lastAccessBefore = node->lastAccessTime;

    // Anti-spam check
    bool incremented = false;
    if (node->lastAccessTime == 0 || (now - node->lastAccessTime) >= kMinFreqIntervalSecs) {
        cache_.invalidate();
        node->frequency++;
        node->lastAccessTime = now;
        updateSuggestionsHelper(root_.get(), norm, 0);
        incremented = true;
    }

    return {true, incremented, freqBefore, lastAccessBefore};
}

// ═══════════════════════════════════════════════════════════════════════════
// Autocomplete  (O(1) from topSuggestions cache + LRU)
// ═══════════════════════════════════════════════════════════════════════════

AutocompleteResult Trie::autocomplete(const std::string& prefix, int maxResults) const {
    auto t0 = std::chrono::high_resolution_clock::now();

    std::string norm = normalize(prefix);
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // LRU cache check (LRUCache has its own internal mutex → safe under shared_lock)
    auto cached = cache_.get(norm);
    if (cached) {
        cached->fromCache    = true;
        cached->searchTimeMs = std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t0).count();
        return *cached;
    }

    const TrieNode* node = findNode(norm);
    AutocompleteResult result;
    result.fromCache = false;

    if (node) {
        const auto& top = node->topSuggestions;
        int take = std::min(static_cast<int>(top.size()), maxResults);
        result.words.reserve(static_cast<size_t>(take));
        for (int i = 0; i < take; ++i) {
            result.words.push_back({top[i].word, top[i].freq, top[i].lastAccess});
        }
    }

    // Cache a copy with fromCache=true so future hits report correctly.
    AutocompleteResult toCache = result;
    toCache.fromCache = true;
    cache_.put(norm, std::move(toCache));

    result.searchTimeMs = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - t0).count();
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// Suggestion cache rebuild
// ═══════════════════════════════════════════════════════════════════════════

// Composite score for ranking: frequency + recency bonus (decays with 7-day half-life).
static double suggestionScore(const SuggestionEntry& e) noexcept {
    double f = static_cast<double>(e.freq);
    if (e.lastAccess <= 0 || f <= 0.0) return f;
    double ageHours = static_cast<double>(std::time(nullptr) - e.lastAccess) / 3600.0;
    double decay    = std::exp(-ageHours / 168.0);   // half-life = 7 days
    return f + decay * std::min(f, 5.0);             // recency bonus capped at +5
}

void Trie::rebuildNodeSuggestions(TrieNode* node) {
    if (!node) return;
    node->topSuggestions.clear();

    if (node->isEndOfWord)
        node->topSuggestions.push_back({node->frequency, node->lastAccessTime, node->word});

    node->forEachChild([&](char, const TrieNode* child) {
        for (const auto& e : child->topSuggestions)
            node->topSuggestions.push_back(e);
    });

    const int K = kMaxStoredSuggestions;
    auto cmp = [](const SuggestionEntry& a, const SuggestionEntry& b) {
        double sa = suggestionScore(a), sb = suggestionScore(b);
        if (sa != sb) return sa > sb;
        return a.word < b.word;   // stable alphabetical tiebreak
    };

    auto& v = node->topSuggestions;
    if (static_cast<int>(v.size()) > K) {
        // nth_element: O(N) average vs sort's O(N log N) — 2-3x faster for large N
        std::nth_element(v.begin(), v.begin() + K, v.end(), cmp);
        v.resize(static_cast<size_t>(K));
        // Sort just the kept K entries so callers get them in order
        std::sort(v.begin(), v.end(), cmp);
    } else {
        std::sort(v.begin(), v.end(), cmp);
    }
}

// Update the topSuggestions cache for every ancestor of `word` from root downward.
// Must traverse both alpha[] and fallback map children, otherwise UTF-8 words
// (Vietnamese, etc.) silently drop their path and autocomplete breaks.
void Trie::updateSuggestionsHelper(TrieNode* node, const std::string& word, size_t depth) {
    if (!node) return;
    if (depth < word.size()) {
        char ch  = word[depth];
        int  idx = TrieNode::alphaIdx(ch);
        TrieNode* next = nullptr;
        if (idx >= 0) {
            next = node->alpha[idx].get();
        } else {
            auto it = node->fallback.find(ch);
            if (it != node->fallback.end()) next = it->second.get();
        }
        if (next) updateSuggestionsHelper(next, word, depth + 1);
    }
    rebuildNodeSuggestions(node);
}

void Trie::rebuildAllSuggestions(TrieNode* node) {
    if (!node) return;
    node->forEachChildMut([&](char, std::unique_ptr<TrieNode>& child) {
        rebuildAllSuggestions(child.get());
    });
    rebuildNodeSuggestions(node);
}

// ═══════════════════════════════════════════════════════════════════════════
// Fuzzy search (Levenshtein DP with trie pruning)
// ═══════════════════════════════════════════════════════════════════════════

std::vector<FuzzyResult> Trie::fuzzySearch(const std::string& word, int maxErrors) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<FuzzyResult> results;
    if (word.empty()) return results;

    std::string norm = normalize(word);
    int n = static_cast<int>(norm.size());

    // Pre-allocate DP matrix: max depth support of 52 (covers words up to 50 chars).
    // Index 0 is the start state, up to index 51.
    std::vector<std::vector<int>> dp(52, std::vector<int>(n + 1));
    for (int i = 0; i <= n; ++i) dp[0][i] = i;

    std::function<void(const TrieNode*, int, char)> dfs =
        [&](const TrieNode* node, int depth, char ch) {
            if (depth >= 52) return; // Guard against excessively deep trees

            int prev_depth = depth - 1;
            dp[depth][0] = dp[prev_depth][0] + 1;

            int minVal = dp[depth][0];
            for (int i = 1; i <= n; ++i) {
                int cost = (ch != norm[i - 1]) ? 1 : 0;
                dp[depth][i] = std::min({dp[depth][i-1] + 1, dp[prev_depth][i] + 1, dp[prev_depth][i-1] + cost});
                if (dp[depth][i] < minVal) minVal = dp[depth][i];
            }

            if (dp[depth][n] <= maxErrors && node->isEndOfWord) {
                results.push_back({node->word, dp[depth][n], node->frequency});
            }

            if (minVal <= maxErrors) {
                node->forEachChild([&](char c, const TrieNode* child) {
                    dfs(child, depth + 1, c);
                });
            }
        };

    root_->forEachChild([&](char c, const TrieNode* child) {
        dfs(child, 1, c);
    });

    std::sort(results.begin(), results.end());
    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
// Statistics
// ═══════════════════════════════════════════════════════════════════════════

int Trie::getWordCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return wordCount_;
}
int Trie::getNodeCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodeCount_;
}

std::vector<std::string> Trie::getAllWords() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> out;
    out.reserve(static_cast<size_t>(wordCount_));
    std::function<void(const TrieNode*)> dfs = [&](const TrieNode* node) {
        if (node->isEndOfWord) out.push_back(node->word);
        node->forEachChild([&](char, const TrieNode* c) { dfs(c); });
    };
    root_->forEachChild([&](char, const TrieNode* c) { dfs(c); });
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// Persistence  (V2 binary format with magic header + lastAccessTime)
// ═══════════════════════════════════════════════════════════════════════════

static const char kMagic[4] = {'T','R','I','2'};

bool Trie::saveToDisk(const std::string& filename) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::ofstream f(filename, std::ios::binary);
    if (!f) return false;

    f.write(kMagic, 4);

    // Collect all word records via iterative DFS (avoids stack overflow with deep tries)
    struct Rec { std::string word; int freq; int64_t lastAccess; };
    std::vector<Rec> records;
    records.reserve(static_cast<size_t>(wordCount_));

    std::vector<const TrieNode*> stack;
    stack.reserve(256);
    root_->forEachChild([&](char, const TrieNode* c) { stack.push_back(c); });

    while (!stack.empty()) {
        const TrieNode* n = stack.back(); stack.pop_back();
        if (n->isEndOfWord) records.push_back({n->word, n->frequency, n->lastAccessTime});
        n->forEachChild([&](char, const TrieNode* c) { stack.push_back(c); });
    }

    auto wrt = [&](const void* d, std::streamsize sz) {
        f.write(static_cast<const char*>(d), sz); return f.good();
    };

    int cnt = static_cast<int>(records.size());
    if (!wrt(&cnt, sizeof(cnt))) return false;

    for (const auto& r : records) {
        uint32_t len = static_cast<uint32_t>(r.word.size());
        if (!wrt(&len, sizeof(len))) return false;
        if (!wrt(r.word.data(), static_cast<std::streamsize>(len))) return false;
        if (!wrt(&r.freq, sizeof(r.freq))) return false;
        if (!wrt(&r.lastAccess, sizeof(r.lastAccess))) return false;
    }

    f.close();
    return !f.fail();
}

bool Trie::saveToDiskAtomic(const std::string& filename) const {
    std::string tmp = filename + ".tmp";
    std::string bak = filename + ".bak";

    if (!saveToDisk(tmp)) { std::remove(tmp.c_str()); return false; }

    std::remove(bak.c_str());
    std::rename(filename.c_str(), bak.c_str());         // current → .bak
    if (std::rename(tmp.c_str(), filename.c_str()) != 0) {
        std::rename(bak.c_str(), filename.c_str());     // restore if rename fails
        return false;
    }
    return true;
}

bool Trie::loadFromDisk(const std::string& filename) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    std::ifstream f(filename, std::ios::binary);
    if (!f) return false;

    // Detect V1 vs V2 format
    char magic[4] = {};
    f.read(magic, 4);
    if (f.fail()) return false;
    bool isV2 = (magic[0]=='T' && magic[1]=='R' && magic[2]=='I' && magic[3]=='2');

    if (!isV2) {
        // V1: no magic; first 4 bytes are start of wordCount int.
        // Rewind and skip the old 2-int header (wordCount + nodeCount).
        f.seekg(0);
        int dummy[2] = {};
        f.read(reinterpret_cast<char*>(dummy), sizeof(dummy));
        if (f.fail()) return false;
    }

    int cnt = 0;
    f.read(reinterpret_cast<char*>(&cnt), sizeof(cnt));
    if (f.fail() || cnt < 0 || cnt > 5'000'000) return false;

    struct Rec { std::string word; int freq; int64_t lastAccess; };
    std::vector<Rec> records;
    records.reserve(static_cast<size_t>(cnt));

    for (int i = 0; i < cnt; ++i) {
        uint32_t len = 0;
        f.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (f.fail() || len == 0 || len > 1024) return false;

        std::string word(len, '\0');
        f.read(word.data(), static_cast<std::streamsize>(len));
        if (f.fail()) return false;

        int freq = 0;
        f.read(reinterpret_cast<char*>(&freq), sizeof(freq));
        if (f.fail()) return false;

        int64_t lastAccess = 0;
        if (isV2) {
            f.read(reinterpret_cast<char*>(&lastAccess), sizeof(lastAccess));
            if (f.fail()) return false;
        }

        records.push_back({std::move(word), freq, lastAccess});
    }
    f.close();

    // All data read successfully — now reset and rebuild trie
    root_      = std::make_unique<TrieNode>();
    wordCount_ = 0;
    nodeCount_ = 1;
    cache_.invalidate();

    for (auto& r : records) {
        TrieNode* node = insertWordNoLock(r.word);
        if (node) { node->frequency = r.freq; node->lastAccessTime = r.lastAccess; }
    }
    rebuildAllSuggestions(root_.get());
    return true;
}
