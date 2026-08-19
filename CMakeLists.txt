cmake_minimum_required(VERSION 3.20)
project(Mars VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dear ImGui (GUI voor native én web) ──────────────────────────────────────
# Eén bron voor beide platforms; eenmalig ophalen op een gepinde release.
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# Subdirectories
add_subdirectory(Gereedschap)
add_subdirectory(src)

# Demo's (optioneel, uitgeschakeld: Gereedschap heeft zelf al demo's)
option(MARS_BUILD_DEMOS "Build demo applications" OFF)
if(MARS_BUILD_DEMOS)
    add_subdirectory(demos)
endif()