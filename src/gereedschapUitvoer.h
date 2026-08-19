#pragma once
#include "penseel.h"
#include <cstddef>

//Uitvoeringsgereedschappen: beschrijven wát een penseel per cel doet. De echte
//toepassing zit in shaders/penseel.comp; hier staat de (GUI-)metadata. De
//gereedschappen staan bewust los van de selectie en van de aanwijzer.
struct uitvoerDefinitie {
	penseelMode mode;
	const char *naam;
	bool        materiaalKeuze;   ///< toon de zand/rots-checkboxes (Terrein/Reliëf)
};

const uitvoerDefinitie *uitvoerLijst();
size_t uitvoerAantal();
