# Plan: browser + cross-platform executable + GUI

## Uitgangspunt
Eén C++/WGSL-codebase blijven voeren. De GPU-keten (shaders, `planeet.cpp`, bind-groepen) is al WebGPU-portable. Vier dingen moeten er wél bij: platformlaag voor venster/surface, faseringsrefactor van de hoofdloop, web backend zonder blokkerende GPU-waits, en Dear ImGui als gedeelde GUI.

## Fase 1 — Faseringsrefactor (gedragsgelijk, native)
- `src/mars.cpp`: alles achter het `while(!moetStoppen())`-blok (`src/mars.cpp:1233`) plus de opzet ernaartoe (shaders, buffers, `planeet`, toetsenhandler, lambda's `doeSchaduwPass`/`doeRenderPassen`) in een eigen module (`src/simulatie.h/.cpp`) als klassen met `init()` en `stap()`. De lokale variabelen/lambda's worden leden.
- CLI-argumenten verhuizen naar een struct die ook van buiten vulbaar is (web kan later argv uit URL-query halen; `--hoofdloos` blijft native-only: `#ifndef __EMSCRIPTEN__`).
- Verificatie per AGENTS-workflow: screenshots diffen vóór/na refactor (zelfde `--procedureel --diepte 4 --stappen 300`), `--conservering` blijft "GEVANGEN".

## Fase 2 — Cross-platform native (macOS blijft leidend)
- CMake: de geforceerde `CMAKE_OSX_*`-regels blijven alleen op APPLE; wgpu-native via `file(DOWNLOAD)` van de GitHub-release (v29.x, checksum) voor Windows/Linux naast het bestaande brew-pad; GLFW/glm/libpng via FetchContent of vcpkg op Windows.
- Oppervlak-creatie in Gereedschap naast `metaalLaag.mm`: kleine platformbestanden met `WGPUSurfaceSourceWindowsHWND` (`glfwGetWin32Window`), `WGPUSurfaceSourceXlibWindow`/`Wayland` — GLFW 3.5 heeft namelijk géén WGPU-surface-API. In `weergaveScherm.cpp:138` zit nu alleen Metal.
- Verwacht weinig verrassingen: de rest van de code gebruikt alleen standaard-`webgpu.h` (wgpu-native 29 biedt die chains al).
- Gereedschap is een submodule → commits dáár + pointer-update in de hoofdrepo (per AGENTS).

## Fase 3 — Browser (Emscripten + browser-eigen WebGPU)
- Nieuwe web-backend in Gereedschap (`__EMSCRIPTEN__`-takken): device uit de pagina (promise-based init, dus geen `wgpuCreateInstance`-spin), surface via canvas-selector, key/mouse via `emscripten_set_*_callback`, frame via `emscripten_set_main_loop` → daarvoor bestaat Fase-1's `stap()`.
- De blokkerende patronen (`while(!klaar) wgpuInstanceProcessEvents(...)`, `wachtOpRij` in `src/mars.cpp:464`, diagnose/CSV/screenshot-tools) zijn native-only; web krijgt pingpong-uniforms i.p.v. de per-frame GPU-wait (zelfde flikkerfix, niet-blokkerend).
- Assets: `--preload-file shaders` + `plaatjes` (~0,7 MB) — `tekstInlezen()`/`laadPNG()` blijven ongewijzigd. Default `--procedureel` (gekozen); MOLA (6,8 MB) volgt eventueel later als lazy fetch via GUI-knop.
- `web/index.html` (+ shell met `navigator.gpu`-detectie en melding), buildscript `web/bouw.sh` (emcc ≥4.0.10, `-sUSE_WEBGPU`, `-sUSE_LIBPNG`, `-sALLOW_MEMORY_GROWTH`). WGSL-shaders hoeven niet aangepast.

## Fase 4 — Dear ImGui GUI (beide platforms)
- ImGui als submodule/FetchContent; de WebGPU-backend (`imgui_impl_wgpu`) wordt actief onderhouden voor zowel native wgpu als Emscripten (eist emsdk ≥4.0.10). Let op: compilatie checken tegen de exacte `webgpu.h` van brew's wgpu-native 29 — kleine header-drift is het enige reële risico.
- Integratie in het bestaande enkaderingspatroon: planeten-passes eindigen op `pasRondRenderAf()`, daarna ImGui als laatste pass vóór `rondWeergevenAf()` (bind-groepen 0/1 blijven intact; blending aan, diepte-schrijven uit).
- Widgets spiegelen alle bestaande toets-tunables: sliders (zonKracht, winterZonneKracht, rotatieOmega, coriolisOmega, wrijving, diffusie, verdamping, neerslagFactor, hoogteKoel, grondMult, luchtStappen...), checkboxes (water, wolken, schaduw, zon, rotatie, bevroren), overlay-pulldown (T/2-0 mapping), "herstart" met procedureel/diepte, stats (fps, cellen, geheugen). Toetsen blijven werken naast de GUI.

## Fase 5 — CI, pakketten, docs
- GitHub Actions matrix: macOS arm64 (brew), Linux x64 (apt + release-wgpu), Windows x64 (MSVC + FetchContent/vcpkg), plus een emscripten-job die `web/dist/` als artifact (en evt. Pages-deploy) oplevert.
- README + AGENTS bijwerken (nieuwe bouwroutes, vlaggen, GUI-toets `F1`/knop).

## Risico's
1. ImGui-header-drift met wgpu-native 29 → vroeg bouwen in Fase 4, pin imgui-release.
2. Per-frame GPU-wait op web → pingpong-uniforms i.p.v. ASYNCIFY (performance-vriendelijker).
3. Windows/Wayland-surface-gedrag (gamma/presentatie) vraagt een handmatige rooktest per platform.

## Volgorde & verifiëren
Fase 1 → 2 → 3 → 4 → 5; elke fase sluit af met headless A/B-diff en `--conservering` (AGENTS-workflow). Fase 1+2 kunnen ook parallel met 3 als twee branches.
