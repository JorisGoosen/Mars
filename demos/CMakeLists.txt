cmake_minimum_required(VERSION 3.20)
project(MarsDemos VERSION 1.0.0 LANGUAGES CXX)

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
pkg_check_modules(GLM REQUIRED glm)
find_package(OpenGL REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Gereedschap)
include_directories(${GLFW3_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${LIBPNG_INCLUDE_DIRS})
include_directories(${GLM_INCLUDE_DIRS})

# Demo sources
set(DEMO_SOURCES
    quadDemo.cpp
    perspectiefDemo.cpp
    geoDemo.cpp
    geometryDemo.cpp
    landDemo.cpp
    computeDemo.cpp
    marcheerDemo.cpp
)

# Build each demo as separate executable
foreach(DEMO ${DEMO_SOURCES})
    # Get demo name without extension
    string(REPLACE ".cpp" "" DEMO_NAME ${DEMO})
    
    add_executable(${DEMO_NAME} ${DEMO})
    
    target_link_libraries(${DEMO_NAME}
        gereedschap
        ${GLFW3_LIBRARIES}
        ${GLEW_LIBRARIES}
        ${LIBPNG_LIBRARIES}
        ${OPENGL_LIBRARIES}
        $<$<PLATFORM_ID:Darwin>:-framework OpenGL>
    )
    
    # Compiler flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${DEMO_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror=return-type
        )
    endif()
endforeach()
