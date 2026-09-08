#!/bin/bash
# Bouw de Mars-simulatie voor de browser (Emscripten)
# Vereist: emsdk geïnstalleerd en geactiveerd.
# Configureert de repo-root (gedeelde CMakeLists-structuur) met de
# Emscripten-toolchain — er is geen aparte web/CMakeLists.txt meer.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MARS_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$BUILD_DIR/dist"

echo "=== Mars Web-Build ==="
echo "Repo root: $MARS_ROOT"
echo "Build dir: $BUILD_DIR"

# Controleer of emcc beschikbaar is
if ! command -v emcc &> /dev/null; then
    echo "FOUT: Emscripten (emcc) niet gevonden!"
    echo "Installeer emsdk via:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo "Emscripten versie: $(emcc --version | head -1)"

# Maak build directory
mkdir -p "$BUILD_DIR"

# Configureer de ROOT met de Emscripten-toolchain
echo ""
echo "CMake configureren (repo-root)..."
cmake -S "$MARS_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGEREEDSCHAP_BUILD_DEMOS=OFF

# Bouw
echo ""
echo "Bouwen..."
cmake --build "$BUILD_DIR" --config Release -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# Controleer output
if [ -f "$DIST_DIR/mars.js" ] && [ -f "$DIST_DIR/mars.wasm" ]; then
    echo ""
    echo "=== Build succesvol! ==="
    echo "Web-applicatie: $DIST_DIR/"
    echo ""
    echo "Start een lokale server om te testen:"
    echo "  python3 -m http.server 8080 --directory $DIST_DIR"
    echo "  # of"
    echo "  npx serve $DIST_DIR"
    echo ""
    echo "Open in browser: http://localhost:8080"

    # Synchroniseer naar de GitHub Pages-releasefolder (docs/)
    DOCS_DIR="$MARS_ROOT/docs"
    echo ""
    echo "Synchroniseren naar $DOCS_DIR ..."
    mkdir -p "$DOCS_DIR"
    rm -rf "${DOCS_DIR:?}"/* "$DOCS_DIR"/.nojekyll
    cp -R "$DIST_DIR"/. "$DOCS_DIR"/
    touch "$DOCS_DIR/.nojekyll"
    echo "Releasefolder bijgewerkt: docs/ (committen en pushen voor GitHub Pages)"
else
    echo ""
    echo "FOUT: Build mislukt (geen mars.js/mars.wasm gevonden in $DIST_DIR)"
    exit 1
fi
