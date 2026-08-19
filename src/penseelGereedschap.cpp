#include "penseelGereedschap.h"
#include "planeet.h"
#include "penseelSelectie.h"
#include <GLFW/glfw3.h>
#include <cmath>

penseelGereedschap::penseelGereedschap(weergaveSchermPerspectief *scherm, planeet *geo)
	: _scherm(scherm), _geo(geo), _camera(scherm, GLFW_MOUSE_BUTTON_RIGHT)
{
	_gewichten.resize(_geo ? _geo->aantalVakjes() : 0, 0.0f);
	vulBuffer();
}

// ── Instellingen ────────────────────────────────────────────────────────────

void penseelGereedschap::zetMode(penseelMode mode)
{
	if(_mode == mode) return;
	_mode = mode;
	_bufferVuil = true;
}

void penseelGereedschap::zetSelectieModus(uint32_t modus)
{
	if(_selectieModus == modus) return;
	_selectieModus = modus;
	_bufferVuil = true;
	herbereken();
}

void penseelGereedschap::zetStappen(uint32_t stappen)
{
	if(_stappen == stappen) return;
	_stappen = stappen;
	_bufferVuil = true;
	herbereken();
}

void penseelGereedschap::zetStraal(float straal)
{
	if(_straal == straal) return;
	_straal = straal;
	_bufferVuil = true;
}

void penseelGereedschap::zetHardness(float hardness)
{
	if(_hardness == hardness) return;
	_hardness = hardness;
	_bufferVuil = true;
	herbereken();
}

void penseelGereedschap::zetKracht(float kracht)
{
	if(_kracht == kracht) return;
	_kracht = kracht;
	_bufferVuil = true;
}

void penseelGereedschap::zetMateriaal(uint32_t materiaal)
{
	if(_materiaal == materiaal) return;
	_materiaal = materiaal;
	_bufferVuil = true;
}

void penseelGereedschap::zetOppervlak(uint32_t oppervlak)
{
	if(_oppervlak == oppervlak) return;
	_oppervlak = oppervlak;
	_bufferVuil = true;
}

void penseelGereedschap::zetVerwijderModus(bool aan)
{
	if(_verwijderModus == aan) return;
	_verwijderModus = aan;
	_bufferVuil = true;
}

// ── Per-frame ───────────────────────────────────────────────────────────────

void penseelGereedschap::zetCenterId(uint32_t id)
{
	if(id >= _gewichten.size())
		id = geenCelId;

	if(_centerId != id)
	{
		_centerId = id;
		_bufferVuil = true;
		herbereken();
	}

	bool geldig = (id != geenCelId);
	if(_actief != geldig)
	{
		_actief = geldig;
		_bufferVuil = true;
	}
}

void penseelGereedschap::herbereken()
{
	if(_selectieModus == selectieGrid)
	{
		gridSelectie sel(_stappen, _hardness);
		sel.vulGewichten(*_geo, _centerId, _gewichten);
	}
	else
	{
		//bol: GPU rekent de gewichten live uit het verplaatste oppervlak; hier niets.
		std::fill(_gewichten.begin(), _gewichten.end(), 0.0f);
	}
	_bufferVuil = true;
}

void penseelGereedschap::vulBuffer()
{
	_buffer.mode      = (uint32_t)_mode;
	_buffer.selectie  = _selectieModus;
	_buffer.materiaal = _materiaal;
	_buffer.oppervlak = _oppervlak;
	_buffer.actief    = _actief ? 1u : 0u;
	_buffer.straal    = _straal;
	_buffer.stappen   = (float)_stappen;
	_buffer.hardness  = _hardness;
	_buffer.centerId  = _centerId;

	float richting = _verwijderModus ? -1.0f : 1.0f;
	if(_shift) richting = -richting;
	_buffer.kracht = std::fabs(_kracht) * richting;
}

// ── Invoer ──────────────────────────────────────────────────────────────────

void penseelGereedschap::muisPos(double x, double y)
{
	_aanwijzer.muisPos(x, y);

	if(_camera.isBezig())
		_camera.muisPos(x, y);
}

void penseelGereedschap::muisKnop(int knop, int actie, int mods)
{
	bool shift = (mods & GLFW_MOD_SHIFT) != 0;
	if(_shift != shift)
	{
		_shift = shift;
		_bufferVuil = true;
	}

	if(knop == GLFW_MOUSE_BUTTON_LEFT)
		_schildert = (actie == GLFW_PRESS);
	else if(knop == GLFW_MOUSE_BUTTON_RIGHT)
		_camera.muisKnop(knop, actie, mods);
}

void penseelGereedschap::muisWiel(double dx, double dy)
{
	_camera.muisWiel(dx, dy);
}
