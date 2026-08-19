#pragma once
#include <cstdint>

//Gedeelde definities voor de penseel-gereedschappen. Byte-identiek met
//shaders/penseelStructen.wgsl (penseelBuffer; header 52 bytes).

enum penseelMode : uint32_t {
	penseelModeTerrein      = 1,
	penseelModeRelief       = 2,
	penseelModeWater        = 3,
	penseelModeIjs          = 4,
	penseelModeBodem        = 5,
	penseelModeTemperatuur  = 6,
	penseelModeLeven        = 7,
	penseelModeWolken       = 8,
	penseelModeDamp         = 9,
};

//Bitvlaggen voor penseelBuffer.materiaal (uitvoer): welke laag Terrein/Reliëf wijzigt.
enum penseelMateriaal : uint32_t {
	penseelZand = 1u,
	penseelRots = 2u,
};

//Bitvlaggen voor penseelBuffer.oppervlak (bolselectie); rots is altijd de basis.
enum penseelOppervlak : uint32_t {
	oppervlakZand  = 1u,
	oppervlakWater = 2u,
	oppervlakIjs   = 4u,
};

//Selectie-modus.
enum penseelSelectieModus : uint32_t {
	selectieGrid = 0u,
	selectieBol  = 1u,
};

//Gedeelde waarde voor "geen cel" in de pick-readback (achtergrond = wit).
const uint32_t geenCelId = 0xFFFFFFFFu;

//Byte-identiek met de WGSL-struct (alle leden 4-byte aligned; 52 bytes).
struct penseelBuffer {
	uint32_t mode;
	uint32_t selectie;
	uint32_t materiaal;
	uint32_t oppervlak;
	uint32_t actief;
	float    kracht;
	float    straal;
	float    stappen;
	float    hardness;
	uint32_t centerId;
	uint32_t _pad0;
	uint32_t _pad1;
	uint32_t _pad2;
};
static_assert(sizeof(penseelBuffer) == 52, "penseelBuffer moet 52 bytes zijn (gelijk aan WGSL)");
