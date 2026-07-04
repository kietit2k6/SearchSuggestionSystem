#pragma once

#include "AppCore.h"
#include "Trie.h"
#include <imgui.h>

#include <string>
#include <vector>
#include <unordered_map>

// Forward declaration of GLFWwindow
struct GLFWwindow;

class GuiApp {
public:
    GuiApp(Trie& trie, AppStats& stats, AutoSaver& saver,
           Analytics& analytics, const std::string& saveFile);
    ~GuiApp();

    // Initialize GLFW, OpenGL, and ImGui
    bool init();

    // Start rendering loop (blocks until window is closed)
    void run();

    // Static callback for ImGui InputText key events (Tab, Up/Down arrows)
    static int inputTextCallback(ImGuiInputTextCallbackData* data);

private:
    // ── Main UI Windows ───────────────────────────────────────────────────
    void drawMenuBar();
    void drawSearchTab();
    void drawDashboardTab();
    void drawHelpOverlay();

    // ── Logic Helpers ─────────────────────────────────────────────────────
    void refreshSuggestions();
    void submitSearch(const std::string& word);
    void setupDarkTheme();
    void shutdown();
    void handleCtrlS();
    void renderSuggestionRow(const WordEntry& e, int idx, int maxFreq,
                             bool isFuzzy, int editDist = 0);

    // ── References ────────────────────────────────────────────────────────
    Trie&        trie_;
    AppStats&    stats_;
    AutoSaver&   saver_;
    Analytics&   analytics_;
    std::string  saveFile_;

    // ── Window State ──────────────────────────────────────────────────────
    GLFWwindow* window_ = nullptr;
    int         windowWidth_  = 1024;
    int         windowHeight_ = 768;

    // ── Active State ──────────────────────────────────────────────────────
    char                     queryBuf_[128] = "";
    int                      selectedIdx_   = -1;
    std::vector<WordEntry>   suggestions_;
    std::vector<FuzzyResult> fuzzyResults_;
    bool                     showHelp_      = false;

    // History & Navigation State
    std::vector<std::string> submitHistory_;
    bool                     historyMode_   = false;
    int                      historyIdx_    = 0;

    // Anti-Spam state
    std::unordered_map<std::string, int64_t> lastSearchedAt_;
    std::string                              spamMsg_;

    // Status notifications
    std::string lastSearch_;
    int         lastFreqBefore_ = 0;
    bool        isNewWord_      = false;
    std::string statusMsg_;
    std::string validationMsg_;

    // Blinking cursor bg (changes on focus)
    static constexpr const char* BG_CURSOR_ = "\033[48;5;27m";
};
