// SearchSuggestionTests — unit tests for the SearchEngine static library.
//
// Build: cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
// Run:   ./build/SearchSuggestionTests

#include "Trie.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

// ── Test harness ─────────────────────────────────────────────────────────────

static int gPassed = 0;
static int gFailed = 0;

#define ASSERT(expr)                                                         \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::cerr << "  FAIL  " << __FILE__ << ":" << __LINE__          \
                      << "  " << #expr << "\n";                              \
            ++gFailed;                                                       \
            return;   /* skip rest of test on first failure */               \
        }                                                                    \
    } while (0)

#define RUN_TEST(fn)                                        \
    do {                                                    \
        std::cout << "  Testing " << #fn << " ... " << std::flush; \
        fn();                                               \
        if (gFailed == 0) {                                 \
            std::cout << "OK\n";                            \
            ++gPassed;                                      \
        } else {                                            \
            std::cout << "(see above)\n";                   \
        }                                                   \
    } while (0)

// ── Individual tests ─────────────────────────────────────────────────────────

// P0-C: new words must start at frequency 1, not 0.
void testInsertFrequencyStartsAtOne() {
    Trie trie;
    ASSERT(trie.insert("hello"));
    ASSERT(trie.getFrequency("hello") == 1);
    ASSERT(trie.getLastAccessTime("hello") > 0);

    // Inserting again (already exists) must not reset/increment
    ASSERT(!trie.insert("hello"));
    ASSERT(trie.getFrequency("hello") == 1);
}

void testTrieBasic() {
    Trie trie;
    ASSERT(!trie.search("hello"));
    ASSERT(trie.insert("hello"));
    ASSERT(trie.search("hello"));
    ASSERT(!trie.insert("hello")); // duplicate

    ASSERT(trie.startsWith("he"));
    ASSERT(trie.startsWith("hello"));
    ASSERT(!trie.startsWith("hi"));
    ASSERT(trie.getWordCount() == 1);
}

// P0-A: autocomplete must work for UTF-8 / Vietnamese words.
void testTrieFallbackMapUTF8() {
    Trie trie;
    // Space
    ASSERT(trie.insert("vietnam 101"));
    ASSERT(trie.search("vietnam 101"));

    // Vietnamese UTF-8 precomposed (U+00E0 = 'à', U+0169 = 'ũ')
    ASSERT(trie.insert("xin chào"));
    ASSERT(trie.search("xin chào"));

    // Vietnamese words with dot-below (dấu nặng): ạ, ặ, ậ, ẹ, ệ, ị, ọ, ộ, ợ, ụ, ự, ỵ
    ASSERT(trie.insert("nặng"));
    ASSERT(trie.search("nặng"));
    ASSERT(trie.insert("lịch"));
    ASSERT(trie.search("lịch"));
    ASSERT(trie.insert("học"));
    ASSERT(trie.search("học"));
    ASSERT(trie.insert("nhập"));
    ASSERT(trie.search("nhập"));
    ASSERT(trie.insert("vực"));
    ASSERT(trie.search("vực"));
    ASSERT(trie.insert("chị"));
    ASSERT(trie.search("chị"));
    ASSERT(trie.insert("nhận"));
    ASSERT(trie.search("nhận"));

    // ASCII punctuation not in fast-path
    ASSERT(trie.insert("c++"));
    ASSERT(trie.search("c++"));

    // Case-insensitivity checks for 'dấu nặng' words
    ASSERT(trie.search("NẶNG"));
    ASSERT(trie.search("LỊCH"));
    ASSERT(trie.search("HỌC"));
    ASSERT(trie.search("NHẬP"));
    ASSERT(trie.search("VỰC"));
    ASSERT(trie.search("CHỊ"));
    ASSERT(trie.search("NHẬN"));

    // NFD (decomposed) search matching NFC (precomposed)


    ASSERT(trie.search("na\xCC\x86\xCC\xA3ng")); // "nặng" in NFD

    // case folding for Ơ / Ư
    ASSERT(trie.insert("Mơ"));
    ASSERT(trie.search("mơ"));
    ASSERT(trie.search("MƠ"));

    ASSERT(trie.getWordCount() == 11);

    // P0-A: autocomplete for Vietnamese prefix must find the word.
    trie.rebuildAll();
    AutocompleteResult res = trie.autocomplete("xin", 5);
    bool found = false;
    for (const auto& e : res.words)
        if (e.word == "xin chào") { found = true; break; }
    ASSERT(found); // fails before the updateSuggestionsHelper fix
}

void testTrieDeletion() {
    Trie trie;
    trie.insert("he");
    trie.insert("hello");
    trie.insert("world");

    int nodesBefore = trie.getNodeCount();
    ASSERT(trie.remove("world"));
    ASSERT(!trie.search("world"));
    ASSERT(trie.getNodeCount() < nodesBefore); // branch pruned

    ASSERT(trie.search("he"));
    ASSERT(trie.search("hello"));
    ASSERT(trie.remove("hello"));
    ASSERT(!trie.search("hello"));
    ASSERT(trie.search("he")); // "he" is an independent word, must survive
}

// LRU cache: first hit misses, second hits; mutation invalidates.
void testLRUCache() {
    Trie trie;
    trie.insert("apple");
    trie.insert("apricot");
    trie.rebuildAll();

    AutocompleteResult r1 = trie.autocomplete("ap", 5);
    ASSERT(!r1.fromCache);
    ASSERT(r1.words.size() == 2);

    AutocompleteResult r2 = trie.autocomplete("ap", 5);
    ASSERT(r2.fromCache); // same key, must hit

    // Inserting a new word invalidates the cache entry.
    trie.insert("apex");
    AutocompleteResult r3 = trie.autocomplete("ap", 5);
    ASSERT(!r3.fromCache);
    ASSERT(r3.words.size() == 3);
}

// Autocomplete ranking: higher-frequency word appears first.
void testAutocompleteRanking() {
    Trie trie;
    trie.insert("test");
    trie.insert("testing");
    trie.insert("tester");
    trie.rebuildAll();

    // All start at frequency 1 — alphabetical tiebreak expected.
    AutocompleteResult r1 = trie.autocomplete("te", 5);
    ASSERT(r1.words.size() == 3);

    // Artificially advance time so the rate-limit window passes,
    // then bump "testing" to frequency 2 (freq 1 from insert + 1 increment).
    int64_t futureTs = static_cast<int64_t>(std::time(nullptr)) + 10;
    bool ok = trie.incrementFrequency("testing", futureTs);
    ASSERT(ok); // should succeed since timestamp > lastAccessTime

    // After frequency bump, cache is invalidated — "testing" should rank first.
    AutocompleteResult r2 = trie.autocomplete("te", 5);
    ASSERT(!r2.words.empty());
    ASSERT(r2.words[0].word == "testing");
}

// P0-B + SearchResult.lastAccessBefore
void testSearchAndUpdate() {
    Trie trie;
    trie.insert("banana");
    // insert() sets lastAccessTime = now. Use a timestamp past the rate-limit
    // window so the first searchAndUpdate call actually increments the frequency.
    int64_t base = static_cast<int64_t>(std::time(nullptr));
    int64_t t1   = base + Trie::kMinFreqIntervalSecs + 1;

    auto res1 = trie.searchAndUpdate("banana", t1);
    ASSERT(res1.found);
    ASSERT(res1.incremented);          // window has passed: freq 1 -> 2
    ASSERT(res1.freqBefore == 1);

    // Immediate re-submit at same timestamp: rate-limited.
    auto res2 = trie.searchAndUpdate("banana", t1);
    ASSERT(res2.found);
    ASSERT(!res2.incremented);
    ASSERT(res2.lastAccessBefore == t1); // captured before the (no-op) update

    // Submit after another rate-limit window: should succeed again.
    int64_t t2 = t1 + Trie::kMinFreqIntervalSecs + 1;
    auto res3 = trie.searchAndUpdate("banana", t2);
    ASSERT(res3.found && res3.incremented);
    ASSERT(trie.getFrequency("banana") == 3);
}

void testFuzzySearch() {
    Trie trie;
    trie.insert("hello");
    trie.insert("hell");
    trie.insert("help");
    trie.insert("yellow");
    trie.rebuildAll();

    auto res = trie.fuzzySearch("hell", 1);
    // Expected: "hell" (dist 0), "hello" (dist 1), "help" (dist 1)
    // "yellow" has dist ≥ 4 — must not appear.
    ASSERT(res.size() == 3);

    bool hasHell = false, hasHello = false, hasHelp = false;
    for (const auto& r : res) {
        if (r.word == "hell")  { hasHell  = true; ASSERT(r.editDistance == 0); }
        if (r.word == "hello") { hasHello = true; ASSERT(r.editDistance == 1); }
        if (r.word == "help")  { hasHelp  = true; ASSERT(r.editDistance == 1); }
    }
    ASSERT(hasHell && hasHello && hasHelp);
}

void testPersistence() {
    const std::string file = "test_trie_persistence.bin";
    std::remove(file.c_str());
    std::remove((file + ".bak").c_str());
    std::remove((file + ".tmp").c_str());

    {
        Trie trie;
        trie.insert("apple");
        trie.insert("banana");
        // Bump "apple" with a timestamp beyond the rate-limit window.
        trie.incrementFrequency("apple", static_cast<int64_t>(std::time(nullptr)) + 10);
        ASSERT(trie.saveToDiskAtomic(file));
    }

    {
        Trie trie;
        ASSERT(trie.loadFromDisk(file));
        ASSERT(trie.search("apple"));
        ASSERT(trie.search("banana"));
        // "apple" was incremented once after insert, so frequency == 2.
        ASSERT(trie.getFrequency("apple") == 2);
        // "banana" was never incremented beyond insert, so frequency == 1.
        ASSERT(trie.getFrequency("banana") == 1);
    }

    std::remove(file.c_str());
    std::remove((file + ".bak").c_str());
}

void testGetLastAccessTime() {
    Trie trie;
    ASSERT(trie.getLastAccessTime("unknown") == 0);

    trie.insert("word");
    int64_t t = trie.getLastAccessTime("word");
    ASSERT(t > 0); // set during insert
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "════════════════════════════════════════\n";
    std::cout << "  SearchEngine Unit Tests\n";
    std::cout << "════════════════════════════════════════\n";

    // P0 regressions first
    RUN_TEST(testInsertFrequencyStartsAtOne);   // P0-C
    RUN_TEST(testSearchAndUpdate);              // P0-B
    RUN_TEST(testTrieFallbackMapUTF8);          // P0-A

    // Core correctness
    RUN_TEST(testTrieBasic);
    RUN_TEST(testTrieDeletion);
    RUN_TEST(testLRUCache);
    RUN_TEST(testAutocompleteRanking);
    RUN_TEST(testFuzzySearch);
    RUN_TEST(testPersistence);
    RUN_TEST(testGetLastAccessTime);

    std::cout << "════════════════════════════════════════\n";
    std::cout << "  " << gPassed << " passed  |  " << gFailed << " failed\n";
    std::cout << "════════════════════════════════════════\n";
    return gFailed > 0 ? 1 : 0;
}
