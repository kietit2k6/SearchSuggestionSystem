#pragma once

#include "AppCore.h"
#include "Trie.h"
#include <imgui.h>

#include <string>
#include <vector>

// Forward declaration of GLFWwindow
struct GLFWwindow;

class GuiApp {
public:
    GuiApp(SearchController& controller, AppStats& stats, AutoSaver& saver, Analytics& analytics);
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

    // ── Logic/Render Helpers ──────────────────────────────────────────────
    void setupDarkTheme();
    void shutdown();
    void renderSuggestionRow(const WordEntry& e, int idx, int maxFreq,
                             bool isFuzzy, int editDist = 0);

    // ── References ────────────────────────────────────────────────────────
    SearchController& controller_;
    AppStats&         stats_;
    AutoSaver&        saver_;
    Analytics&        analytics_;

    // ── Window State ──────────────────────────────────────────────────────
    GLFWwindow* window_ = nullptr;
    int         windowWidth_  = 1024;
    int         windowHeight_ = 768;

    // ── Active UI State ───────────────────────────────────────────────────
    char                     queryBuf_[128] = "";
    bool                     showHelp_      = false;

    // Blinking cursor bg (changes on focus)
    static constexpr const char* BG_CURSOR_ = "\033[48;5;27m";
};
