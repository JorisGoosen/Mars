# AGENTS.md

## Bouwen & draaien
- `cmake -B build && cmake --build build`; draai `./build/src/mars` **vanaf de repo-root**: shaders, `MARS_Hoogte.png` en `plaatjes/zeewater_bump.png` worden runtime vanuit de CWD geladen.
- Shaders worden runtime gecompileerd: shader-wijzigingen vereisen géén rebuild; C++ wel. `#include "x.wgsl"` in shaders wordt door het framework opgelost relatief t.o.v. het includende bestand.
- `Gereedschap/` is een git-submodule (eigen WebGPU-wrapper). Framework-fixes commit je ín de submodule én als pointer-update in de hoofdrepo.
- macOS arm64 only (CMake forceert dit); demo's staan uit (`MARS_BUILD_DEMOS`).

## Verifiëren (er is geen test-/lint-suite)
- Headless A/B is de workflow: `--hoofdloos --procedureel --diepte 3..5 --stappen N --schermafbeelding x.png`, plus `--veldKaart <veld> x.png` (o.a. `temperatuur`, `wind`, `wolken`, `zonZicht`), `--diagnoseCsv`, `--conservering`, `--dumpSchaduw`.
- Vlaggen voor isolatie: `--zonder-water`, `--zonder-erosie`, `--zonder-leven`, `--zonder-atmosfeer`, `--zonder-schaduw`, `--stil`.
- Let op: `--stil` bevriest óók de rekenketen (sim-velden zoals `zonZicht` blijven op hun startwaarde).
- Artifact-jacht: render hetzelfde frame met/zonder `--zonder-schaduw` en diff de pixels; wat zonder schaduw ook verschilt is géén schaduw-artifact (bijv. terminator-beweging).

## Architectuur-valkuilen
- C++↔WGSL structs moeten byte-identiek blijven: `vak` (152 B), `vakMeta` (144 B), `rekenParameters` (96 B), `extraParameters` (exact 16 floats). `static_assert`s in `src/mars.cpp`/`src/planeet.cpp`; wijzig beide kanten tegelijk.
- Rekenketen (vaste volgorde in de loop van mars.cpp): waterStroming → waterDruk → waterGemiddelde → luchtStroming → vochtStroming → waterLucht, dan `volgendeRonde()` (pingpong vakken0↔vakken1). Elk veld wordt door precies één shader per ronde geschreven; erosie/depositie én de zand-rusthelling zitten samen in waterDruk.
- Bind-groepen: render 0=uniforms (`extra` = de 16 floats), 1=textuur+lineaire sampler, 2=vier opslag-buffers, 3=schaduwkaart+nearest sampler; compute 0=opslag, 1=schaduw-layout-textuur (binden via `bindTextuur(...)` vóór de dispatch, daarna `bindTextuur("", 0)`).
- Schaduwkaart: orthografische projectie **analytisch** uit de zonrichting (`zonProjectie` in `shaders/zonSchaduw.wgsl`) — dezelfde formule in shadow-vertex, fragment-lookups én compute, anders krijg je gespiegelde/verplaatste schaduwen. NDC y+ landt in texel-rij 0 → v-as omklappen bij de lookup (`zonSchaduwUV`).
- Shadow-pass: `cullMode Front` (voorkant eruit, anders zelf-vergelijking), casters = terrein+ijs (water telt niet mee), casters worden `schaduwEpsilon` van de zon af geduwd. Diepte-texturen zijn unfilterable → nearest sampler + handmatige PCF.
- Vierkante offscreen/depth-passes mogen `_schermVerhouding` niet overschrijven (guard `!_diepteDoel` in `Gereedschap/weergaveScherm.cpp`) — anders lijkt alles op verschoven schaduwen.

## Stijl
- Alles in het Nederlands: identifiers, comments, commit-messages, README. Commits kort, met oud→nieuw bij tunables.
- Sim-tunables wonen in `shaders/planeetStructen.wgsl`; weergave/schaduw-helpers in `shaders/zonSchaduw.wgsl` / `planeetDefinities*.wgsl`.
