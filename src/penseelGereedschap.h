#pragma once
#include "gereedschap.h"
#include "penseel.h"
#include "planeetAanwijzer.h"
#include <cstdint>
#include <vector>

class planeet;
class weergaveSchermPerspectief;

///Interactief penseel-gereedschap: lijmt aanwijzer (punt-selectie), selectie-module
///(grid/bol) en uitvoer-mode aan de muis. Links-sleep schildert, Shift+links
///verwijdert, rechts-sleep draait en het wiel zoomt (via de ingebedde camera).
class penseelGereedschap : public gereedschap {
public:
	penseelGereedschap(weergaveSchermPerspectief *scherm, planeet *geo);

	// ── gereedschap-overrides ─────────────────────────────────────────────
	void muisPos(double x, double y) override;
	void muisKnop(int knop, int actie, int mods) override;
	void muisWiel(double dx, double dy) override;
	bool isBezig() const override { return _schildert || _camera.isBezig(); }

	// ── Instellingen (GUI) ────────────────────────────────────────────────
	void zetMode(penseelMode mode);
	void zetSelectieModus(uint32_t modus);
	void zetStappen(uint32_t stappen);
	void zetStraal(float straal);
	void zetHardness(float hardness);
	void zetKracht(float kracht);
	void zetMateriaal(uint32_t materiaal);
	void zetOppervlak(uint32_t oppervlak);
	void zetVerwijderModus(bool aan);

	penseelMode mode() const            { return _mode; }
	uint32_t    selectieModus() const   { return _selectieModus; }
	uint32_t    stappen() const         { return _stappen; }
	float       straal() const          { return _straal; }
	float       hardness() const        { return _hardness; }
	float       kracht() const          { return _kracht; }
	uint32_t    materiaal() const       { return _materiaal; }
	uint32_t    oppervlak() const       { return _oppervlak; }
	bool        verwijderModus() const  { return _verwijderModus; }

	// ── Per-frame vanuit Simulatie ─────────────────────────────────────────
	void zetCenterId(uint32_t id);      ///< resultaat van de pick-readback (of geenCelId)
	bool schildert() const              { return _schildert; }
	bool heeftCursor() const            { return _actief; }
	void vulBuffer();                   ///< vult de staging-buffer met de actuele params
	void herbereken();                  ///< herrekent de grid-gewichten (indien grid-modus)

	const penseelBuffer & buffer() const { return _buffer; }
	const std::vector<float> & gewichten() const { return _gewichten; }
	size_t bufferGrootte() const        { return sizeof(penseelBuffer) + _gewichten.size() * sizeof(float); }
	bool bufferVuil() const             { return _bufferVuil; }
	void markeerSchoon()                { _bufferVuil = false; }

	// ── Aanwijzer (punt-selectie op de bol) ────────────────────────────────
	int  texelX(int breedte) const { return _aanwijzer.texelX(breedte); }
	int  texelY(int hoogte)  const { return _aanwijzer.texelY(hoogte); }
	bool heeftPlek() const         { return _aanwijzer.heeftPlek(); }

private:
	weergaveSchermPerspectief *_scherm;
	planeet * _geo;

	verplaatsGereedschap _camera;   ///< rechts-sleep = draaien, wiel = zoomen
	planeetAanwijzer     _aanwijzer;

	//Instellingen
	penseelMode _mode = penseelModeTerrein;
	uint32_t    _selectieModus = selectieGrid;
	uint32_t    _stappen  = 3;
	float       _straal   = 0.15f;
	float       _hardness = 0.5f;
	float       _kracht   = 0.05f;
	uint32_t    _materiaal = penseelZand | penseelRots;
	uint32_t    _oppervlak = oppervlakZand | oppervlakWater | oppervlakIjs;
	bool        _verwijderModus = false;

	//Invoer-toestand
	bool _schildert  = false;
	bool _shift      = false;
	bool _actief     = false;   ///< cursor/highlight zichtbaar (geldige center-cel)
	uint32_t _centerId = geenCelId;

	penseelBuffer _buffer{};
	std::vector<float> _gewichten;
	bool _bufferVuil = true;
};
