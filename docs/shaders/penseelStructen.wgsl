//Gedeelde definities voor de penseel-gereedschappen.
//Byte-identiek met de C++-spiegel in src/penseel.h (static_assert).

const penseelModeTerrein      = 1u;
const penseelModeRelief       = 2u;
const penseelModeWater        = 3u;
const penseelModeIjs          = 4u;
const penseelModeBodem        = 5u;
const penseelModeTemperatuur  = 6u;
const penseelModeLeven        = 7u;
const penseelModeWolken       = 8u;
const penseelModeDamp         = 9u;

//Bitvlaggen voor penseel.materiaal (uitvoer): welke laag Terrein/Reliëf wijzigt.
const penseelZand = 1u;
const penseelRots = 2u;

//Bitvlaggen voor penseel.oppervlak (bolselectie): welke lagen de bolhoogte vormen.
//Rots is altijd de basis; zand/water/ijs tellen er optioneel bovenop.
const oppervlakZand  = 1u;
const oppervlakWater = 2u;
const oppervlakIjs   = 4u;

//Selectie-modus voor penseel.selectie.
const selectieGrid = 0u;
const selectieBol  = 1u;

struct penseelBuffer {
    mode      : u32,
    selectie  : u32,
    materiaal : u32,
    oppervlak : u32,
    actief    : u32,   //1 = bezig (hover/schilder), 0 = niets
    kracht    : f32,
    straal    : f32,   //bolstraal in wereld-eenheden
    stappen   : f32,   //aantal buren-stappen (grid)
    hardness  : f32,   //0..1 (0 = zacht, 1 = hard)
    centerId  : u32,
    _pad0     : u32,
    _pad1     : u32,
    _pad2     : u32,
    gewichten : array<f32>,  //één per cel; alleen in grid-modus geschreven
};

//Gaussiaanse afval naar de randen: t = genormaliseerde afstand 0..1.
//hardness 0 = zacht/breed, 1 = scherpe rand; buiten t=1 is het gewicht 0.
fn gaussiaansGewicht(t : f32, hardness : f32) -> f32 {
    if(t > 1.0) { return 0.0; }
    let sigma = mix(0.55, 0.18, hardness);
    return exp(-(t * t) / (2.0 * sigma * sigma));
}
