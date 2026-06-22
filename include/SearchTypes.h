#pragma once

#include <string>
#include <vector>

// ── Entry trong kết quả autocomplete
struct WordEntry {
    std::string word;
    int frequency = 0;
};

// ── Kết quả autocomplete 
struct AutocompleteResult {
    std::vector<WordEntry> words;
    double searchTimeMs = 0.0;
    bool   fromCache    = false;   
};

struct FuzzyResult {
    std::string word;
    int editDistance = 0;
    int frequency    = 0;

    bool operator<(const FuzzyResult& o) const noexcept {
        if (editDistance != o.editDistance) return editDistance < o.editDistance;
        return frequency > o.frequency;
    }
};
