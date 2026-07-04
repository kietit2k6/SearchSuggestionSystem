/*
 * SearchSuggestion — Google-like desktop search engine
 * ────────────────────────────────────────────────────
 */

#include "GuiApp.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

// Simple console styling shorthand for startup banner
namespace StartTerm {
    static constexpr const char* RESET     = "\033[0m";
    static constexpr const char* BOLD      = "\033[1m";
    static constexpr const char* FG_CYAN   = "\033[36m";
    static constexpr const char* FG_YELLOW = "\033[33m";
    static constexpr const char* FG_BGREEN = "\033[92m";
    static constexpr const char* FG_RED    = "\033[31m";
    static constexpr const char* FG_GRAY   = "\033[90m";
}

// Configuration details
struct Config {
    std::string trieFile = "trie_data.bin";
    std::string analyticsFile = "trie_data.bin.analytics";
    std::string dictFile = "words.txt";
    int autoSaveInterval = 60;
    int cacheSize = 512;
};

static void writeDefaultConfig(const std::string& path) {
    std::ofstream f(path);
    if (!f) return;
    f << "# SearchSuggestion Configuration File\n"
      << "trie_file=trie_data.bin\n"
      << "analytics_file=trie_data.bin.analytics\n"
      << "dict_file=words.txt\n"
      << "auto_save_interval=60\n"
      << "cache_size=512\n";
}

static Config loadConfig(const std::string& path) {
    Config cfg;
    std::ifstream f(path);
    if (!f) {
        writeDefaultConfig(path);
        return cfg;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (key == "trie_file") cfg.trieFile = val;
            else if (key == "analytics_file") cfg.analyticsFile = val;
            else if (key == "dict_file") cfg.dictFile = val;
            else if (key == "auto_save_interval") cfg.autoSaveInterval = std::stoi(val);
            else if (key == "cache_size") cfg.cacheSize = std::stoi(val);
        }
    }
    return cfg;
}

static std::string findDictFile(const std::string& preferredPath) {
    // 1. Try preferred configuration path
    {
        std::ifstream f(preferredPath);
        if (f.is_open()) return preferredPath;
    }
    // 2. Try default local file in current directory
    {
        std::ifstream f("words.txt");
        if (f.is_open()) return "words.txt";
    }
    // 3. Try macOS/Linux system words path
    {
        std::ifstream f("/usr/share/dict/words");
        if (f.is_open()) return "/usr/share/dict/words";
    }
    return "";
}

// Progress bar (printed on the console before entering GUI mode)
static void printProgressBar(int loaded, int estimated) {
    static constexpr int kW = 32;
    int pct    = (estimated > 0) ? std::min(loaded * 100 / estimated, 99) : 50;
    int filled = pct * kW / 100;

    std::cout << "\r  [";
    for (int i = 0; i < kW; ++i) {
        std::cout << (i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91");
    }
    std::cout << "] " << pct << "%  " << loaded << " words  " << std::flush;
}

int main() {
    // Clear console and print startup header
    std::cout << "\033[2J\033[H\n"
              << StartTerm::BOLD << StartTerm::FG_CYAN
              << "  ╔══════════════════════════════════════════════════════════════╗\n"
              << "  ║       SearchSuggestion — Google-like GUI                     ║\n"
              << "  ╚══════════════════════════════════════════════════════════════╝\n"
              << StartTerm::RESET << "\n";

    Config cfg = loadConfig("config.txt");

    Trie      trie(cfg.cacheSize);
    AppStats  stats;
    Analytics analytics;

    // ── Restore saved trie from disk ──────────────────────────────────────
    {
        std::ifstream probe(cfg.trieFile, std::ios::binary);
        if (probe.is_open()) {
            probe.close();
            std::cout << "  " << StartTerm::FG_YELLOW << "▶" << StartTerm::RESET
                      << " Restoring from " << cfg.trieFile << " ... " << std::flush;
            bool ok = trie.loadFromDisk(cfg.trieFile);
            std::cout << (ok ? StartTerm::FG_BGREEN : StartTerm::FG_RED)
                      << (ok ? "OK" : "FAILED") << StartTerm::RESET
                      << "  (" << trie.getWordCount() << " words)\n";
        }
    }

    // ── Restore saved analytics ───────────────────────────────────────────
    analytics.loadFromDisk(cfg.analyticsFile);

    // ── Load system dictionary ────────────────────────────────────────────
    std::string dictPath = findDictFile(cfg.dictFile);
    if (!dictPath.empty()) {
        std::cout << "  " << StartTerm::FG_YELLOW << "▶" << StartTerm::RESET
                  << " Loading dictionary from " << dictPath << "\n";

        int loaded = loadDictFile(trie, dictPath,
                                  [](int n, int est) { printProgressBar(n, est); });

        std::cout << "\r  \033[K"; // clear progress bar line
        std::cout << "  " << StartTerm::FG_BGREEN << "✔" << StartTerm::RESET
                  << " Dictionary: " << StartTerm::BOLD << loaded << " new words"
                  << StartTerm::RESET << "  (total: " << trie.getWordCount() << ")\n";
    } else {
        std::cout << "  " << StartTerm::FG_YELLOW << "Dictionary file not found" << StartTerm::RESET
                  << " — generating built-in word set ... " << std::flush;
        generateFallbackWords(trie);
        std::cout << StartTerm::FG_BGREEN << trie.getWordCount() << " words" << StartTerm::RESET << "\n";
    }

    // ── Save initial snapshot if file does not exist ──────────────────────
    {
        std::ifstream probe(cfg.trieFile, std::ios::binary);
        if (!probe.is_open()) {
            std::cout << "  " << StartTerm::FG_YELLOW << "▶" << StartTerm::RESET
                      << " Saving initial snapshot ... " << std::flush;
            bool ok = trie.saveToDiskAtomic(cfg.trieFile);
            std::cout << (ok ? StartTerm::FG_BGREEN : StartTerm::FG_RED)
                      << (ok ? "Done" : "Failed") << StartTerm::RESET << "\n";
        }
    }

    std::cout << "\n  " << StartTerm::FG_BGREEN << "✔" << StartTerm::RESET
              << " Ready — " << StartTerm::BOLD << trie.getWordCount() << " words"
              << StartTerm::RESET << ", " << trie.getNodeCount() << " nodes\n\n";

    std::cout << "  " << StartTerm::FG_GRAY << "Launching Graphical Interface..." << StartTerm::RESET << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Initialize systems
    AutoSaver saver(trie, analytics, cfg.trieFile, cfg.analyticsFile, cfg.autoSaveInterval);
    SearchController controller(trie, stats, saver, analytics, cfg.trieFile);

    // Initialize and run GUI App window
    GuiApp ui(controller, stats, saver, analytics);
    if (ui.init()) {
        ui.run();
    } else {
        std::cerr << "Fatal: Failed to initialize GUI application window.\n";
        return 1;
    }

    // ── Save final database snapshot on exit ──────────────────────────────
    std::cout << "\n  " << StartTerm::FG_YELLOW << "▶" << StartTerm::RESET
              << " Saving final database snapshot ... " << std::flush;
    bool ok = trie.saveToDiskAtomic(cfg.trieFile);
    analytics.saveToDisk(cfg.analyticsFile);
    std::cout << (ok ? StartTerm::FG_BGREEN : StartTerm::FG_RED)
              << (ok ? "Done" : "Failed") << StartTerm::RESET << "\n";

    return 0;
}
