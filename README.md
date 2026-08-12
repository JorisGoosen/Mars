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
- **X**: Toggle water visibility
- **C**: Toggle cloud visibility
- **W/S**: Move camera forward/backward
- **A/D**: Move camera left/right
- **Q/E**: Move camera up/down
- **Arrows**: Rotate view
- **Enter**: Single water simulation step
- **;/'**: Adjust ground height
- **K/L**: Adjust evaporation rate
- **[ / ]**: Adjust planet rotation speed (Ω → Coriolis / daglengte)
- **U/I**: Adjust solar heating strength
- **O/P**: Adjust friction (demping)
- **./,**: Adjust precipitation factor

## Testvlaggen
- `--no-water`: start zonder water (`waterHoogte = 0`) — handig om de grond-rendering los te testen.
- `--no-erosion`: houdt het terrein stil (geen erosie/depositie) zodat water gedrag bekeken kan worden zonder hoogteveranderingen.
- `--no-life`: zet plantengroei uit (geen groene begroeiing), handig om louter het rots/zand-erfgoed te bekijken.
- `--no-atmosfeer`: houdt de lucht volledig stil (geen wind, verdamping of neerslag); het water stroomt nog.
- `--procedural`: genereert het terrein met ruis i.p.v. de MOLA-hoogtekaart (geen PNG nodig). Ideaal voor snelle, kleine grids.
- `--subdiv <n>`: icosahedron-onderverdelingsniveau (standaard **5**; hoger = fijner, maar trager).
- `--diag`: print elke 25 frames de extremen van de reken-stand terug (water, bodem/luchtvocht, droesem, temperatuur, luchtdruk, wind, wolken) en meldt niet-eindige cellen.
- `--diagCsv <bestand>`: dumpt de **hele** planeet naar een CSV (één rij per cel) zodat de berekening extern geanalyseerd kan worden. Bedoeld voor kleine grids (laag `--subdiv`); `--diagCsvElkeFrames <n>` zet het interval (standaard 25).
- `--headless`: draait zonder venster (geen aqua/display nodig), bijv. `--headless --procedural --subdiv 4 --stappen 3000 --diagCsv uit.csv`.
- `--stappen <n>`: stop na n rondes (samen met `--headless`).

Voorbeeld: `./build/src/mars --no-water --no-erosion --no-life`
Analyse-voorbeeld: `./build/src/mars --procedural --headless --subdiv 4 --stappen 3000 --diagCsv uit.csv`

## Atmosferische circulatie
De oude synthetische wind (elke frame verzonnen uit een draaiende as) is vervangen
door een **echt, opgeslagen** atmosfeerveld met drie vragen:
- `temperatuur`: stralingsevenwicht (dag/nacht + breedte + hoogte), geadvecteerd
  met de wind en versoepeld naar het evenwicht (Newton-relaxatie).
- `luchtdruk`: thermische bron (warme lucht = lage oppervlaktedruk) + continuïteit
  (divergentie) + diffusie → onderhoudt het drukgradiënt dat de wind aandrijft.
- `wind`: drukgradiëntkracht + Coriolis (Ω × breedte) + wrijving + diffusie.

Straalstromen/banden ontstaan zo vanzelf. Dag/nacht volgt uit een zon die om de
geografische noordpool draait (de planeet draait t.o.v. de zon); Coriolis gebruikt
dezelfde rotatie.

## Vochtcyclus in twee fasen
`luchtVocht` is damp (de capaciteit volgt de temperatuur); `wolken` is het
gecondenseerde wolkwater. Oververzadigde damp condenseert tot wolken; wolken geven
hun water af door terug te verdampen én door **regen die uitsluitend uit wolken
valt**. Zowel damp als wolken worden met het windveld geadvecteerd.


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
