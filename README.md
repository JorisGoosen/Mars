# Mars - Planet Simulation

Cross-platform OpenGL planet simulation with fluid dynamics.

## Build Requirements
- C++20 compiler
- CMake 3.20+
- OpenGL 4.6 compatible GPU (4.4 minimum for compute shaders)
- Libraries: glfw3, glew, libpng, glm

## Build Instructions

### Linux
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libglfw3-dev libglew-dev libpng-dev libglm-dev

# Build
cmake -B build
cmake --build build

# Run
./build/src/mars
```

### macOS (Intel)
```bash
# Install dependencies (via Homebrew)
brew install glfw3 glew libpng glm

# Build
cmake -B build
cmake --build build

# Run
./build/src/mars
```

### macOS (Apple Silicon M1/M2/M3)
**Note:** macOS on Apple Silicon only supports OpenGL 4.1 via the Metal wrapper. 
The planet simulation uses OpenGL compute shaders which require 4.3+ and are not available.

**Workarounds:**
1. **Use Linux VM**: Run Linux in a VM for full features
2. **Use Intel Mac**: Build on Intel-based Mac
3. **Use Metal**: Requires rewriting the graphics code (not implemented)

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

## Supported Platforms
- ✅ Linux (Intel/ARM)
- ✅ macOS Intel
- ⚠️ macOS Apple Silicon (OpenGL 4.1 only, compute shaders not available)
- ❓ Windows (untested)
