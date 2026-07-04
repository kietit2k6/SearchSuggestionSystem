#include "Trie.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdio>

#define RUN_TEST(fn) \
    do { \
        std::cout << "Running " << #fn << " ... " << std::flush; \
        fn(); \
        std::cout << "PASSED\n"; \
    } while (0)

void testTrieBasic() {
    Trie trie;
    assert(!trie.search("hello"));
    assert(trie.insert("hello"));
    assert(trie.search("hello"));
    assert(!trie.insert("hello")); // already inserted

    assert(trie.startsWith("he"));
    assert(trie.startsWith("hello"));
    assert(!trie.startsWith("hi"));
    assert(trie.getWordCount() == 1);
}

void testTrieFallbackMap() {
    Trie trie;
    // Test spaces, digits, punctuation, and Vietnamese UTF-8 precomposed characters
    assert(trie.insert("vietnam 101"));
    assert(trie.search("vietnam 101"));

    assert(trie.insert("xin chào"));
    assert(trie.search("xin chào"));

    assert(trie.insert("antigravity@gemini"));
    assert(trie.search("antigravity@gemini"));

    assert(trie.getWordCount() == 3);
}

void testTrieDeletion() {
    Trie trie;
    trie.insert("he");
    trie.insert("hello");
    trie.insert("world");

    int nodesBefore = trie.getNodeCount();
    assert(trie.remove("world"));
    assert(!trie.search("world"));
    int nodesAfter = trie.getNodeCount();
    assert(nodesAfter < nodesBefore); // "world" branch should be pruned

    assert(trie.search("he"));
    assert(trie.search("hello"));
    assert(trie.remove("hello"));
    assert(!trie.search("hello"));
    assert(trie.search("he")); // "he" should not be deleted because it is a prefix of "hello" but still stands on its own
}

void testLRUCache() {
    Trie trie;
    trie.insert("apple");
    trie.insert("apricot");

    // First call caches autocomplete results
    AutocompleteResult res1 = trie.autocomplete("ap", 5);
    assert(!res1.fromCache);
    assert(res1.words.size() == 2);

    // Second call should hit the cache
    AutocompleteResult res2 = trie.autocomplete("ap", 5);
    assert(res2.fromCache);

    // Modifying Trie invalidates cache
    trie.insert("apex");
    AutocompleteResult res3 = trie.autocomplete("ap", 5);
    assert(!res3.fromCache);
    assert(res3.words.size() == 3);
}

void testAutocompleteRanking() {
    Trie trie;
    trie.insert("test");
    trie.insert("testing");
    trie.insert("tester");

    // By default, alphabetical order or score order
    AutocompleteResult res = trie.autocomplete("te", 5);
    assert(res.words.size() == 3);

    // Increment frequency of a word to push it up
    trie.incrementFrequency("testing");
    AutocompleteResult res2 = trie.autocomplete("te", 5);
    assert(res2.words[0].word == "testing");
}

void testFuzzySearch() {
    Trie trie;
    trie.insert("hello");
    trie.insert("hell");
    trie.insert("help");
    trie.insert("yellow");

    // Fuzzy search for "hell" with maxErrors = 1
    std::vector<FuzzyResult> res = trie.fuzzySearch("hell", 1);
    // Should match "hell" (dist 0), "hello" (dist 1, deletion), "help" (dist 1, sub)
    // Should NOT match "yellow" (dist 3)
    assert(res.size() == 3);

    bool hasHell = false, hasHello = false, hasHelp = false;
    for (const auto& r : res) {
        if (r.word == "hell") { hasHell = true; assert(r.editDistance == 0); }
        else if (r.word == "hello") { hasHello = true; assert(r.editDistance == 1); }
        else if (r.word == "help") { hasHelp = true; assert(r.editDistance == 1); }
    }
    assert(hasHell && hasHello && hasHelp);
}

void testPersistence() {
    std::string filename = "test_persistence.bin";
    std::remove(filename.c_str());

    {
        Trie trie;
        trie.insert("apple");
        trie.insert("banana");
        trie.incrementFrequency("apple");
        assert(trie.saveToDiskAtomic(filename));
    }

    {
        Trie trie;
        assert(trie.loadFromDisk(filename));
        assert(trie.search("apple"));
        assert(trie.search("banana"));
        assert(trie.getFrequency("apple") == 2);
        assert(trie.getFrequency("banana") == 1);
    }

    std::remove(filename.c_str());
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Starting SearchEngine Unit Tests\n";
    std::cout << "========================================\n";

    RUN_TEST(testTrieBasic);
    RUN_TEST(testTrieFallbackMap);
    RUN_TEST(testTrieDeletion);
    RUN_TEST(testLRUCache);
    RUN_TEST(testAutocompleteRanking);
    RUN_TEST(testFuzzySearch);
    RUN_TEST(testPersistence);

    std::cout << "========================================\n";
    std::cout << "All SearchEngine Unit Tests Passed!\n";
    std::cout << "========================================\n";
    return 0;
}
