#include "Trie.h"
#include <algorithm>
#include <cctype>
#include <queue>
using namespace std;
// Constructor
Trie::Trie(size_t cacheCapacity)
    : root_(make_unique<TrieNode>('\0')),
      cache_(cacheCapacity)
{
  // root_ không đại diện cho ký tự nào, nodeCount_ tính luôn root = 1
  wordCount_ = 0;
  nodeCount_ = 1;
}

// normalize(): chuẩn hoá chuỗi đầu vào về chữ thường để Trie không phân
string Trie::normalize(const string &s)
{
  string result = s;
  transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
                 { return tolower(c); });
  return result;
}

bool Trie::insert(const string &word)
{
  if (word.empty())
    return false;
  string key = normalize(word);
  unique_lock<shared_mutex> lock(mutex_);
  TrieNode *current = root_.get();
  for (char c : key)
  {
    auto it = current->children.find(c);
    if (it == current->children.end())
    {
      TrieNode *newNode = new TrieNode(c);
      current->children[c] = newNode;
      ++nodeCount_;
      current = newNode;
    }
    else
    {
      current = it->second;
    }
  }
  bool isNewWord = !current->isEndOfWord;
  if (isNewWord)
  {
    current->isEndOfWord = true;
    current->word = key;
    ++wordCount_;
  }
  ++current->frequency;
  cache_.invalidate();
  return isNewWord;
}

// remove(): xoá 1 từ khỏi Trie bằng đệ quy deleteHelper().
bool Trie::remove(const string &word)
{
  if (word.empty())
    return false;
  string key = normalize(word);
  unique_lock<shared_mutex> lock(mutex_);

  // Kiểm tra từ có thực sự tồn tại không (đã giữ unique_lock nên duyệt trực tiếp)
  const TrieNode *check = root_.get();
  for (char c : key)
  {
    auto it = check->children.find(c);
    if (it == check->children.end())
      return false; // không tồn tại
    check = it->second;
  }
  if (!check->isEndOfWord)
    return false; // chỉ là prefix, không phải từ hoàn chỉnh
  // Tiến hành xoá đánh dấu + dọn node thừa
  deleteHelper(root_.get(), key, 0);
  --wordCount_;
  cache_.invalidate();
  return true;
}

// deleteHelper(): được gọi SAU KHI remove() đã xác nhận từ tồn tại.
bool Trie::deleteHelper(TrieNode *node, const string &word, int depth)
{
  if (!node)
    return false;
  // Trường hợp cơ sở: đã đi hết chuỗi word
  if (depth == static_cast<int>(word.size()))
  {
    if (!node->isEndOfWord)
      return false; // từ không tồn tại trong Trie
    node->isEndOfWord = false;
    node->word.clear();
    node->frequency = 0;
    // Nếu node này không còn con nào -> có thể xoá khỏi cha
    return node->children.empty();
  }
  char c = word[depth];
  auto it = node->children.find(c);
  if (it == node->children.end())
    return false; // không tìm thấy đường đi -> từ không tồn tại
  TrieNode *child = it->second;
  bool shouldDeleteChild = deleteHelper(child, word, depth + 1);
  if (shouldDeleteChild)
  {
    delete child;
    node->children.erase(it);
    --nodeCount_;
    return !node->isEndOfWord && node->children.empty() && node != root_.get();
  }
  return false;
}

// findNode(): duyệt theo prefix, trả về con trỏ tới node cuối cùng của
const TrieNode *Trie::findNode(const string &prefix) const
{
  const TrieNode *current = root_.get();
  string key = normalize(prefix);

  for (char c : key)
  {
    auto it = current->children.find(c);
    if (it == current->children.end())
      return nullptr;
    current = it->second;
  }
  return current;
}

// search(): tìm chính xác 1 từ có tồn tại trong Trie hay không.
bool Trie::search(const string &word) const
{
  if (word.empty())
    return false;
  shared_lock<shared_mutex> lock(mutex_);
  const TrieNode *node = findNode(word);
  return node != nullptr && node->isEndOfWord;
}

// startsWith(): kiểm tra có tồn tại ít nhất 1 từ trong Trie bắt đầu
bool Trie::startsWith(const string &prefix) const
{
  if (prefix.empty())
    return true; // mọi từ đều có chung tiền tố rỗng

  shared_lock<shared_mutex> lock(mutex_);

  return findNode(prefix) != nullptr;
}

// getWordCount() / getNodeCount(): trả về thống kê được cập nhật trực tiếp
int Trie::getWordCount() const
{
  shared_lock<shared_mutex> lock(mutex_);
  return wordCount_;
}
int Trie::getNodeCount() const
{
  shared_lock<shared_mutex> lock(mutex_);
  return nodeCount_;
}

// insertBatch
int Trie::insertBatch(const vector<string>& words)
{
    int inserted = 0;

    for (const auto& word : words)
    {
        if (insert(word))
            inserted++;
    }

    return inserted;
}

// collectTopK
void Trie::collectTopK(const TrieNode* node,
                       const string& current,
                       vector<WordEntry>& results,
                       int maxResults) const
{
    if (node == nullptr)
        return;

    if (node->isEndOfWord)
    {
        results.push_back(
        {
            current,
            node->frequency
        });
    }

    for (const auto& child : node->children)
    {
        collectTopK(
            child.second,
            current + child.first,
            results,
            maxResults
        );
    }
}

// AutocompleteResult
AutocompleteResult Trie::autocomplete(
    const string& prefix,
    int maxResults) const
{
  auto start = chrono::high_resolution_clock::now();
  shared_lock<shared_mutex> lock(mutex_);

  string key = normalize(prefix);

  auto cached = cache_.get(key);

  if (cached.has_value())
  {
    AutocompleteResult result = cached.value();
    result.fromCache = true;
    return result;
  }

  const TrieNode* node = findNode(key);

  AutocompleteResult result;

  if (node == nullptr)
      return result;

  vector<WordEntry> words;

  collectTopK(node,
              key,
              words,
              maxResults);

  auto cmp = [](const WordEntry& a,
                const WordEntry& b)
    {
      if (a.frequency != b.frequency)
          return a.frequency > b.frequency;

      return a.word < b.word;
    };

  priority_queue<
      WordEntry,
      vector<WordEntry>,
      decltype(cmp)> heap(cmp);

  // Đưa tất cả từ vào min-heap
  for (const auto& w : words)
  {
      heap.push(w);

      // Chỉ giữ lại maxResults phần tử
      if ((int)heap.size() > maxResults)
          heap.pop();
  }

  // Lấy kết quả từ heap
  vector<WordEntry> topWords;

  while (!heap.empty())
  {
      topWords.push_back(heap.top());
      heap.pop();
  }

  // Đảo ngược để frequency lớn đứng trước
  reverse(topWords.begin(), topWords.end());

  result.words = topWords;

      result.fromCache = false;

      auto end = chrono::high_resolution_clock::now();

      result.searchTimeMs =
          chrono::duration<double, milli>(end - start).count();

      cache_.put(key, result);

    return result;
}

// commonPrefix()
string Trie::commonPrefix(const string& a,
    const string& b)
{
int length = min((int)a.length(), (int)b.length());

int i = 0;

while (i < length && a[i] == b[i])
{
i++;
}

return a.substr(0, i);
}

// lcpHelper()
string Trie::lcpHelper(const vector<string>& words,
 int left,
 int right)
{
if (left == right)
{
return words[left];
}

int mid = left + (right - left) / 2;

string leftPrefix = lcpHelper(words, left, mid);

string rightPrefix = lcpHelper(words, mid + 1, right);

return commonPrefix(leftPrefix, rightPrefix);
}

// longestCommonPrefixDivideConquer()

string Trie::longestCommonPrefixDivideConquer(
const vector<string>& words)
{
if (words.empty())
{
return "";
}

return lcpHelper(words, 0, (int)words.size() - 1);
}

// longestCommonPrefix()

string Trie::longestCommonPrefix() const
{
shared_lock<shared_mutex> lock(mutex_);

string prefix;

const TrieNode* current = root_.get();

while (current != nullptr)
{
// Nếu node là kết thúc của một từ
if (current->isEndOfWord)
{
break;
}

// Nếu có nhiều hơn 1 nhánh thì dừng
if (current->children.size() != 1)
{
break;
}

auto it = current->children.begin();

prefix += it->first;

current = it->second;
}

return prefix;
}