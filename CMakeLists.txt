cmake_minimum_required(VERSION 3.20)
project(Mars VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS arm64 only (deployment-target moet matchen met de Gereedschap-submodule)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build for Apple Silicon" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# Dependencies (via pkgconfig; de werkelijke include/link-gegevens komen uit Gereedschap)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(LIBPNG REQUIRED libpng)

# Subdirectories
add_subdirectory(Gereedschap)
add_subdirectory(src)

# Demo's (optioneel, uitgeschakeld: Gereedschap heeft zelf al demo's)
option(MARS_BUILD_DEMOS "Build demo applications" OFF)
if(MARS_BUILD_DEMOS)
    add_subdirectory(demos)
endif()