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
- **Muis-sleep**: klik+slepen draait de planeet (trackball)
- **Scroll/swipe**: horizontaal = roteren, verticaal (of pinch) = zoomen
- **Space**: Bevries/ontvries alles (watersim + zonrotatie + modelrotatie)
- **B**: Toggle de zon (dag/nacht) wel/niet laten voortlopen
- **N**: Toggle de schaduwkaart (terreinschaduwen + gedempte instraling in de schaduw)
- **R**: Toggle planet rotation
- **X**: Toggle water visibility
- **C**: Toggle cloud visibility
- **1-9/0**: Weergave-overlays (zelfde keuze als de knoppenbalk onderin; de extra "Water & droesem"-overlay zit alleen in de GUI)
- **W/S**: Move camera forward/backward
- **A/D**: Move camera left/right
- **Q/E**: Move camera up/down
- **Arrows**: Rotate view
- **Enter**: Single water simulation step
- **;/'**: Adjust ground height
- **K/L**: Adjust evaporation rate
- **[ / ]**: Adjust planet rotation speed (daglengte)
- **G/H**: Adjust Coriolis strength (losgekoppeld van daglengte)
- **U/I**: Adjust solar heating strength
- **O/P**: Adjust friction (demping)
- **./,**: Adjust precipitation factor

## Testvlaggen
- `--help` / `-h`: toon een overzicht van alle vlaggetjes (ook bij een foutieve vlag).
- `--zonder-water`: start zonder water (`waterHoogte = 0`) — handig om de grond-rendering los te testen.
- `--zonder-erosie`: houdt het terrein stil (geen erosie/depositie) zodat water gedrag bekeken kan worden zonder hoogteveranderingen.
- `--zonder-leven`: zet plantengroei uit (geen groene begroeiing), handig om louter het rots/zand-erfgoed te bekijken.
- `--zonder-atmosfeer`: houdt de lucht volledig stil (geen wind, verdamping of neerslag); het water stroomt nog.
- `--zonder-schaduw`: zet de schaduwkaart uit (geen terreinschaduwen in beeld, volle zoninstraling in de simulatie).
- `--schaduwGrootte <n>`: resolutie van de schaduwkaart in pixels per zijde (standaard **4096**; hoger = scherper maar meer geheugen, 64 MB bij 4096).
- `--procedureel`: genereert het terrein met ruis i.p.v. de MOLA-hoogtekaart (geen PNG nodig). Ideaal voor snelle, kleine grids.
- `--diepte <n>`: icosahedron-onderverdelingsniveau (standaard **5**; hoger = fijner, maar trager — **9/10** betekent ~5M/20M vakjes en ±0,8/3 GB geheugen, zware machines only).
- `--diagnose`: print elke 25 frames de extremen van de reken-stand terug (water, bodem/luchtvocht, droesem, temperatuur, luchtdruk, wind, wolken) en meldt niet-eindige cellen.
- `--diagnoseCsv <bestand>`: dumpt de **hele** planeet naar een CSV (één rij per cel) zodat de berekening extern geanalyseerd kan worden. Bedoeld voor kleine grids (laag `--diepte`); `--diagnoseCsvFrames <n>` zet het interval (standaard 25).
- `--hoofdloos`: draait zonder venster (geen aqua/display nodig), bijv. `--hoofdloos --procedureel --diepte 4 --stappen 3000 --diagnoseCsv uit.csv`.
- `--stappen <n>`: stop na n rondes (samen met `--hoofdloos`).
- `--schermafbeelding <bestand>`: render (ook met `--hoofdloos`) naar een off-screen framebuffer en bewaar die als PNG, bijv. `--hoofdloos --procedureel --diepte 3 --stappen 300 --schermafbeelding beeld.png`.
- `--stil`: bevries alles vanaf het begin (sim, zon- en modelrotatie). Handig met `--hoofdloos --schermafbeeldingElkeFrames 1` om te controleren dat opeenvolgende beelden identiek zijn (geen flikker).
- `--luchtstappen <n>`: aantal atmosfeer-simstappen per beeld (standaard 1). Hoger zet de wind de damp/wolken per beeld verder, zodat je de wolkbeweging op het scherm zichtbaar sneller voorbij ziet trekken (bijv. `--luchtstappen 8`).
- `--overlay <n>`: weergave-overlay bij start (0..10), zelfde reeks als de overlay-knoppen in de GUI (1 temperatuur, 2 wind+druk, … 10 water & droesem); ook headless te gebruiken.
- `--veldKaart <veld> [bestand]`: volledige-planeet heatmap als equirectangulaire PNG (default bestandsnaam `veldkaart_<veld>.png`); herhaalbaar voor meerdere kaarten in één draai. Velden o.a. `temperatuur`, `wind`, `druk`, `grond`, `water`, `ijs`, `wolken`, `leven`, `droesem`, `zonZicht`, `oppervlakte`.
- `--kaartFactor <n>`: veldkaart-resolutie gedeeld door n (standaard 1; klem 1..40).
- `--veldKaartFrames <n>`: (hoofdloos) schrijf de laatste n frames als `veldkaart0.png` .. `veldkaartN-1.png` (grond-heatmap).
- `--veldKaartElkeFrames <n> <veld>`: (hoofdloos) schrijf elke n frames als `veldkaart_N.png` (standaard veld `grond`).
- `--conservering [tol%]`: controleer of de totale watermassa constant blijft (hoofdloos; tol% = relatieve drift, standaard 1%); retourneert exit-code **1** bij een lek.

Voorbeeld: `./build/src/mars --zonder-water --zonder-erosie --zonder-leven`
Analyse-voorbeeld: `./build/src/mars --procedureel --hoofdloos --diepte 4 --stappen 3000 --diagnoseCsv uit.csv`

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

Om de wolken zichtbaar te laten meebewegen is de wind zwaarder gedempt en
sterker gladgestreken (hoge wind-diffusie) en door een mildere drukgradiënt
aangedreven, zodat het veld **onder** de snelheidsgrens blijft en er coherente,
grootschalige circulatie-ellipsen/banden ontstaan i.p.v. dat élke cel tegelijk op
de cap wordt geperst (waar richtingen onderling tot ~0 uitmiddelen). Damp en
wolken worden daarna **semi-Lagrangiaans** (over meerdere wind-cellen tegelijk,
i.p.v. één donorcel mengen) getransporteerd, zodat wolkpatronen meetrekken met de
luchtstroom i.p.v. op hun plaats te versmeren.

De wolk-advectie-afstand (`wolkSnelheid` in `waterLucht.comp`) moet boven de
**1-cel-grens** uitkomen: ligt hij eronder, dan bekert de wolk alleen maar naar de
naaste stroomopwaartse buur (sub-cel diffusie) en lijkt hij stil te staan op zijn
plek op te laaien. Zie ook `--luchtstappen` om de beweging per beeld te
versnellen.

## Schaduwkaart & binnenkomend zonlicht
Elke frame wordt het terrein in een orthografische dieptekaart gerenderd, bekeken
vanuit de zon (de *schaduwkaart*; toets **N**, `--zonder-schaduw`,
`--schaduwGrootte`). De projectie is analytisch (`zonProjectie` in
`shaders/zonSchaduw.wgsl`): dezelfde formule in de schaduw-pass, de fragment-shaders
en de reken-shaders, dus geen matrices om uit de pas te lopen.

De kaart wordt drie keer gebruikt:
1. **Weergave**: de land/water-fragmentshader doet een 3×3 PCF-lookup en dempt het
   diffuse licht waar bergen/flanken tussen het punt en de zon staan.
2. **Energiebalans**: `luchtStroming.comp` projecteert elke cel op de kaart en
   vermenigvuldigt de instraling met de gevonden zichtfactor — dalen en
   kraterwanden in de schaduw van een berg warmen dus echt langzamer op. De fractie
   wordt per cel bewaard in `zonZicht` (voorheen ongebruikte opvulling in `vak`).
3. **Verdamping**: `waterLucht.comp` dempt het verdampingslicht met dezelfde
   `zonZicht`-factor.

`--veldKaart zonZicht` tekent de benaderde binnenkomende zonnestraling als heatmap
(1 = volle zon, 0 = volledig overschaduwd); `--diagnoseCsv` heeft er een
`zonZicht`-kolom bij. Wolken werpen voorlopig geen schaduw (hun albedo dempt de
instraling wel via de bestaande energiebalans).

## Beweegtest (wolken)
`Gereedschap/bewegingstest.py` bevestigt of de wolken *werkelijk* over de planeet
schuiven of op hun plaats blijven. Maak een dump van opeenvolgende rekenrondes
(ping→pong) en vergelijk die:

```bash
./build/src/mars --procedureel --hoofdloos --diepte 4 --stappen 700 \
    --diagnoseCsvFrames 1 --diagnoseCsv /tmp/pingpong.csv
python3 Gereedschap/bewegingstest.py --samenvatting /tmp/pingpong.csv
```

De samenvatting over de stabiele toestand meldt `persistentie` (fractie wolk-massa
op dezelfde cel als de vorige ronde) en `nieuw` (fractie op andere cellen):
statische wolken geven `persistentie ≈ 1`; bewegende wolken geven lage
`persistentie` (bv. ~0.05), hoge `nieuw` en `delta > 0`.


## Vochtcyclus in twee fasen
`luchtVocht` is damp (de capaciteit volgt de temperatuur); `wolken` is het
gecondenseerde wolkwater. Oververzadigde damp condenseert tot wolken; wolken geven
hun water af door terug te verdampen én door **regen die uitsluitend uit wolken
valt**. Zowel damp als wolken worden met het windveld geadvecteerd.

De oppervlakte-verdamping (`verdamping` in `mars.cpp`) is 10× omlaag gezet, zodat
staand water (en de damp die erdoor gevormd wordt) beduidend langer blijft staan
i.p.v. supersnel weg te dampen — de "beweging" die eerder leek te ontstaan was
vooral verdamping, geen echte wolk-windtracering.

## Wolkendek
De wolken worden getekend als een doorzichtig dek op een **absolute hoogte**(straal vanaf het planeetcentrum) die per cel uit temperatuur, druk en damp wordt
berekend — in dezelfde hoogte→straal-afbeelding als het terrein (sealevel = straal
1.0). Het dek zweeft dus in de atmosfeerlaag i.p.v. als een vast percentage boven
de grond: bergtoppen die hoger reiken dan het lokale dek steken erbovenuit en
hebben daar geen wolk. Het plafond is 90% van het hoogste terreinpunt
(`hoogsteGrond()`, bij het laden bepaald), zodat wolken nooit boven het hoogste
punt van de kaart uitkomen (`Mount Olympus` = 27 km).

## Temperatuur & ijs
- **Temperatuuroverlay** (toets **2**) kleurt het land en het wateroppervlak per
  celtemperatuur: **-25 °C blauw**, **0 °C groen**, **+25 °C rood** (kouder dan
  -25 °C klemt op blauw). IJs toont zijn eigen temperatuurkleur met een dunne
  witte contour op de rand.
- **Echte energiebalans**: ieder vak houdt zijn lucht-temperatuur bij van stap tot
  stap. Per stap komt er energie van de zon **met invalshoek** (cosinus van de
  zonhoogte; scherende straling valt over meer oppervlak), gemoduleerd door het
  **albedo** van de getoonde oppervlakte (ijs/wolken/water/grond/begroeiïng).
  Tegelijk straalt de planeet uit naar de ruimte, afgeremd door het **wolkendek**,
  en houdt de CO₂-atmosfeer (semi-geterraformd Mars) warmte vast via een
  broeikaseffect — zodat de evenaar boven het vriespunt kan komen en de polen
  ijzig blijven.
- **IJs** (altijd actief): onder het vriespunt (273 K) bevriest water tot ijs, hoe
  kouder hoe sneller; boven het vriespunt dooit het terug. IJs telt als grond voor
  de stroming (zie `kolom()` in `planeetDefinities.wgsl`), dus water stroomt er
  overheen zoals over terrein, en bevroren water kan niet weglopen of verdampen
  zolang het koud is. IJs wordt als wit deksel getoond, ook als het vak (bijna)
  geen water meer bevat.


## Erosie / ondergronden
Elke cel heeft twee lagen: een zand/sediment-deklaag boven op een diepere
rots-ondergrond (`rotsHoogte`). De planeet start geheel als blootliggende rots;
zand ontstaat pas waar water erosie-materiaal (droesem) neerlegt. Zand erodeert
snel (×1) en beschermt de rots daaronder; zodra het zand is weggespoeld erodeert
de rots zelf 100× langzamer. Zowel erosie van zand als van rots vormt droesem in
het water, en waar water droesem neerlegt wordt het altijd zand. Op het
oppervlak zie je een zachte overgang van Mars-rode rots naar zand naarmate
de zandlaag dikker wordt.

De erosie-snelheid is in totaal 10× trager gemaakt (`oplosheid` in
`planeetStructen.wgsl`): zowel zand als rots eroderen langzamer, zodat het
terrein rustiger blijft en de vorming/afvoer van sediment niet het watergedrag
overschaduwt.

## Supported Platforms
- ✅ Linux (Vulkan)
- ✅ macOS Intel & Apple Silicon (Metal)
- ❓ Windows (D3D12, untested)
