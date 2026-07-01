#include <iostream>
#include "Trie.h"
using namespace std;

int main()
{
  Trie trie;
  // người 1
  cout << "== Test insert() ==\n";
  cout << boolalpha;
  cout << "insert(\"apple\")  -> " << trie.insert("apple") << " (mong doi true)\n";
  cout << "insert(\"app\")    -> " << trie.insert("app") << " (mong doi true)\n";
  cout << "insert(\"apply\")  -> " << trie.insert("apply") << " (mong doi true)\n";
  cout << "insert(\"banana\") -> " << trie.insert("banana") << " (mong doi true)\n";
  cout << "insert(\"apple\")  -> " << trie.insert("apple") << " (mong doi false, da ton tai)\n";

  cout << "\n== Thong ke ==\n";
  cout << "wordCount = " << trie.getWordCount() << " (mong doi 4)\n";
  cout << "nodeCount = " << trie.getNodeCount() << "\n";

  cout << "\n== Test search() ==\n";
  cout << "search(\"apple\")  -> " << trie.search("apple") << " (true)\n";
  cout << "search(\"appl\")   -> " << trie.search("appl") << " (false)\n";
  cout << "search(\"grape\")  -> " << trie.search("grape") << " (false)\n";

  cout << "\n== Test startsWith() ==\n";
  cout << "startsWith(\"app\") -> " << trie.startsWith("app") << " (true)\n";
  cout << "startsWith(\"ban\") -> " << trie.startsWith("ban") << " (true)\n";
  cout << "startsWith(\"xyz\") -> " << trie.startsWith("xyz") << " (false)\n";

  cout << "\n== Test remove() ==\n";
  cout << "remove(\"app\")    -> " << trie.remove("app") << " (true)\n";
  cout << "search(\"app\")    -> " << trie.search("app") << " (false, da xoa)\n";
  cout << "search(\"apple\")  -> " << trie.search("apple") << " (true, khong bi anh huong)\n";
  cout << "startsWith(\"app\")-> " << trie.startsWith("app") << " (true, vi con apple/apply)\n";
  cout << "wordCount = " << trie.getWordCount() << " (mong doi 3)\n";

  cout << "\n== Test remove() tu khong ton tai ==\n";
  cout << "remove(\"orange\") -> " << trie.remove("orange") << " (false)\n";

  // người 2
  cout << "\n=============================================================";
  cout << "\n== Test insertBatch() ==\n";
  vector<string> batch =
  {
      "cat",
      "car",
      "cart",
      "care",
      "camera",
      "cake"
  };

  int inserted = trie.insertBatch(batch);

  cout << "So tu duoc them = "
            << inserted
            << endl;

  cout << "\n== Test autocomplete() - Lan 1 ==\n";

  AutocompleteResult result1 =
      trie.autocomplete("ca", 5);

  cout << "From cache: "
            << result1.fromCache
            << endl;

  cout << "Search time: "
            << result1.searchTimeMs
            << " ms\n";

  for (const auto& w : result1.words)
  {
      cout << w.word << " (" << w.frequency << ")" << endl;
  }

  cout << "\n== Test autocomplete() - Lan 2 ==\n";

  AutocompleteResult result2 =
      trie.autocomplete("ca", 5);

  cout << "From cache: "
            << result2.fromCache
            << endl;

  cout << "Search time: "
            << result2.searchTimeMs
            << " ms\n";

  for (const auto& w : result2.words)
  {
      cout
          << w.word
          << " ("
          << w.frequency
          << ")"
          << endl;
  }

  cout << "\n== Tang tan suat tu 'car' ==\n";

  trie.insert("car");
  trie.insert("car");
  trie.insert("car");

  AutocompleteResult result3 =
      trie.autocomplete("ca", 5);

  cout << "From cache: "
            << result3.fromCache
            << endl;

  for (const auto& w : result3.words)
  {
      cout << w.word
          << " ("
          << w.frequency
          << ")"
          << endl;
  }
  return 0;
}