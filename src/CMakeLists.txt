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
    gereedschap.cpp
    planeet.cpp
    monsterPNG.cpp
    gereedschapUitvoer.cpp
    penseelSelectie.cpp
    planeetAanwijzer.cpp
    penseelGereedschap.cpp
)

# Executable
add_executable(mars ${MARS_SOURCES})

# Gereedschap brengt glfw, libpng, glm en wgpu als PUBLIC mee
target_link_libraries(mars
    gereedschap
)

# ── Dear ImGui-GUI (static lib, native én web) ──────────────────────────────
add_library(imgui_ui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
    gui.cpp
)
target_compile_features(imgui_ui PUBLIC cxx_std_20)
#PUBLIC: gui.h (en daarmee imgui.h/imgui_impl_wgpu.h) wordt ook door mars.cpp/
#simulatie.cpp geinclude
target_include_directories(imgui_ui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# imgui_impl_wgpu: native én web gebruiken de C-API (webgpu.h). De DAWN-backend
# past bij wgpu-native 29 én emdawnwebgpu (het alias WGPUProgrammableStageDescriptor
# = WGPUComputeState vangt de naamswissel af). Op web kiest de port die sowieso.
if(EMSCRIPTEN)
    target_compile_options(imgui_ui PRIVATE --use-port=emdawnwebgpu)
else()
    target_compile_definitions(imgui_ui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
    #compat biedt <webgpu/webgpu.h>/<webgpu/wgpu.h> → wgpu-native; ook voor mars
    #omdat gui.h/imgui_impl_wgpu.h die include.
    target_include_directories(imgui_ui PUBLIC ${CMAKE_SOURCE_DIR}/compat)
    if(APPLE)
        # imgui_impl_wgpu.cpp includeert op macOS <Cocoa/Cocoa.h> voor een
        # surface-helper die wij niet gebruiken; als ObjC++ compileren kan dat wel.
        set_source_files_properties(${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
            PROPERTIES COMPILE_FLAGS "-x objective-c++")
    endif()
endif()

# gui.cpp includeert Simulatie/weergaveScherm → neem de include-paden + emdawnwebgpu
# compile-vlag van Gereedschap over.
target_link_libraries(imgui_ui PUBLIC gereedschap)

target_link_libraries(mars imgui_ui)

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
        # MARS_Hoogte.png op de server (niet gepreload) voor de lazy MOLA-fetch
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/MARS_Hoogte.png ${CMAKE_BINARY_DIR}/dist/MARS_Hoogte.png
        COMMENT "Kopiëren van assets naar dist/"
    )

    # index.html naar dist (POST_BUILD: de gegenereerde mars.js moet al bestaan)
    add_custom_command(TARGET mars POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/web/index.html ${CMAKE_BINARY_DIR}/dist/index.html
        COMMENT "Kopiëren van index.html naar dist/"
    )
endif()
