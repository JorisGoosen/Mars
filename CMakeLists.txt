cmake_minimum_required(VERSION 3.20)
project(Mars VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Subdirectories
add_subdirectory(Gereedschap)
add_subdirectory(src)

# Demo's (optioneel, uitgeschakeld: Gereedschap heeft zelf al demo's)
option(MARS_BUILD_DEMOS "Build demo applications" OFF)
if(MARS_BUILD_DEMOS)
    add_subdirectory(demos)
endif()