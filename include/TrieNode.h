#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────
// SuggestionEntry
// Cached entry stored inside TrieNode::topSuggestions.
// Carries both raw frequency (for display) and lastAccess timestamp
// (for recency-weighted ranking) so composite scoring can happen during
// sort without extra trie lookups.
// ──────────────────────────────────────────────────────────────────────────
struct SuggestionEntry {
    int         freq      = 0;    // raw search frequency (for display bar)
    int64_t     lastAccess = 0;   // Unix epoch seconds; 0 = never searched
    std::string word;
};

// ──────────────────────────────────────────────────────────────────────────
// TrieNode
//
// Performance note on children container:
//   std::unordered_map<char,…> has ~40-80 B overhead per node (bucket array,
//   load factor, hash state) plus two pointer indirections.  For a trie with
//   >750k nodes that is 30-60 MB of extra RAM and heavy cache-miss pressure.
//
//   Instead we use std::array<27> (a-z + apostrophe) giving O(1) indexed
//   access with a single pointer dereference and no heap allocation per node.
//   Index mapping: 'a'-'z' → 0-25,  '\'' → 26.
//   Any character outside this alphabet falls through to the fallback map.
// ──────────────────────────────────────────────────────────────────────────
struct TrieNode {
    // 27 slots: indices 0-25 = 'a'-'z', index 26 = '\'' (apostrophe)
    static constexpr int kAlphaSlots = 27;
    std::array<std::unique_ptr<TrieNode>, kAlphaSlots> alpha{};   // fast path

    // Fallback map for characters outside the fast path (digits, spaces, hyphens, UTF-8 bytes)
    std::unordered_map<char, std::unique_ptr<TrieNode>> fallback;

    bool        isEndOfWord    = false;
    int         frequency      = 0;
    int64_t     lastAccessTime = 0;   // epoch seconds; 0 = never searched
    std::string word;                 // full normalized word (set when isEndOfWord)

    // Cached top-K suggestions for the subtree rooted at this node,
    // sorted descending by composite score (freq + recency bonus).
    std::vector<SuggestionEntry> topSuggestions;

    // ── Child access helpers ──────────────────────────────────────────────

    // Map a character to its alpha-slot index, or -1 if not in fast path.
    static int alphaIdx(char c) noexcept {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 'a' && uc <= 'z') return uc - 'a';
        if (c == '\'')              return 26;
        return -1;
    }

    // Get child pointer (may be null) — const version.
    const TrieNode* child(char c) const noexcept {
        int idx = alphaIdx(c);
        if (idx >= 0) return alpha[idx].get();
        auto it = fallback.find(c);
        return (it != fallback.end()) ? it->second.get() : nullptr;
    }

    // Iterate over all existing children (for DFS / rebuild).
    template <typename Fn>
    void forEachChild(Fn&& fn) const {
        for (int i = 0; i < kAlphaSlots; ++i) {
            if (alpha[i]) fn(static_cast<char>(i < 26 ? 'a' + i : '\''),
                             alpha[i].get());
        }
        for (const auto& [ch, childPtr] : fallback) {
            fn(ch, childPtr.get());
        }
    }

    template <typename Fn>
    void forEachChildMut(Fn&& fn) {
        for (int i = 0; i < kAlphaSlots; ++i) {
            if (alpha[i]) fn(static_cast<char>(i < 26 ? 'a' + i : '\''),
                             alpha[i]);
        }
        for (auto& [ch, childPtr] : fallback) {
            fn(ch, childPtr);
        }
    }
};