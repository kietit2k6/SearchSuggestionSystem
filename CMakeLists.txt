cmake_minimum_required(VERSION 3.14)

project(SearchSuggestion CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── FetchContent for GLFW & ImGui ──────────────────────────────────────────
include(FetchContent)

# 1. GLFW
FetchContent_Declare(
    glfw
    URL      https://github.com/glfw/glfw/archive/refs/tags/3.3.8.zip
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. Dear ImGui
FetchContent_Declare(
    imgui
    URL      https://github.com/ocornut/imgui/archive/refs/tags/v1.89.9.zip
)
FetchContent_MakeAvailable(imgui)

# Define ImGui static library since the raw repo doesn't expose a CMake target
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# Find OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC OpenGL::GL)

# ── Project Sources ────────────────────────────────────────────────────────
set(SOURCE_FILES
    src/main.cpp
    src/Trie.cpp
    src/LRUCache.cpp
    src/AppCore.cpp
    src/GuiApp.cpp
)

set(HEADER_FILES
    include/Trie.h
    include/TrieNode.h
    include/SearchTypes.h
    include/LRUCache.h
    include/AppCore.h
    include/GuiApp.h
)

add_executable(SearchSuggestion ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(SearchSuggestion PRIVATE include)
target_link_libraries(SearchSuggestion PRIVATE imgui)