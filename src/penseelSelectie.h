#pragma once
#include "penseel.h"
#include <cstdint>
#include <vector>

class planeet;

//Gaussiaanse afval naar de randen: t = genormaliseerde afstand 0..1. hardness 0 =
//zacht/breed, 1 = scherpe rand; buiten t=1 is het gewicht 0. Byte-gelijk aan
//gaussiaansGewicht in shaders/penseelStructen.wgsl.
float gaussiaansGewicht(float t, float hardness);

//Selectie-modules: bepalen wélke cellen een penseel raakt. De grid-module vult de
//gewichten-buffer via een BFS over de buren-graaf; de bol-module rekent op de GPU
//(afstand over het verplaatste oppervlak) en vult hier niets — ze draagt alleen de
//parameters (straal/oppervlak/hardness) over.
class penseelSelectie {
public:
	virtual ~penseelSelectie() = default;
	virtual uint32_t selectieModus() const = 0;
	virtual void vulGewichten(planeet &geo, uint32_t centerId, std::vector<float> &gewichten) const = 0;
};

class gridSelectie : public penseelSelectie {
public:
	gridSelectie(uint32_t stappen, float hardness) : _stappen(stappen), _hardness(hardness) {}
	uint32_t selectieModus() const override { return selectieGrid; }
	void vulGewichten(planeet &geo, uint32_t centerId, std::vector<float> &gewichten) const override;
private:
	uint32_t _stappen;
	float    _hardness;
};

class bolSelectie : public penseelSelectie {
public:
	bolSelectie(float straal, uint32_t oppervlak, float hardness)
		: _straal(straal), _oppervlak(oppervlak), _hardness(hardness) {}
	uint32_t selectieModus() const override { return selectieBol; }
	void vulGewichten(planeet &geo, uint32_t centerId, std::vector<float> &gewichten) const override; // no-op: GPU rekent
private:
	float    _straal;
	uint32_t _oppervlak;
	float    _hardness;
};
