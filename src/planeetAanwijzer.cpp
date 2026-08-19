#include "planeetAanwijzer.h"
#include <algorithm>

int planeetAanwijzer::texelX(int breedte) const
{
	if(breedte <= 0) return 0;
	return std::clamp((int)_x, 0, breedte - 1);
}

int planeetAanwijzer::texelY(int hoogte) const
{
	if(hoogte <= 0) return 0;
	return std::clamp((int)_y, 0, hoogte - 1);
}

uint32_t planeetAanwijzer::decode(const unsigned char rgba[4])
{
	return (uint32_t)rgba[0]
	     | ((uint32_t)rgba[1] << 8)
	     | ((uint32_t)rgba[2] << 16)
	     | ((uint32_t)rgba[3] << 24);
}

bool planeetAanwijzer::geldig(uint32_t id, size_t aantalVakjes)
{
	return id != geenCelId && id < (uint32_t)aantalVakjes;
}
