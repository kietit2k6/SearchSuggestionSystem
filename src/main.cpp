/*
 * SearchSuggestion — Google-like desktop search engine
 * ────────────────────────────────────────────────────
 * Features:
 *  • Loads /usr/share/dict/words with real-time progress bar (console)
 *  • Restores saved Trie + analytics from disk on startup
 *  • Graphical User Interface built with Dear ImGui + GLFW + OpenGL3
 *  • Real-time autocomplete via Trie prefix cache + LRU (O(1))
 *  • Recency-weighted ranking: recently searched words rank higher
 *  • Colored visual indicators: green for recently searched, dimmed for dict-only
 *  • Keyboard selection: [↑/↓] to select recommendations, [TAB] to complete
 *  • History sidebar: click any past query to re-search instantly
 *  • Analytics Dashboard tab: top-10 searches histogram + hourly sparkline
 *  • Anti-spam frequency rate-limiting + atomic save backups (.tmp / .bak)
 */

#include "GuiApp.h"

#include <chrono>
#include <fstream>
#include <iostream>
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
    static const std::string kSaveFile      = "trie_data.bin";
    static const std::string kAnalyticsFile = "trie_data.bin.analytics";
    static const std::string kDictFile      = "/usr/share/dict/words";
    static const int         kAutoSaveSec   = 60;
    static const std::size_t kCacheSize     = 512;

    // Clear console and print startup header
    std::cout << "\033[2J\033[H\n"
              << StartTerm::BOLD << StartTerm::FG_CYAN
              << "  \xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x97\n"
              << "  \xe2\x95\x91" << StartTerm::RESET
              << "       SearchSuggestion \xe2\x80\x94 Google-like GUI        "
              << StartTerm::FG_CYAN << "\xe2\x95\x91\n"
              << "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                 "\xe2\x95\x9d\n"
              << StartTerm::RESET << "\n";

    Trie      trie(kCacheSize);
    AppStats  stats;
    Analytics analytics;

    // ── Restore saved trie from disk ──────────────────────────────────────
    {
        std::ifstream probe(kSaveFile, std::ios::binary);
        if (probe.is_open()) {
            probe.close();
            std::cout << "  " << StartTerm::FG_YELLOW << "\xe2\x96\xb6" << StartTerm::RESET
                      << " Restoring from " << kSaveFile << " ... " << std::flush;
            bool ok = trie.loadFromDisk(kSaveFile);
            std::cout << (ok ? StartTerm::FG_BGREEN : StartTerm::FG_RED)
                      << (ok ? "OK" : "FAILED") << StartTerm::RESET
                      << "  (" << trie.getWordCount() << " words)\n";
        }
    }

    // ── Restore saved analytics ───────────────────────────────────────────
    analytics.loadFromDisk(kAnalyticsFile);

    // ── Load system dictionary ────────────────────────────────────────────
    std::cout << "  " << StartTerm::FG_YELLOW << "\xe2\x96\xb6" << StartTerm::RESET
              << " Loading dictionary from " << kDictFile << "\n";

    int loaded = loadDictFile(trie, kDictFile,
                              [](int n, int est) { printProgressBar(n, est); });

    std::cout << "\r  \033[K"; // clear progress bar line
    if (loaded > 0) {
        std::cout << "  " << StartTerm::FG_BGREEN << "\xe2\x9c\x94" << StartTerm::RESET
                  << " Dictionary: " << StartTerm::BOLD << loaded << " new words"
                  << StartTerm::RESET << "  (total: " << trie.getWordCount() << ")\n";
    } else {
        std::cout << "  " << StartTerm::FG_YELLOW << "not found" << StartTerm::RESET
                  << " \xe2\x80\x94 generating built-in word set ... " << std::flush;
        generateFallbackWords(trie);
        std::cout << StartTerm::FG_BGREEN << trie.getWordCount() << " words" << StartTerm::RESET << "\n";
    }

    // ── Save initial snapshot if file does not exist ──────────────────────
    {
        std::ifstream probe(kSaveFile, std::ios::binary);
        if (!probe.is_open()) {
            std::cout << "  " << StartTerm::FG_YELLOW << "\xe2\x96\xb6" << StartTerm::RESET
                      << " Saving initial snapshot ... " << std::flush;
            bool ok = trie.saveToDiskAtomic(kSaveFile);
            std::cout << (ok ? StartTerm::FG_BGREEN : StartTerm::FG_RED)
                      << (ok ? "Done" : "Failed") << StartTerm::RESET << "\n";
        }
    }

    std::cout << "\n  " << StartTerm::FG_BGREEN << "\xe2\x9c\x93" << StartTerm::RESET
              << " Ready \xe2\x80\x94 " << StartTerm::BOLD << trie.getWordCount() << " words"
              << StartTerm::RESET << ", " << trie.getNodeCount() << " nodes\n\n";

    std::cout << "  " << StartTerm::FG_GRAY << "Launching Graphical Interface..." << StartTerm::RESET << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Initialize systems
    AutoSaver saver(trie, analytics, kSaveFile, kAnalyticsFile, kAutoSaveSec);

    // Initialize and run GUI App window
    GuiApp ui(trie, stats, saver, analytics, kSaveFile);
    if (ui.init()) {
        ui.run();
    } else {
        std::cerr << "Fatal: Failed to initialize GUI application window.\n";
        return 1;
    }

    return 0;
}
