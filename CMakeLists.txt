cmake_minimum_required(VERSION 3.20)
project(Mars VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS arm64 only
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build for Apple Silicon" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
    
    # Note: macOS on Apple Silicon only supports OpenGL 4.1 via Metal wrapper
    # Compute shaders require OpenGL 4.3+ which is not available
    # demos with compute shaders won't work
endif()

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)
pkg_check_modules(LIBPNG REQUIRED libpng)
find_package(OpenGL REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Gereedschap)
include_directories(${GLFW3_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${LIBPNG_INCLUDE_DIRS})

# Subdirectories
add_subdirectory(Gereedschap)
add_subdirectory(src)

# Demos (optional)
option(MARS_BUILD_DEMOS "Build demo applications" ON)
if(MARS_BUILD_DEMOS)
    add_subdirectory(demos)
endif()
