# AGENTS.md

## Bouwen & draaien
- `cmake -B build && cmake --build build`; draai `./build/src/mars` **vanaf de repo-root**: shaders, `MARS_Hoogte.png` en `plaatjes/zeewater_bump.png` worden runtime vanuit de CWD geladen.
- Shaders worden runtime gecompileerd: shader-wijzigingen vereisen géén rebuild; C++ wel. `#include "x.wgsl"` in shaders wordt door het framework opgelost relatief t.o.v. het includende bestand.
- `Gereedschap/` is een git-submodule (eigen WebGPU-wrapper). Framework-fixes commit je ín de submodule én als pointer-update in de hoofdrepo.
- macOS arm64 only (CMake forceert dit); demo's staan uit (`MARS_BUILD_DEMOS`).

## Verifiëren (er is geen test-/lint-suite)
- Headless A/B is de workflow: `--hoofdloos --procedureel --diepte 3..5 --stappen N --schermafbeelding x.png`, plus `--veldKaart <veld> x.png` (o.a. `temperatuur`, `wind`, `wolken`, `zonZicht`), `--diagnoseCsv`, `--conservering`, `--dumpSchaduw`.
- Vlaggen voor isolatie: `--zonder-water`, `--zonder-erosie`, `--zonder-leven`, `--zonder-atmosfeer`, `--zonder-schaduw`, `--stil`.
- Beginwaarden per cel zijn via CLI te zetten (default 0 = "lege Mars"): `--water`, `--bodemvocht`, `--wolk`, `--leven`, `--ijs`, `--damp`, `--zand`, `--temperatuur` (Celsius; intern Kelvin, default 0 °C = 273 K). Dezelfde waarden zitten in de topbar (numeriek) en `SimulatieConfig`/`planeetInit`.
- Let op: `--stil` bevriest óók de rekenketen (sim-velden zoals `zonZicht` en `waterSchijn` blijven op hun startwaarde — water/ijs renderen dan op `waterSchijn=0`).
- Artifact-jacht: render hetzelfde frame met/zonder `--zonder-schaduw` en diff de pixels; wat zonder schaduw ook verschilt is géén schaduw-artifact (bijv. terminator-beweging).

## Architectuur-valkuilen
- C++↔WGSL structs moeten byte-identiek blijven: `vak` (152 B), `vakMeta` (144 B), `rekenParameters` (96 B), `extraParameters` (exact 16 floats). `static_assert`s in `src/mars.cpp`/`src/planeet.cpp`; wijzig beide kanten tegelijk. `extraParameters` heeft `wolkAlpha` (doorzichtigheid wolkendek, ex-`_padB`) en `waterReflectie` (waterspiegel/randreflectie-sterkte, ex-`_padC`); de overige pads (`_padD`/`_padE`) zijn vrij.
- Rekenketen (vaste volgorde in de loop van mars.cpp): waterStroming → waterDruk → waterGemiddelde → luchtStroming → vochtStroming → waterLucht, dan `volgendeRonde()` (pingpong vakken0↔vakken1). Elk veld wordt door precies één shader per ronde geschreven; erosie/depositie én de zand-rusthelling zitten samen in waterDruk.
- Render-pass-volgorde (`doeRenderPassen`): wolken(geflipt, `cull Front`) → grond → ijs-onderkant(`cull Front`) → ijs-bovenkant(`cull Back`) → water → wolken(normaal, `cull Back`) → highlight → GUI. Eerste pass wist kleur+diepte (`wisScherm`); ijs heeft eigen shaders (`planeetgridVertIjs[Onder].wgsl` + gedeelde `planeetgridFragIjs.wgsl`) en tekent ijs drijvend op de waterspiegel (`waterSchijn`) met zijn echte dikte — niet zoals de sim het intern als "grond onder water" rekent. Beide ijs-passes schrijven diepte vóór het water, zodat het transparante water (geen diepte-schrijf) ijs alleen bedekt waar het er vóór ligt (waterbulten boven het ijs correct).
- Bind-groepen: render 0=uniforms (`extra` = de 16 floats), 1=textuur+lineaire sampler, 2=vier opslag-buffers, 3=schaduwkaart+nearest sampler; compute 0=opslag, 1=schaduw-layout-textuur (binden via `bindTextuur(...)` vóór de dispatch, daarna `bindTextuur("", 0)`).
- Highlight/pick (penseel) liggen op `oppervlakTopHoogte` = grond + `waterSchijn` + ijs (het bovenste zichtbare oppervlak), zodat cursor en highlight óók op water en ijs zichtbaar zijn en de pick de cel op het zichtbare oppervlak leest.
- Schaduwkaart: orthografische projectie **analytisch** uit de zonrichting (`zonProjectie` in `shaders/zonSchaduw.wgsl`) — dezelfde formule in shadow-vertex, fragment-lookups én compute, anders krijg je gespiegelde/verplaatste schaduwen. NDC y+ landt in texel-rij 0 → v-as omklappen bij de lookup (`zonSchaduwUV`).
- Shadow-pass: `cullMode Front` (voorkant eruit, anders zelf-vergelijking), casters = het bovenste zichtbare oppervlak: terrein + waterspiegel (`waterSchijn`) + drijvend ijs (zelfde hoogte als de render, óók in de compute-lookup `luchtStroming`). Gevolg: de waterspiegel werpt schaduw op de zeebodem eronder. Casters worden `schaduwEpsilon` van de zon af geduwd. Diepte-texturen zijn unfilterable → nearest sampler + handmatige PCF; schaduwranden lopen gradueel via `schaduwZacht` (penumbra in `zonSchaduwPCF`).
- Vierkante offscreen/depth-passes mogen `_schermVerhouding` niet overschrijven (guard `!_diepteDoel` in `Gereedschap/weergaveScherm.cpp`) — anders lijkt alles op verschoven schaduwen.

## Stijl
- Alles in het Nederlands: identifiers, comments, commit-messages, README. Commits kort, met oud→nieuw bij tunables.
- Sim-tunables wonen in `shaders/planeetStructen.wgsl`; weergave/schaduw-helpers in `shaders/zonSchaduw.wgsl` / `planeetDefinities*.wgsl`.
