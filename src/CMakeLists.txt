cmake_minimum_required(VERSION 3.20)
project(Mars VERSION 1.0.0 LANGUAGES CXX)

# Set C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS arm64 only
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build for Apple Silicon" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)
pkg_check_modules(LIBPNG REQUIRED libpng)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Gereedschap)
include_directories(${GLFW3_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${LIBPNG_INCLUDE_DIRS})

# Source files
set(MARS_SOURCES
    mars.cpp
    planeet.cpp
    monsterPNG.cpp
)

# Executable
add_executable(mars ${MARS_SOURCES})

# Link libraries
target_link_libraries(mars
    gereedschap
    ${GLFW3_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${LIBPNG_LIBRARIES}
)

# macOS specific linking
if(APPLE)
    target_link_options(mars PRIVATE -framework OpenGL)
    # Note: macOS on Apple Silicon only supports OpenGL 4.1
    # Compute shaders are not available (requires 4.3+)
    # Water simulation will fail - use procedural mode instead
endif()

# Add library directories for linking
target_link_directories(mars PRIVATE
    ${GLFW3_LIBRARY_DIRS}
    ${GLEW_LIBRARY_DIRS}
    ${LIBPNG_LIBRARY_DIRS}
)

# Compile definitions
target_compile_definitions(mars PRIVATE
    _GLFWHAVE_CONTEXT_CREATION_API
)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(mars PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
    )
endif()
