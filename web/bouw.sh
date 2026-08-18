#!/bin/bash
# Bouw de Mars-simulatie voor de browser (Emscripten)
# Vereist: emsdk geïnstalleerd en geactiveerd

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

echo "Emscripten versie: $(emcc --version)"

# Maak build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configureer met CMake
echo ""
echo "CMake configureren..."
cmake "$SCRIPT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGEREEDSCHAP_BUILD_DEMOS=OFF

# Bouw
echo ""
echo "Bouwen..."
cmake --build . --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

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
else
    echo ""
    echo "FOUT: Build mislukt (geen mars.js/mars.wasm gevonden)"
    exit 1
fi
