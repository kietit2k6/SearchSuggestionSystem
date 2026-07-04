#include "GuiApp.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iostream>

// ═══════════════════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

GuiApp::GuiApp(SearchController& controller, AppStats& stats, AutoSaver& saver, Analytics& analytics)
    : controller_(controller), stats_(stats), saver_(saver), analytics_(analytics) {}

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

        // Handle global hotkeys
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            controller_.handleCtrlS();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
            showHelp_ = !showHelp_;
        }

        // ── Keyboard Check: detect query changes live ─────────────────────
        if (std::strcmp(queryBuf_, lastQuery) != 0) {
            std::strcpy(lastQuery, queryBuf_);
            currentQuery = queryBuf_;
            controller_.clearValidationAndSpam();

            auto v = InputValidator::validateWord(currentQuery);
            if (!v.ok && !currentQuery.empty()) {
                controller_.setValidationMsg(v.reason);
                controller_.clearSuggestions();
            } else {
                controller_.refreshSuggestions(currentQuery);
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

        // Modals / Overlays
        drawHelpOverlay();

        ImGui::End(); // Workspace

        // Render Frame
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
                controller_.handleCtrlS();
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
        const auto& suggestions = controller_.suggestions();
        int selectedIdx = controller_.selectedIdx();
        if (selectedIdx >= 0 && selectedIdx < static_cast<int>(suggestions.size())) {
            word = suggestions[selectedIdx].word;
        }
        controller_.submitSearch(word);
        queryBuf_[0] = '\0';
        controller_.selectedIdx() = -1;
        controller_.refreshSuggestions("");
    }

    // Instant validation feedback text
    const std::string& valMsg = controller_.validationMsg();
    const std::string& spam = controller_.spamMsg();
    const auto& submitHistory = controller_.submitHistory();
    if (!valMsg.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "⚠ %s", valMsg.c_str());
    } else if (!spam.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "%s", spam.c_str());
    } else if (controller_.historyMode()) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "↺ History recall index [%d/%d]",
                           controller_.historyIdx(), static_cast<int>(submitHistory.size()));
    } else {
        ImGui::TextDisabled("Use [Up/Down] to browse recommendations, [TAB] to complete, [ENTER] to confirm.");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ── Autocomplete / Suggestions list ──────────────────────────────────
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "💡 Autocomplete Recommendations");

    ImGui::BeginChild("SuggestionsBox", ImVec2(0, 240), true);

    const auto& suggestions = controller_.suggestions();
    const auto& fuzzyResults = controller_.fuzzyResults();

    if (!suggestions.empty()) {
        int maxFreq = 1;
        for (const auto& s : suggestions) maxFreq = std::max(maxFreq, s.frequency);

        for (int i = 0; i < static_cast<int>(suggestions.size()); ++i) {
            renderSuggestionRow(suggestions[i], i, maxFreq, false);
        }
    } else if (!fuzzyResults.empty()) {
        int maxFreq = 1;
        for (const auto& r : fuzzyResults) maxFreq = std::max(maxFreq, r.frequency);

        for (int i = 0; i < static_cast<int>(fuzzyResults.size()); ++i) {
            WordEntry we{fuzzyResults[i].word, fuzzyResults[i].frequency, 0};
            renderSuggestionRow(we, i, maxFreq, true, fuzzyResults[i].editDistance);
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
    const std::string& lastSearch = controller_.lastSearch();
    if (!lastSearch.empty()) {
        int freq = controller_.getWordFrequency(lastSearch);
        if (controller_.isNewWord()) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                               "✦ Learned: \"%s\" (initial frequency = 1)", lastSearch.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                               "✓ Searched: \"%s\"  [Freq: %d → %d]",
                               lastSearch.c_str(), controller_.lastFreqBefore(), freq);
        }
    } else if (!controller_.statusMsg().empty()) {
        ImGui::Text("%s", controller_.statusMsg().c_str());
    }

    // ── RIGHT COLUMN: Sidebar (Search History) ────────────────────────────
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.18f, 0.48f, 0.90f, 1.00f), "↺ Search History");

    ImGui::BeginChild("HistoryBox", ImVec2(0, 360), true);
    if (submitHistory.empty()) {
        ImGui::TextDisabled("No searches registered yet.");
    } else {
        // Show reverse chronological history
        for (auto it = submitHistory.rbegin(); it != submitHistory.rend(); ++it) {
            std::string hWord = *it;
            // Draw a clickable button for history recall
            if (ImGui::Button(hWord.c_str(), ImVec2(-1.0f, 0))) {
                controller_.submitSearch(hWord);
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

    // Column 1: search submissions
    ImGui::Text("Search Submissions");
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
    ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.1f, 1.0f), "%d words", controller_.getTrieWordCount());

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
    if (!showHelp_) return;

    ImGui::SetNextWindowSize(ImVec2(480.0f, 320.0f), ImGuiCond_Always);
    if (ImGui::Begin("Usage Help Guide", &showHelp_,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "⌨ Keyboard Controls");
        ImGui::BulletText("Type characters in the search field to get recommendations.");
        ImGui::BulletText("Press [UP] / [DOWN] arrow keys to navigate suggestions.");
        ImGui::BulletText("Press [TAB] to complete selected suggestion into input.");
        ImGui::BulletText("Press [ENTER] to submit search and increase query count.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "⚙ Hotkeys");
        ImGui::BulletText("[Ctrl + S] : Save database snapshot instantly.");
        ImGui::BulletText("[F1]       : Toggle this help guide screen.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close Guide", ImVec2(120.0f, 30.0f))) {
            showHelp_ = false;
        }
    }
    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// Render Suggestion Row
// ═══════════════════════════════════════════════════════════════════════════

void GuiApp::renderSuggestionRow(const WordEntry& e, int idx, int maxFreq,
                                 bool isFuzzy, int editDist) {
    ImGui::PushID(idx);

    // Apply selection styling background if active row
    bool isSelected = (idx == controller_.selectedIdx());
    if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // Green text indicator if recently accessed
    bool recentlySearched = (e.lastAccess > 0);
    ImVec4 wordColor = recentlySearched ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f)
                                        : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

    if (isFuzzy) {
        // Red color for fuzzy matches
        wordColor = ImVec4(0.9f, 0.4f, 0.4f, 1.0f);
    }

    // Selectable behavior
    char selectLabel[128];
    if (isFuzzy) {
        std::snprintf(selectLabel, sizeof(selectLabel), "%s (fuzzy, dist=%d)", e.word.c_str(), editDist);
    } else {
        std::snprintf(selectLabel, sizeof(selectLabel), "%s", e.word.c_str());
    }

    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
    if (ImGui::Selectable(selectLabel, isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 24.0f))) {
        controller_.selectedIdx() = idx;
        controller_.submitSearch(e.word);
        queryBuf_[0] = '\0';
        controller_.selectedIdx() = -1;
        controller_.refreshSuggestions("");
    }
    ImGui::PopStyleVar();

    if (isSelected) {
        ImGui::PopStyleColor();
    }

    // Right-aligned frequency progress bar
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

    ImGui::PopID();
}

// ═══════════════════════════════════════════════════════════════════════════
// ImGui Callback: handles input keyboard captures (Completion, History UP/DOWN)
// ═══════════════════════════════════════════════════════════════════════════

int GuiApp::inputTextCallback(ImGuiInputTextCallbackData* data) {
    GuiApp* app = static_cast<GuiApp*>(data->UserData);
    if (!app) return 0;

    auto& controller = app->controller_;
    const auto& suggestions = controller.suggestions();
    const auto& submitHistory = controller.submitHistory();

    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCompletion: {
        // Tab completion
        std::string word;
        if (controller.selectedIdx() >= 0 && controller.selectedIdx() < static_cast<int>(suggestions.size())) {
            word = suggestions[controller.selectedIdx()].word;
        } else if (!suggestions.empty()) {
            word = suggestions[0].word;
        }

        if (!word.empty()) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, word.c_str());
            controller.selectedIdx() = -1;
            controller.historyMode() = false;
        }
        break;
    }
    case ImGuiInputTextFlags_CallbackHistory: {
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (data->BufTextLen == 0 || controller.historyMode()) {
                // Navigate history
                if (!submitHistory.empty()) {
                    controller.historyIdx() = std::min(controller.historyIdx() + 1, static_cast<int>(submitHistory.size()));
                    std::string hWord = submitHistory[submitHistory.size() - controller.historyIdx()];
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, hWord.c_str());
                    controller.historyMode() = true;
                    controller.selectedIdx() = -1;
                }
            } else if (!suggestions.empty()) {
                // Move selection up
                if (controller.selectedIdx() <= 0) {
                    controller.selectedIdx() = static_cast<int>(suggestions.size()) - 1;
                } else {
                    controller.selectedIdx()--;
                }
            }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (controller.historyMode()) {
                controller.historyIdx()--;
                if (controller.historyIdx() <= 0) {
                    controller.historyIdx() = 0;
                    controller.historyMode() = false;
                    data->DeleteChars(0, data->BufTextLen);
                } else {
                    std::string hWord = submitHistory[submitHistory.size() - controller.historyIdx()];
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, hWord.c_str());
                }
            } else if (!suggestions.empty()) {
                // Move selection down
                controller.selectedIdx() = (controller.selectedIdx() + 1) % static_cast<int>(suggestions.size());
            }
        }
        break;
    }
    }
    return 0;
}
