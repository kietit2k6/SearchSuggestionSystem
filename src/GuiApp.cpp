#include "GuiApp.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <ctime>
#include <iostream>


// ═══════════════════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

GuiApp::GuiApp(Trie& trie, AppStats& stats, AutoSaver& saver,
               Analytics& analytics, const std::string& saveFile)
    : trie_(trie), stats_(stats), saver_(saver),
      analytics_(analytics), saveFile_(saveFile) {
    // Populate session history cache from saved analytics
    submitHistory_ = analytics_.history();
}

GuiApp::~GuiApp() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════
// Initialize GLFW + OpenGL + ImGui
// ═══════════════════════════════════════════════════════════════════════════

bool GuiApp::init() {
    // 1. GLFW Setup
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    // Modern OpenGL settings (Mac requires core profile 3.2+)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window_ = glfwCreateWindow(windowWidth_, windowHeight_,
                               "Search Suggestion Engine (ImGui)", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable VSync

    // 2. ImGui Setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Keyboard navigation

    // Apply premium styling
    setupDarkTheme();

    // Bind Backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    return true;
}

// Helper to destroy windows and terminate library
void GuiApp::shutdown() {
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Styling Setup (Dark Mode with Accent Colors)
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::setupDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.18f, 0.48f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.18f, 0.48f, 0.90f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.48f, 0.90f, 0.70f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.18f, 0.48f, 0.90f, 0.90f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.18f, 0.48f, 0.90f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.48f, 0.90f, 0.30f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.18f, 0.48f, 0.90f, 0.70f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.18f, 0.48f, 0.90f, 0.90f);
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.18f, 0.48f, 0.90f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.18f, 0.48f, 0.90f, 1.00f);

    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
}

// ═══════════════════════════════════════════════════════════════════════════
// GUI Rendering Loop
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::run() {
    std::string currentQuery;
    char lastQuery[128] = "";

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Start new ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ── Keyboard Check: detect query changes live ─────────────────────
        if (std::strcmp(queryBuf_, lastQuery) != 0) {
            std::strcpy(lastQuery, queryBuf_);
            currentQuery = queryBuf_;
            validationMsg_.clear();
            spamMsg_.clear();

            if (currentQuery.empty()) {
                suggestions_.clear();
                fuzzyResults_.clear();
            } else {
                refreshSuggestions();
            }
        }

        // Full Screen layout window
        int w, h;
        glfwGetFramebufferSize(window_, &w, &h);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)));
        ImGui::Begin("Workspace", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_MenuBar);

        drawMenuBar();

        // Navigation Tabs
        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Search Workspace")) {
                drawSearchTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Analytics Dashboard")) {
                drawDashboardTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // Floating overlays
        if (showHelp_) drawHelpOverlay();

        ImGui::End();

        // ── Render ────────────────────────────────────────────────────────
        ImGui::Render();
        glViewport(0, 0, w, h);
        glClearColor(0.09f, 0.09f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Menu Bar
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::drawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save snapshot", "Ctrl+S")) {
                handleCtrlS();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window_, true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Usage Guide", "F1")) {
                showHelp_ = !showHelp_;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Tab 1: Search Workspace
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::drawSearchTab() {
    ImGui::Spacing();

    // 2-Column Layout: Search (Left: 70%) | History Sidebar (Right: 30%)
    ImGui::Columns(2, "WorkspaceColumns", true);

    // Make sure columns are set reasonably
    static bool setWidth = true;
    if (setWidth) {
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.70f);
        setWidth = false;
    }

    // ── LEFT COLUMN: Search Area ──────────────────────────────────────────
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "🔍 Smart Query Bar");
    ImGui::SetNextItemWidth(-1.0f);

    // Call ImGui InputText with flags supporting ENTER key, completion callbacks
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackCompletion |
                                ImGuiInputTextFlags_CallbackHistory;

    bool inputSubmitted = ImGui::InputText("##SearchField", queryBuf_, sizeof(queryBuf_),
                                           flags, inputTextCallback, this);

    if (inputSubmitted && std::strlen(queryBuf_) > 0) {
        // ENTER key submitted
        std::string word = queryBuf_;
        if (selectedIdx_ >= 0 && selectedIdx_ < static_cast<int>(suggestions_.size())) {
            word = suggestions_[selectedIdx_].word;
        }
        submitSearch(word);
        queryBuf_[0] = '\0';
        selectedIdx_ = -1;
        refreshSuggestions();
    }

    // Instant validation feedback text
    if (!validationMsg_.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "⚠ %s", validationMsg_.c_str());
    } else if (!spamMsg_.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "%s", spamMsg_.c_str());
    } else if (historyMode_) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "↺ History recall index [%d/%d]",
                           historyIdx_, static_cast<int>(submitHistory_.size()));
    } else {
        ImGui::TextDisabled("Use [↑/↓] to browse recommendations, [TAB] to complete, [ENTER] to confirm.");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ── Autocomplete / Suggestions list ──────────────────────────────────
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "💡 Autocomplete Recommendations");

    ImGui::BeginChild("SuggestionsBox", ImVec2(0, 240), true);

    if (!suggestions_.empty()) {
        int maxFreq = 1;
        for (const auto& s : suggestions_) maxFreq = std::max(maxFreq, s.frequency);

        for (int i = 0; i < static_cast<int>(suggestions_.size()); ++i) {
            renderSuggestionRow(suggestions_[i], i, maxFreq, false);
        }
    } else if (!fuzzyResults_.empty()) {
        int maxFreq = 1;
        for (const auto& r : fuzzyResults_) maxFreq = std::max(maxFreq, r.frequency);

        for (int i = 0; i < static_cast<int>(fuzzyResults_.size()); ++i) {
            WordEntry we{fuzzyResults_[i].word, fuzzyResults_[i].frequency, 0};
            renderSuggestionRow(we, i, maxFreq, true, fuzzyResults_[i].editDistance);
        }
    } else {
        if (std::strlen(queryBuf_) == 0) {
            ImGui::TextDisabled("Type something to display autocomplete suggestions...");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
                               "No match found. Press ENTER to learn \"%s\"!", queryBuf_);
        }
    }
    ImGui::EndChild();

    // ── Status banner ─────────────────────────────────────────────────────
    ImGui::Spacing();
    if (!lastSearch_.empty()) {
        int freq = trie_.getFrequency(lastSearch_);
        if (isNewWord_) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                               "✦ Learned: \"%s\" (initial frequency = 1)", lastSearch_.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                               "✓ Searched: \"%s\"  [Freq: %d → %d]",
                               lastSearch_.c_str(), lastFreqBefore_, freq);
        }
    } else if (!statusMsg_.empty()) {
        ImGui::Text("%s", statusMsg_.c_str());
    }

    // ── RIGHT COLUMN: Sidebar (Search History) ────────────────────────────
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "↺ Search History");

    ImGui::BeginChild("HistoryBox", ImVec2(0, 360), true);
    if (submitHistory_.empty()) {
        ImGui::TextDisabled("No searches registered yet.");
    } else {
        // Show reverse chronological history
        for (auto it = submitHistory_.rbegin(); it != submitHistory_.rend(); ++it) {
            std::string hWord = *it;
            // Draw a clickable button for history recall
            if (ImGui::Button(hWord.c_str(), ImVec2(-1.0f, 0))) {
                submitSearch(hWord);
            }
        }
    }
    ImGui::EndChild();

    // Restore column layout
    ImGui::Columns(1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Tab 2: Stats Dashboard
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::drawDashboardTab() {
    ImGui::Spacing();

    // 1. Key Metrics Cards
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "📈 Engine Performance Metrics");

    ImGui::BeginChild("MetricsCard", ImVec2(0, 110), true);
    ImGui::Columns(4, "StatsColumns", false);

    // Column 1
    ImGui::Text("Total Searches");
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "%llu", stats_.totalSearches.load());
    ImGui::NextColumn();

    // Column 2
    ImGui::Text("New Words Learned");
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%llu", stats_.newWordsLearned.load());
    ImGui::NextColumn();

    // Column 3
    ImGui::Text("Cache Hit Rate");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%d%%", stats_.cacheHitPct());
    ImGui::NextColumn();

    // Column 4
    ImGui::Text("Trie Dictionary Size");
    ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.1f, 1.0f), "%d words", trie_.getWordCount());

    ImGui::Columns(1);
    ImGui::Separator();

    auto up  = std::to_string(stats_.uptimeSecs()) + "s";
    auto sv  = saver_.lastSaveInfo();
    ImGui::Text("System Uptime: %s | Auto-save status: %s", up.c_str(), sv.c_str());

    ImGui::EndChild();
    ImGui::Spacing();

    // 2. Charts and Visualization
    ImGui::Columns(2, "DashboardCharts", true);

    // Left Chart: Top Searches (Histogram)
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "🔥 Top 10 Searches");
    auto topSearched = analytics_.topSearches(10);
    if (topSearched.empty()) {
        ImGui::TextDisabled("No search data recorded yet.");
    } else {
        std::vector<float> values;
        std::vector<std::string> labels;
        float maxVal = 0.0f;

        for (const auto& [w, cnt] : topSearched) {
            values.push_back(static_cast<float>(cnt));
            labels.push_back(w + " (" + std::to_string(cnt) + ")");
            if (cnt > maxVal) maxVal = static_cast<float>(cnt);
        }

        // Draw nice ImGui PlotHistogram
        ImGui::PlotHistogram("##TopSearchesPlot", values.data(), static_cast<int>(values.size()),
                             0, nullptr, 0.0f, maxVal * 1.2f, ImVec2(-1.0f, 180.0f));

        // Legend / labels below histogram
        ImGui::BeginChild("LegendBox", ImVec2(0, 100), false);
        for (size_t i = 0; i < labels.size(); ++i) {
            ImGui::Text("%d. %s", static_cast<int>(i + 1), labels[i].c_str());
        }
        ImGui::EndChild();
    }

    ImGui::NextColumn();

    // Right Chart: Hourly activity graph
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "⚡ Hourly Search Volume (Last 24 Hours)");
    auto hours = analytics_.hourlyActivity();
    std::vector<float> activityData;
    float maxActivity = 1.0f;
    for (int h : hours) {
        float fval = static_cast<float>(h);
        activityData.push_back(fval);
        if (fval > maxActivity) maxActivity = fval;
    }

    ImGui::PlotLines("##HourlyActivityPlot", activityData.data(), static_cast<int>(activityData.size()),
                     0, nullptr, 0.0f, maxActivity * 1.2f, ImVec2(-1.0f, 180.0f));

    ImGui::TextDisabled("24 hours ago                                             Current Hour");

    ImGui::Columns(1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Help Dialog Overlay
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::drawHelpOverlay() {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Help & Documentation", &showHelp_);
    ImGui::TextWrapped("Search Suggestion Engine Manual:\n\n"
                       "• Type characters in the search bar. Autocomplete results appear below instantly.\n"
                       "• Select a result using the Up and Down keys, or click them directly with your mouse.\n"
                       "• Press TAB to fill the search query with the selected suggestion.\n"
                       "• Press ENTER to search/learn. If the query does not exist in the dictionary, "
                       "the engine will insert it and set its initial frequency count to 1.\n"
                       "• Clicking any word in the history column on the right will immediately execute a search.\n"
                       "• The engine rate-limits incrementing frequencies to protect against query spam.\n"
                       "• View graphs and charts in the Analytics Tab.");
    ImGui::Spacing();
    if (ImGui::Button("Close")) showHelp_ = false;
    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// Suggestion list item rendering
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::renderSuggestionRow(const WordEntry& e, int idx, int maxFreq,
                                 bool isFuzzy, int editDist) {
    bool selected = (idx == selectedIdx_);

    // Color theme highlight logic:
    // Bright cyan/green for recently searched, white for common, dimmed gray for dict-only
    auto now = static_cast<int64_t>(std::time(nullptr));
    bool recentlySearched = (e.lastAccess > 0 && (now - e.lastAccess) < 86400); // within 24h
    bool everSearched     = (e.frequency > 0);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

    // Choose row colors
    ImVec4 textColor = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    if (selected) {
        textColor = ImVec4(1.0f, 0.9f, 0.2f, 1.0f); // Bright yellow selection
    } else if (recentlySearched) {
        textColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Bright green
    } else if (!everSearched) {
        textColor = ImVec4(0.5f, 0.5f, 0.5f, 1.00f); // Dimmed gray
    }

    // Clickable selectable item row
    char label[128];
    if (isFuzzy) {
        std::snprintf(label, sizeof(label), "  %s  (dist: %d)##%d", e.word.c_str(), editDist, idx);
    } else {
        std::snprintf(label, sizeof(label), "  %s%s##%d", e.word.c_str(), (recentlySearched ? "  [★]" : ""), idx);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    if (ImGui::Selectable(label, selected)) {
        selectedIdx_ = idx;
        std::strcpy(queryBuf_, e.word.c_str());
        submitSearch(e.word);
        queryBuf_[0] = '\0';
        selectedIdx_ = -1;
        refreshSuggestions();
    }
    ImGui::PopStyleColor();

    // Side-by-side progress bar indicating search frequency weight
    ImGui::SameLine(300.0f);
    float barWeight = static_cast<float>(e.frequency) / static_cast<float>(maxFreq);
    char progressLabel[32];
    std::snprintf(progressLabel, sizeof(progressLabel), "%d", e.frequency);
    ImGui::SetNextItemWidth(120.0f);

    // Colored progress bars
    ImVec4 progressCol = ImVec4(0.18f, 0.48f, 0.90f, 0.80f);
    if (recentlySearched) progressCol = ImVec4(0.2f, 0.8f, 0.2f, 0.80f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressCol);
    ImGui::ProgressBar(barWeight, ImVec2(0.0f, 0.0f), progressLabel);
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
}

// ═══════════════════════════════════════════════════════════════════════════
// UI Logic Helpers
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::refreshSuggestions() {
    std::string q = queryBuf_;
    if (q.empty()) {
        suggestions_.clear();
        fuzzyResults_.clear();
        return;
    }

    auto res = trie_.autocomplete(q, 8);
    suggestions_ = res.words;
    stats_.totalSearches++;
    if (res.fromCache) stats_.cacheHits++;

    if (suggestions_.empty() && q.size() >= 3) {
        fuzzyResults_ = trie_.fuzzySearch(q, 2);
        if (fuzzyResults_.size() > 6) fuzzyResults_.resize(6);
    } else {
        fuzzyResults_.clear();
    }
}

void GuiApp::submitSearch(const std::string& word) {
    auto v = InputValidator::validateWord(word);
    if (!v.ok) {
        statusMsg_ = std::string("⚠ ") + (v.reason ? v.reason : "Invalid query!");
        return;
    }

    auto now = static_cast<int64_t>(std::time(nullptr));
    auto it  = lastSearchedAt_.find(word);
    if (it != lastSearchedAt_.end() &&
        (now - it->second) < static_cast<int64_t>(Trie::kMinFreqIntervalSecs)) {
        int wait = static_cast<int>(Trie::kMinFreqIntervalSecs - (now - it->second));
        spamMsg_ = "⚠ Rate limited — please wait " + std::to_string(wait) + "s";
        return;
    }
    lastSearchedAt_[word] = now;

    lastSearch_     = word;
    lastFreqBefore_ = trie_.getFrequency(word);

    auto result = trie_.searchAndUpdate(word, now);
    if (result.found) {
        lastFreqBefore_ = result.freqBefore;
        isNewWord_      = false;
    } else {
        trie_.insert(word);
        stats_.newWordsLearned++;
        isNewWord_ = true;
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

void GuiApp::handleCtrlS() {
    bool ok = trie_.saveToDiskAtomic(saveFile_);
    analytics_.saveToDisk(saveFile_ + ".analytics");
    saver_.triggerNow();
    statusMsg_ = ok ? "Saved successfully!" : "Failed to save data!";
}

// ═══════════════════════════════════════════════════════════════════════════
// ImGui Callback: handles input keyboard captures (Completion, History UP/DOWN)
// ═══════════════════════════════════════════════════════════════════════════

int GuiApp::inputTextCallback(ImGuiInputTextCallbackData* data) {
    GuiApp* app = static_cast<GuiApp*>(data->UserData);
    if (!app) return 0;

    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCompletion: {
        // Tab completion
        std::string word;
        if (app->selectedIdx_ >= 0 && app->selectedIdx_ < static_cast<int>(app->suggestions_.size())) {
            word = app->suggestions_[app->selectedIdx_].word;
        } else if (!app->suggestions_.empty()) {
            word = app->suggestions_[0].word;
        }

        if (!word.empty()) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, word.c_str());
            app->selectedIdx_ = -1;
            app->historyMode_ = false;
        }
        break;
    }
    case ImGuiInputTextFlags_CallbackHistory: {
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (data->BufTextLen == 0 || app->historyMode_) {
                // Navigate history
                if (!app->submitHistory_.empty()) {
                    app->historyIdx_ = std::min(app->historyIdx_ + 1, static_cast<int>(app->submitHistory_.size()));
                    std::string hWord = app->submitHistory_[app->submitHistory_.size() - app->historyIdx_];
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, hWord.c_str());
                    app->historyMode_ = true;
                    app->selectedIdx_ = -1;
                }
            } else if (!app->suggestions_.empty()) {
                // Move selection up
                if (app->selectedIdx_ <= 0) {
                    app->selectedIdx_ = static_cast<int>(app->suggestions_.size()) - 1;
                } else {
                    app->selectedIdx_--;
                }
            }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (app->historyMode_) {
                app->historyIdx_--;
                if (app->historyIdx_ <= 0) {
                    app->historyIdx_ = 0;
                    app->historyMode_ = false;
                    data->DeleteChars(0, data->BufTextLen);
                } else {
                    std::string hWord = app->submitHistory_[app->submitHistory_.size() - app->historyIdx_];
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, hWord.c_str());
                }
            } else if (!app->suggestions_.empty()) {
                // Move selection down
                app->selectedIdx_ = (app->selectedIdx_ + 1) % static_cast<int>(app->suggestions_.size());
            }
        }
        break;
    }
    }
    return 0;
}
