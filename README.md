# Mars - Planet Simulation

Cross-platform WebGPU planet simulation with fluid dynamics and erosion.

## Build Requirements
- C++20 compiler
- CMake 3.20+
- A WebGPU-enabled backend (Vulkan, Metal, or D3D12) — provided by the `Gereedschap` submodule
- Libraries (via pkg-config): glfw3, libpng

## Build Instructions

### Linux
```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install libglfw3-dev libpng-dev

# Build
cmake -B build
cmake --build build

# Run
./build/src/mars
```

### macOS
```bash
# Install dependencies (via Homebrew)
brew install glfw libpng

# Build
cmake -B build
cmake --build build

# Run
./build/src/mars
```

WebGPU vertaalt automatisch naar het onderliggende grafische backend,
dus op Apple Silicon draait het via Metal (in tegenstelling tot OpenGL
zijn WebGPU-compute-shaders wél beschikbaar).

## Controls
- **Space**: Toggle water flow
- **R**: Toggle rotation
- **Z**: Toggle sun rotation
- **X**: Toggle water visibility
- **W/S**: Move camera forward/backward
- **A/D**: Move camera left/right
- **Q/E**: Move camera up/down
- **Arrows**: Rotate view
- **Enter**: Single water simulation step
- **;/'**: Adjust ground height
- **K/L**: Adjust evaporation rate

## Testvlaggen
- `--no-water`: start zonder water (`waterHoogte = 0`) — handig om de grond-rendering los te testen.
- `--no-erosion`: houdt het terrein stil (geen erosie/depositie) zodat water gedrag bekeken kan worden zonder hoogteveranderingen.
- `--no-life`: zet plantengroei uit (geen groene begroeiing), handig om louter het rots/zand-erfgoed te bekijken.

Voorbeeld: `./build/src/mars --no-water --no-erosion --no-life`

## Erosie / ondergronden
Elke cel heeft twee lagen: een zand/sediment-deklaag boven op een diepere
rots-ondergrond (`rotsHoogte`). De planeet start geheel als blootliggende rots;
zand ontstaat pas waar water erosie-materiaal (droesem) neerlegt. Zand erodeert
snel (×1) en beschermt de rots daaronder; zodra het zand is weggespoeld erodeert
de rots zelf 100× langzamer. Zowel erosie van zand als van rots vormt droesem in
het water, en waar water droesem neerlegt wordt het altijd zand. Op het
oppervlak zie je een zachte overgang van Mars-rode rots naar zand naarmate
de zandlaag dikker wordt.

## Supported Platforms
- ✅ Linux (Vulkan)
- ✅ macOS Intel & Apple Silicon (Metal)
- ❓ Windows (D3D12, untested)
