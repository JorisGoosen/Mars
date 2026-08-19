#pragma once
#include "penseel.h"
#include <cstddef>
#include <cstdint>

//Punt-selectie op de bol: houdt de muispositie bij en vertaalt die naar de texel
//die de GPU-pick-pass moet uitlezen + decodeert het teruggelezen RGBA-bytes naar
//een cel-ID. De eigenlijke pick-pass en readback zitten in Simulatie (die bezit de
//GPU-bronnen); deze module is puur de muis→cel-vertaling.
class planeetAanwijzer {
public:
	void muisPos(double x, double y) { _x = x; _y = y; _heeftPlek = true; }

	bool heeftPlek() const { return _heeftPlek; }

	///De texel-coördinaten onder de cursor (geklemd binnen het scherm).
	int texelX(int breedte) const;
	int texelY(int hoogte) const;

	///Decodeert RGBA8 (4 bytes) naar een cel-ID; 0xFFFFFFFF = geen cel (achtergrond).
	static uint32_t decode(const unsigned char rgba[4]);

	///Of een gedecodeerd ID een geldige cel is.
	static bool geldig(uint32_t id, size_t aantalVakjes);

private:
	double _x = 0.0, _y = 0.0;
	bool   _heeftPlek = false;
};
