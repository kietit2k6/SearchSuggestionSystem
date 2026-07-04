# SearchSuggestion

A Google-like desktop search engine written in C++17 using Dear ImGui, GLFW, and OpenGL 3.

## Features

- **High-Performance Trie Engine**: Custom C++ Trie supporting standard `a-z` characters and a fallback child map for spaces, punctuation, digits, and multi-byte UTF-8 characters (e.g., Vietnamese).
- **O(1) Autocomplete**: Features a prefix recommendation cache inside Trie nodes combined with a thread-safe LRU Cache for high-frequency queries.
- **Dynamic Ranking**: Autocomplete suggestions are ranked by a composite score of search frequency and query recency (exponential time decay).
- **Fuzzy Search Fallback**: Levenshtein distance calculation optimized with a pre-allocated matrix to prevent heap allocations during deep Trie traversal.
- **Configurable Settings**: Customizable runtime settings (dictionary path, database storage location, auto-save interval, and cache size) loaded via `config.txt`.
- **Keyboard navigation**: Use `[Up/Down]` arrows to select recommendations, `[Tab]` to complete, and `[Enter]` to submit.
- **Hotkeys**:
  - `Ctrl + S`: Manually save data snapshot.
  - `F1`: Toggle help overlay.

## Building and Running

### Prerequisites

#### macOS
No extra tools are required aside from a modern compiler and CMake.

#### Linux (Ubuntu/Debian)
Ensure you have the required GLFW dependencies installed:
```bash
sudo apt-get update
sudo apt-get install cmake build-essential libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
```

### Build Instructions

To build the application and the unit test suite:

```bash
cmake -B build
cmake --build build
```

### Run Instructions

#### Running the main GUI application:
```bash
./build/SearchSuggestion
```

#### Running the unit test suite:
```bash
./build/SearchSuggestionTests
```

## Dictionary Configuration

By default, the application tries to load a wordlist file in the following order:
1. `./words.txt` (local file in current directory)
2. `/usr/share/dict/words` (macOS/Linux system words)
3. Internal fallback words (approx. 2000 common programming terms/suffixes)

To use your own dictionary file, place it in the application's root directory and rename it to `words.txt`.
