cmake_minimum_required(VERSION 3.10)

project(SearchSuggestion SYSTEM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    src/main.cpp
    src/Trie.cpp
    src/LRUCache.cpp
)

set(HEADER_FILES
    include/Trie.h
    include/TrieNode.h
    include/SearchTypes.h
    include/LRUCache.h
)

add_executable(SearchSuggestion ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(SearchSuggestion PRIVATE include)