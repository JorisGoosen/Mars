#include "gereedschapUitvoer.h"

static const uitvoerDefinitie uitvoeren[] = {
	{ penseelModeTerrein,     "Terrein",     true  },
	{ penseelModeRelief,      "Reliëf",      true  },
	{ penseelModeWater,       "Water",       false },
	{ penseelModeIjs,         "IJs",         false },
	{ penseelModeBodem,       "Bodemvocht",  false },
	{ penseelModeTemperatuur, "Temperatuur", false },
	{ penseelModeLeven,       "Leven",       false },
	{ penseelModeWolken,      "Wolken",      false },
	{ penseelModeDamp,        "Damp",        false },
};

const uitvoerDefinitie *uitvoerLijst()
{
	return uitvoeren;
}

size_t uitvoerAantal()
{
	return sizeof(uitvoeren) / sizeof(uitvoeren[0]);
}
