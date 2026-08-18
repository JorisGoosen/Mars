cmake_minimum_required(VERSION 3.20)
project(MarsWeb LANGUAGES CXX)

# Emscripten-specifieke instellingen
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output als .html (entry-point voor de browser)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist)

# Mars executable bronnen
file(GLOB MARS_SOURCES 
    ${CMAKE_SOURCE_DIR}/../src/mars.cpp
    ${CMAKE_SOURCE_DIR}/../src/simulatie.cpp
    ${CMAKE_SOURCE_DIR}/../src/planeet.cpp
    ${CMAKE_SOURCE_DIR}/../src/monsterPNG.cpp
)

add_executable(mars_web ${MARS_SOURCES})
set_target_properties(mars_web PROPERTIES SUFFIX ".html")

# Gereedschap als statische bibliotheek (alle bronnen + platform-laag)
file(GLOB GEREEDSCHAP_SOURCES 
    ${CMAKE_SOURCE_DIR}/../Gereedschap/weergaveScherm.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/weergaveSchermVierkant.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/weergaveSchermPerspectief.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/helpers.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/perlinRuis.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/nepScherm.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/geometrie/icosahedron.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/geometrie/geodesisch.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/geometrie/vierkantRooster.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/subjecten/wolfSchaap.cpp
    ${CMAKE_SOURCE_DIR}/../Gereedschap/emscriptenWebBackend.cpp
)

add_library(gereedschap STATIC ${GEREEDSCHAP_SOURCES})
target_compile_features(gereedschap PUBLIC cxx_std_20)

# Include pads: homebrew headers voor glm/libpng (wordt door port aangevuld met webgpu.h)
target_include_directories(gereedschap PUBLIC
    ${CMAKE_SOURCE_DIR}/../Gereedschap
    ${CMAKE_SOURCE_DIR}/../Gereedschap/geometrie
    ${CMAKE_SOURCE_DIR}/../Gereedschap/subjecten
    /opt/homebrew/include
)

# Voor Emscripten: voeg emdawnwebgpu include pad toe vóór homebrew pads
# zodat webgpu.h van de port wordt gebruikt i.p.v. wgpu-native
set(EMDAWNWEBGPU_INCLUDE "/Users/virtuoos/Broncode/emsdk/upstream/emscripten/cache/ports/emdawnwebgpu/emdawnwebgpu_pkg/webgpu/include/webgpu")
if(EXISTS "${EMDAWNWEBGPU_INCLUDE}")
    message(STATUS "emdawnwebgpu includes gevonden: ${EMDAWNWEBGPU_INCLUDE}")
    target_include_directories(gereedschap BEFORE PUBLIC "${EMDAWNWEBGPU_INCLUDE}")
endif()

# Link gereedschap naar mars_web
target_link_libraries(mars_web PRIVATE gereedschap)

# ── Emscripten-flags ────────────────────────────────────────────────────────
# --use-port=emdawnwebgpu: levert <webgpu/webgpu.h> + JS-bindings (vervangt -sUSE_WEBGPU)
# -sUSE_GLFW=3:           Emscripten GLFW-implementatie (maakt canvas, key-mapping, etc.)
# -sASYNCIFY=1:           Laat blokkerende while-loops werken tegen async JS-calls
# -sFORCE_FILESYSTEM=1:   Virtueel bestandssysteem voor shaders/plaatjes
# --preload-file:         Shaders en bump-textuur in het virtuele FS laden
# --closure=1:            Code-size optimalisatie voor release
# -sALLOW_MEMORY_GROWTH:  Dynamisch geheugen (nodig voor grotere grids)
# -sMODULARIZE:           Module-pattern voor embedden in HTML

foreach(TARGET mars_web gereedschap)
    target_compile_options(${TARGET} PRIVATE
        --use-port=emdawnwebgpu
        -O2
    )
endforeach()

target_link_options(mars_web PRIVATE
    --use-port=emdawnwebgpu
    -sUSE_GLFW=3
    -sASYNCIFY=1
    -sFORCE_FILESYSTEM=1
    -sUSE_LIBPNG=1
    -sALLOW_MEMORY_GROWTH=1
    -sEXPORTED_FUNCTIONS=['_main']
    -sMODULARIZE=1
    -sINITIAL_MEMORY=268435456
    -sMAXIMUM_MEMORY=4294967296
    --preload-file=${CMAKE_SOURCE_DIR}/../shaders@/shaders
    --preload-file=${CMAKE_SOURCE_DIR}/../plaatjes@/plaatjes
    --closure=1
)

# Kopiër index.html naar dist
add_custom_command(TARGET mars_web POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/index.html ${CMAKE_BINARY_DIR}/dist/index.html
    COMMENT "Kopiëren van index.html naar dist/"
)
