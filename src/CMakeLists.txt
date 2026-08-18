cmake_minimum_required(VERSION 3.20)
project(Mars VERSION 1.0.0 LANGUAGES CXX)

# Set C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(MARS_SOURCES
    mars.cpp
    simulatie.cpp
    planeet.cpp
    monsterPNG.cpp
)

# Executable
add_executable(mars ${MARS_SOURCES})

# Gereedschap brengt glfw, libpng, glm en wgpu als PUBLIC mee
target_link_libraries(mars
    gereedschap
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