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

# ── Web-build (Emscripten) ───────────────────────────────────────────────────
# De Gereedschap-submodule detecteert EMSCRIPTEN zelf (single source of truth);
# hier komen alleen de linker-flags en asset-kopie die specifiek voor de
# Mars-executable zijn.
if(EMSCRIPTEN)
    set_target_properties(mars PROPERTIES
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist
    )

    target_compile_options(mars PRIVATE --use-port=emdawnwebgpu)

    target_link_options(mars PRIVATE
        --use-port=emdawnwebgpu
        -sASYNCIFY=1
        -sFORCE_FILESYSTEM=1
        -sUSE_LIBPNG=1
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_FUNCTIONS=['_main']
        -sMODULARIZE=1
        -sEXPORT_NAME=MarsWeb
        -sINITIAL_MEMORY=268435456
        -sMAXIMUM_MEMORY=4294967296
        --preload-file=${CMAKE_BINARY_DIR}/dist/shaders@/shaders
        --preload-file=${CMAKE_BINARY_DIR}/dist/plaatjes@/plaatjes
        --closure=1
    )

    # ── Assets kopiëren naar dist vóór build ─────────────────────────────
    add_custom_command(TARGET mars PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/dist/shaders
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/plaatjes
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/plaatjes ${CMAKE_BINARY_DIR}/dist/plaatjes
        COMMENT "Kopiëren van assets naar dist/"
    )

    # index.html naar dist (POST_BUILD: de gegenereerde mars.js moet al bestaan)
    add_custom_command(TARGET mars POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/web/index.html ${CMAKE_BINARY_DIR}/dist/index.html
        COMMENT "Kopiëren van index.html naar dist/"
    )
endif()
