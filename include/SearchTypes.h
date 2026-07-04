#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ── Entry in an autocomplete result ────────────────────────────────────────
struct WordEntry {
    std::string word;
    int         frequency  = 0;
    int64_t     lastAccess = 0;   // epoch seconds; 0 = never searched
};

// ── Autocomplete result bundle ──────────────────────────────────────────────
struct AutocompleteResult {
    std::vector<WordEntry> words;
    double searchTimeMs = 0.0;
    bool   fromCache    = false;
};

// ── Fuzzy search result ─────────────────────────────────────────────────────
struct FuzzyResult {
    std::string word;
    int editDistance = 0;
    int frequency    = 0;

    bool operator<(const FuzzyResult& o) const noexcept {
        if (editDistance != o.editDistance) return editDistance < o.editDistance;
        return frequency > o.frequency;   // higher freq first when same distance
    }
};

// ── Analytics: single search event ─────────────────────────────────────────
struct SearchRecord {
    std::string word;
    int64_t     timestamp = 0;    // epoch seconds
    bool        isNewWord = false;
    bool        fromCache = false;
};
