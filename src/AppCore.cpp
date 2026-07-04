#include "AppCore.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════
// AppStats
// ═══════════════════════════════════════════════════════════════════════════

int AppStats::uptimeSecs() const {
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startTime).count());
}
int AppStats::cacheHitPct() const {
    uint64_t t = autocompleteRequests.load();
    return (t == 0) ? 0 : static_cast<int>(autocompleteCacheHits.load() * 100 / t);
}

// ═══════════════════════════════════════════════════════════════════════════
// Analytics
// ═══════════════════════════════════════════════════════════════════════════

void Analytics::record(const std::string& word, bool isNew, bool fromCache) {
    std::lock_guard<std::mutex> lk(mu_);
    auto now = static_cast<int64_t>(std::time(nullptr));
    records_.push_back({word, now, isNew, fromCache});
    wordCounts_[word]++;
}

std::vector<std::pair<std::string, int>> Analytics::topSearches(int n) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::pair<std::string, int>> v(wordCounts_.begin(), wordCounts_.end());
    std::partial_sort(v.begin(),
                      v.begin() + std::min(n, static_cast<int>(v.size())),
                      v.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });
    if (static_cast<int>(v.size()) > n) v.resize(static_cast<size_t>(n));
    return v;
}

int Analytics::sessionTotal() const {
    std::lock_guard<std::mutex> lk(mu_);
    return static_cast<int>(records_.size());
}
int Analytics::sessionNewWords() const {
    std::lock_guard<std::mutex> lk(mu_);
    int cnt = 0;
    for (const auto& r : records_) if (r.isNewWord) cnt++;
    return cnt;
}

std::array<int, 24> Analytics::hourlyActivity() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::array<int, 24> hours{};
    auto now = static_cast<int64_t>(std::time(nullptr));
    for (const auto& r : records_) {
        int64_t age = now - r.timestamp;
        if (age >= 0 && age < 24 * 3600) {
            int hAgo = static_cast<int>(age / 3600);
            hours[23 - hAgo]++;
        }
    }
    return hours;
}

std::vector<std::string> Analytics::history() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out;
    out.reserve(records_.size());
    for (const auto& r : records_) out.push_back(r.word);
    return out;
}

bool Analytics::saveToDisk(const std::string& path) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::ofstream f(path);
    if (!f) return false;
    f << "# SearchSuggestion Chronological Analytics (timestamp\tisNew\tfromCache\tword)\n";
    for (const auto& r : records_) {
        f << r.timestamp << '\t'
          << (r.isNewWord ? 1 : 0) << '\t'
          << (r.fromCache ? 1 : 0) << '\t'
          << r.word << '\n';
    }
    return f.good();
}

bool Analytics::loadFromDisk(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::lock_guard<std::mutex> lk(mu_);
    records_.clear();
    wordCounts_.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Count tabs to differentiate formats
        size_t tabCount = 0;
        for (char c : line) if (c == '\t') tabCount++;
        
        if (tabCount >= 3) {
            std::stringstream ss(line);
            std::string tsStr, isNewStr, fromCacheStr, word;
            if (std::getline(ss, tsStr, '\t') &&
                std::getline(ss, isNewStr, '\t') &&
                std::getline(ss, fromCacheStr, '\t') &&
                std::getline(ss, word)) {
                try {
                    SearchRecord r;
                    r.timestamp = std::stoll(tsStr);
                    r.isNewWord = (isNewStr == "1");
                    r.fromCache = (fromCacheStr == "1");
                    r.word = word;
                    records_.push_back(r);
                    wordCounts_[word]++;
                } catch (...) {}
            }
        } else if (tabCount == 1) {
            size_t tab = line.find('\t');
            if (tab != std::string::npos) {
                try {
                    std::string word = line.substr(0, tab);
                    int count = std::stoi(line.substr(tab + 1));
                    wordCounts_[word] = count;
                    int64_t now = static_cast<int64_t>(std::time(nullptr));
                    for (int j = 0; j < count; ++j) {
                        records_.push_back({word, now, false, false});
                    }
                } catch (...) {}
            }
        }
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// AutoSaver
// ═══════════════════════════════════════════════════════════════════════════

AutoSaver::AutoSaver(Trie& trie, Analytics& analytics,
                     std::string trieFile, std::string analyticsFile,
                     int intervalSecs)
    : trie_(trie), analytics_(analytics),
      trieFile_(std::move(trieFile)), analyticsFile_(std::move(analyticsFile)),
      interval_(intervalSecs), running_(true) {
    thread_ = std::thread([this] { loop(); });
}

AutoSaver::~AutoSaver() {
    { std::lock_guard<std::mutex> lk(mu_); running_ = false; }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();

    // Best-effort final save: protects data accumulated since the last
    // auto-save tick in case the caller (main) crashes before its own save.
    trie_.saveToDiskAtomic(trieFile_);
    analytics_.saveToDisk(analyticsFile_);
}

void AutoSaver::triggerNow() { cv_.notify_all(); }

std::string AutoSaver::lastSaveInfo() const {
    std::lock_guard<std::mutex> lk(mu_);
    return lastInfo_;
}

static std::tm safeLocalTime(std::time_t t) {
    std::tm tm_res;
#if defined(_WIN32)
    localtime_s(&tm_res, &t);
#else
    localtime_r(&t, &tm_res);
#endif
    return tm_res;
}

void AutoSaver::loop() {
    while (true) {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait_for(lk, std::chrono::seconds(interval_), [this] { return !running_; });
        if (!running_) break;
        lk.unlock();

        bool ok = trie_.saveToDiskAtomic(trieFile_);
        analytics_.saveToDisk(analyticsFile_);

        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char buf[12];
        std::tm tm_val = safeLocalTime(t);
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_val);

        std::lock_guard<std::mutex> lk2(mu_);
        lastInfo_ = ok ? (std::string(buf) + "  \033[92m✓\033[0m")
                       : "\033[31mfailed\033[0m";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// InputValidator
// ═══════════════════════════════════════════════════════════════════════════

InputValidator::Result InputValidator::validate(char c, const std::string& q) {
    if (static_cast<int>(q.size()) >= kMaxLen)
        return {false, "Max 50 characters"};

    unsigned char uc = static_cast<unsigned char>(c);
    if (uc < 32 || uc == 127)
        return {false, "Control character"};
    // Allow UTF-8 multi-byte continuation/leading bytes through
    if (uc >= 128) return {true, nullptr};
    // Printable ASCII: allow letters, digits, space, hyphen, apostrophe, underscore
    if (std::isalnum(uc) || c == ' ' || c == '-' || c == '\'' || c == '_')
        return {true, nullptr};
    return {false, "Only letters, digits, spaces and hyphens"};
}

InputValidator::Result InputValidator::validateWord(const std::string& word) {
    if (word.empty()) return {false, "Empty word"};
    if (static_cast<int>(word.size()) > kMaxLen) return {false, "Too long (max 50)"};
    for (char c : word) {
        auto r = validate(c, "");
        if (!r.ok) return r;
    }
    return {true, nullptr};
}

// ═══════════════════════════════════════════════════════════════════════════
// Dictionary loading
// ═══════════════════════════════════════════════════════════════════════════

int loadDictFile(Trie& trie, const std::string& path,
                 std::function<void(int, int)> onProgress) {
    std::ifstream f(path);
    if (!f) return 0;

    // Estimate total lines from file size (avg ~7 chars/word in /usr/share/dict/words)
    f.seekg(0, std::ios::end);
    auto fileSize  = static_cast<int>(f.tellg() / 7);
    f.seekg(0);

    static constexpr int kBatchSize = 10'000;
    std::vector<std::string> batch;
    batch.reserve(kBatchSize);

    std::string line;
    int total = 0;

    while (std::getline(f, line)) {
        if (line.empty() || line.size() > 48) continue;

        // Accept full UTF-8 words: only reject C0/C1 control characters
        // (bytes 0x00-0x1F and 0x7F-0x9F). Bytes >= 0x80 are valid UTF-8
        // continuation/leading bytes handled by Trie's fallback map.
        bool ok = true;
        for (unsigned char c : line) {
            if (c < 0x20 || c == 0x7F) { ok = false; break; }
        }
        if (!ok) continue;

        batch.push_back(line);

        if (static_cast<int>(batch.size()) >= kBatchSize) {
            total += trie.insertBatchNoRebuild(batch);
            batch.clear();
            if (onProgress) onProgress(total, fileSize);
        }
    }

    if (!batch.empty()) {
        total += trie.insertBatchNoRebuild(batch);
        if (onProgress) onProgress(total, fileSize);
    }

    // Build all suggestion caches once at the end
    trie.rebuildAll();
    return total;
}

void generateFallbackWords(Trie& trie) {
    static const std::vector<std::string> kWords = {
        "the","be","to","of","and","a","in","that","have","it",
        "for","not","on","with","he","as","you","do","at","this",
        "but","his","by","from","they","we","say","her","she","or",
        "an","will","my","one","all","would","there","their","what",
        "so","up","out","if","about","who","get","which","go","me",
        "when","make","can","like","time","no","just","him","know",
        "take","people","into","year","your","good","some","could",
        "them","see","other","than","then","now","look","only","come",
        "its","over","think","also","back","after","use","two","how",
        "our","work","first","well","way","even","new","want","because",
        "any","these","give","day","most","us",
        "algorithm","database","framework","interface","javascript",
        "kubernetes","machine","network","object","package","programming",
        "protocol","python","quantum","recursion","software","terminal",
        "typescript","unicode","variable","webpack","backend","frontend",
        "docker","container","microservice","serverless","artificial",
        "intelligence","neural","learning","training","inference","tensor",
        "gradient","descent","epoch","autocomplete","suggestion","prediction",
        "search","engine","query","index","cache","hash","binary","tree",
        "graph","linked","queue","stack","sorting","merging","encoding",
        "computer","keyboard","monitor","processor","memory","storage",
        "application","program","function","class","method","property",
        "string","integer","boolean","array","vector","pointer","reference",
        "compile","execute","runtime","debug","error","exception","handle",
        "thread","process","mutex","atomic","concurrent","parallel","async",
        "request","response","server","client","api","rest","http","https",
        "json","xml","html","css","react","angular","vue","node","express",
        "spring","django","flask","rails","laravel","symfony","fastapi",
        "linux","windows","macos","ubuntu","debian","android","ios","safari",
        "chrome","firefox","browser","extension","plugin","module","library",
        "github","gitlab","bitbucket","commit","branch","merge","rebase",
        "deploy","pipeline","cicd","devops","agile","scrum","sprint","kanban",
        "security","authentication","authorization","password","token","jwt",
        "encryption","decryption","certificate","ssl","tls","firewall","vpn",
        "people","place","thing","world","life","day","way","time","year",
        "hand","part","child","eye","woman","man","case","week","company",
        "system","question","government","number","night","point","home",
        "water","room","mother","area","money","story","fact","month","lot",
        "right","study","book","job","word","business","issue","side","kind",
        "head","house","service","friend","father","power","hour","game",
        "line","end","great","little","own","old","big","high","different",
        "small","large","next","early","young","important","public","private",
        "real","best","free","able","human","local","sure","strong","true",
    };
    static const std::vector<std::string> kSuffixes = {
        "","s","ed","ing","er","est","ly","ness","tion","ment",
        "able","ful","less","ive","ous","al","ic","ize","re","un","pre","pro",
    };
    std::vector<std::string> all;
    all.reserve(kWords.size() * 4);
    for (const auto& w : kWords) {
        all.push_back(w);
        for (const auto& s : kSuffixes) {
            std::string v = w + s;
            if (v.size() >= 3 && v.size() <= 24 && v != w)
                all.push_back(std::move(v));
        }
    }
    trie.insertBatch(all);
}

// ═══════════════════════════════════════════════════════════════════════════
// SearchController
// ═══════════════════════════════════════════════════════════════════════════

SearchController::SearchController(Trie& trie, AppStats& stats, AutoSaver& saver,
                                   Analytics& analytics, const std::string& saveFile)
    : trie_(trie), stats_(stats), saver_(saver),
      analytics_(analytics), saveFile_(saveFile) {
    submitHistory_ = analytics_.history();
}

void SearchController::refreshSuggestions(const std::string& query) {
    if (query.empty()) {
        suggestions_.clear();
        fuzzyResults_.clear();
        return;
    }

    auto res = trie_.autocomplete(query, 8);
    suggestions_ = res.words;
    stats_.autocompleteRequests++;
    if (res.fromCache) stats_.autocompleteCacheHits++;

    if (suggestions_.empty() && query.size() >= 3) {
        fuzzyResults_ = trie_.fuzzySearch(query, 2);
        if (fuzzyResults_.size() > 6) fuzzyResults_.resize(6);
    } else {
        fuzzyResults_.clear();
    }
}

void SearchController::submitSearch(const std::string& word) {
    auto v = InputValidator::validateWord(word);
    if (!v.ok) {
        statusMsg_ = std::string("⚠ ") + (v.reason ? v.reason : "Invalid query!");
        return;
    }

    // Rate-limit is enforced solely inside Trie::searchAndUpdate via the
    // per-node lastAccessTime field + kMinFreqIntervalSecs. We do NOT duplicate
    // that check here; the SearchResult::incremented flag tells us whether the
    // Trie actually counted this submission or silently skipped it.

    auto now = static_cast<int64_t>(std::time(nullptr));
    lastSearch_     = word;
    lastFreqBefore_ = trie_.getFrequency(word);

    auto result = trie_.searchAndUpdate(word, now);
    if (result.found) {
        lastFreqBefore_ = result.freqBefore;
        isNewWord_      = false;
        if (!result.incremented) {
            // Word found but rate-limited — inform the user without blocking.
            // Use lastAccessBefore captured *before* the node was updated,
            // so elapsed is accurate even when incremented == false.
            int wait = std::max(0, static_cast<int>(
                Trie::kMinFreqIntervalSecs - (now - result.lastAccessBefore)));
            if (wait > 0) {
                spamMsg_ = "⚠ Rate limited — please wait " + std::to_string(wait) + "s";
                return;
            }
        }
    } else {
        if (trie_.insert(word)) {
            stats_.newWordsLearned++;
            isNewWord_ = true;
        } else {
            isNewWord_ = false;
        }
    }

    analytics_.record(word, isNewWord_, false);

    // Save in history queue
    auto histIt = std::find(submitHistory_.begin(), submitHistory_.end(), word);
    if (histIt != submitHistory_.end()) {
        submitHistory_.erase(histIt);
    }
    submitHistory_.push_back(word);
    if (submitHistory_.size() > 100) {
        submitHistory_.erase(submitHistory_.begin());
    }

    statusMsg_.clear();
    stats_.totalSearches++;

    static uint64_t submitCount = 0;
    if (++submitCount % 50 == 0) saver_.triggerNow();
}

void SearchController::handleCtrlS() {
    bool ok = trie_.saveToDiskAtomic(saveFile_);
    analytics_.saveToDisk(saveFile_ + ".analytics");
    saver_.triggerNow();
    statusMsg_ = ok ? "Saved successfully!" : "Failed to save data!";
}
