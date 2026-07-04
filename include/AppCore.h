#pragma once

#include "Trie.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "SearchTypes.h"

// ═══════════════════════════════════════════════════════════════════════════
// AppStats — lock-free session counters
// ═══════════════════════════════════════════════════════════════════════════

struct AppStats {
    AppStats() = default;

    std::atomic<uint64_t> totalSearches{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> newWordsLearned{0};
    std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::now()};

    int uptimeSecs()  const;
    int cacheHitPct() const;
};

// ═══════════════════════════════════════════════════════════════════════════
// Analytics — per-session search history and word counts
// ═══════════════════════════════════════════════════════════════════════════

class Analytics {
public:
    Analytics() = default;

    void record(const std::string& word, bool isNew, bool fromCache);

    // Top N by session hit count (desc)
    std::vector<std::pair<std::string, int>> topSearches(int n) const;

    // Counts for current session
    int sessionTotal() const;
    int sessionNewWords() const;

    // Hourly activity over last 24h (index 0 = 24h ago, index 23 = current hour)
    std::array<int, 24> hourlyActivity() const;

    // Full search history (newest last) — for history navigation
    std::vector<std::string> history() const;

    // Persistence (text format: "word\tcount" per line)
    bool saveToDisk(const std::string& path) const;
    bool loadFromDisk(const std::string& path);

private:
    mutable std::mutex                     mu_;
    std::vector<SearchRecord>              records_;        // chronological
    std::unordered_map<std::string, int>   wordCounts_;     // aggregated
};

// ═══════════════════════════════════════════════════════════════════════════
// AutoSaver — background periodic save thread
// ═══════════════════════════════════════════════════════════════════════════

class AutoSaver {
public:
    AutoSaver(Trie& trie, Analytics& analytics,
              std::string trieFile, std::string analyticsFile,
              int intervalSecs);
    ~AutoSaver();

    void triggerNow();
    std::string lastSaveInfo() const;

private:
    void loop();

    Trie&                   trie_;
    Analytics&              analytics_;
    std::string             trieFile_;
    std::string             analyticsFile_;
    int                     interval_;
    std::atomic<bool>       running_;
    std::thread             thread_;
    mutable std::mutex      mu_;
    std::condition_variable cv_;
    std::string             lastInfo_{"never"};
};

// ═══════════════════════════════════════════════════════════════════════════
// InputValidator — keystroke-level input filtering
// ═══════════════════════════════════════════════════════════════════════════

class InputValidator {
public:
    static constexpr int kMaxLen = 50;
    static constexpr int kMinLen = 1;

    struct Result { bool ok; const char* reason; };

    // Validate adding char c to currentQuery.
    static Result validate(char c, const std::string& currentQuery);

    // Validate a complete word (e.g. from history recall or TAB complete).
    static Result validateWord(const std::string& word);
};

// ═══════════════════════════════════════════════════════════════════════════
// Dictionary loading helpers
// ═══════════════════════════════════════════════════════════════════════════

// Load from file with optional per-batch progress callback: fn(loaded, estimate).
// Uses insertBatchNoRebuild + one final rebuildAll for faster startup.
int loadDictFile(Trie& trie, const std::string& path,
                 std::function<void(int, int)> onProgress = nullptr);

void generateFallbackWords(Trie& trie);
