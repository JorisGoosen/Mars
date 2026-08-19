#include "penseelSelectie.h"
#include "planeet.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>

float gaussiaansGewicht(float t, float hardness)
{
	if(t > 1.0f) return 0.0f;
	const float sigma = 0.55f + (0.18f - 0.55f) * hardness; // mix(0.55, 0.18, hardness)
	return std::exp(-(t * t) / (2.0f * sigma * sigma));
}

void gridSelectie::vulGewichten(planeet &geo, uint32_t centerId, std::vector<float> &gewichten) const
{
	const size_t aantal = gewichten.size();
	std::fill(gewichten.begin(), gewichten.end(), 0.0f);

	if(centerId >= aantal || _stappen == 0)
		return;

	//BFS over de buren-graaf: ring-afstand vanaf de center-cel (uint8 = ring, 255 = ongezien).
	std::vector<uint8_t> ring(aantal, 255u);
	std::deque<uint32_t> rij;

	ring[centerId] = 0;
	gewichten[centerId] = 1.0f;
	rij.push_back(centerId);

	while(!rij.empty())
	{
		uint32_t nu = rij.front();
		rij.pop_front();

		uint8_t afstand = ring[nu];
		if(afstand >= _stappen)
			continue;

		size_t burenAantal = geo.burenAantal(nu);
		for(size_t k = 0; k < burenAantal; k++)
		{
			uint32_t buur = geo.buurVan(nu, k);
			if(ring[buur] != 255u)
				continue;

			ring[buur] = (uint8_t)(afstand + 1u);
			gewichten[buur] = gaussiaansGewicht(float(afstand + 1u) / float(_stappen), _hardness);
			rij.push_back(buur);
		}
	}
}

void bolSelectie::vulGewichten(planeet &, uint32_t, std::vector<float> &) const
{
	//De bol-selectie rekent op de GPU (penseel.comp / highlight): de gewichten worden
	//daar per cel uit de verplaatste oppervlak-afstand berekend. Hier niets te vullen.
}
