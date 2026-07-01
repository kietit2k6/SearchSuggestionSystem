#pragma once

#include <string>
#include <vector>
using namespace std;
// ── Entry trong kết quả autocomplete
struct WordEntry {
    string word;
    int frequency = 0;
};

// ── Kết quả autocomplete 
struct AutocompleteResult {
    vector<WordEntry> words;
    double searchTimeMs = 0.0;
    bool   fromCache    = false;   
};

struct FuzzyResult {
    string word;
    int editDistance = 0;
    int frequency    = 0;

    bool operator<(const FuzzyResult& o) const noexcept {
        if (editDistance != o.editDistance) return editDistance < o.editDistance;
        return frequency > o.frequency;
    }
};
