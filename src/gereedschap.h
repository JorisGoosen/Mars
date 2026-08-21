#pragma once

#include "planeetAanwijzer.h"
#include <functional>

class weergaveSchermPerspectief;

///Basis voor interactieve gereedschappen (muis/trackpad-bediening van de wereld).
///Nieuwe gereedschappen erven hiervan; Simulatie routerd de invoer ernaartoe
///zodra de GUI (ImGui) de muis niet zelf opeist.
class gereedschap {
public:
	virtual ~gereedschap() = default;

	///Muisbeweging (beeldpixels). Wordt ook buiten een sleep aangeroepen zodat
	///het gereedschap zijn ankerpunt bij kan houden.
	virtual void muisPos(double x, double y) { (void)x; (void)y; }
	///Muisknop (GLFW-conventie: knop 0 = links; actie GLFW_PRESS/GLFW_RELEASE).
	virtual void muisKnop(int knop, int actie, int mods) { (void)knop; (void)actie; (void)mods; }
	///Scrollwiel/two-finger swipe (dx = horizontaal, dy = verticaal).
	virtual void muisWiel(double dx, double dy) { (void)dx; (void)dy; }
	///Of het gereedschap midden in een interactie zit (sleep o.i.d.) — dan blijft
	///de muis ook boven de GUI heen naar het gereedschap gaan (loslaten telt door).
	virtual bool isBezig() const { return false; }
};

///Verplaats-gereedschap: trackball-sleep roteert de wereld (het aangeklikte
///oppervlak volgt de muis), horizontale swipe roteert om de Y-as en verticale
///scroll/pinch zoomt. Bij het begin van een sleep beslist Simulatie via een
///pick-pass de modus: een geraakte CEL = de normale cameradraai, de ACHTERGROND
///= de zichtbare zon roteert om de origin (via de zon-roteer-callback). De
///eerste delta wordt gebufferd tot die modus bekend is, zodat niets verloren gaat.
class verplaatsGereedschap : public gereedschap {
public:
	enum modus { mOnbekend, mPlaneet, mZon };

	/// rotatieKnop = de muisknop die de trackball-sleep start (GLFW-conventie;
	/// standaard 0 = links). Het penseel gebruikt rechts om de camera vrij te houden.
	explicit verplaatsGereedschap(weergaveSchermPerspectief* scherm, int rotatieKnop = 0) : _scherm(scherm), _rotatieKnop(rotatieKnop) {}

	void muisPos(double x, double y) override;
	void muisKnop(int knop, int actie, int mods) override;
	void muisWiel(double dx, double dy) override;
	bool isBezig() const override { return _sleept; }

	bool modusOnbekend() const { return _modus == mOnbekend; }
	///Zet de modus (Simulatie, na de pick); flusht de gebufferde eerste delta.
	void zetModus(modus m);

	///Callback voor de zon-modus: (dx, dy) in beeldpixels sinds de vorige muisPos.
	void zetZonRoteer(std::function<void(double, double)> f) { _zonRoteer = std::move(f); }

	//Cursor-positie omgezet naar pick-texelcoördinaten (voor de pick-pass).
	int  cursorTexelX(int breedte) const { return _aanwijzer.texelX(breedte); }
	int  cursorTexelY(int hoogte)  const { return _aanwijzer.texelY(hoogte); }

private:
	void _stuurDelta(double dx, double dy);

	weergaveSchermPerspectief* _scherm;
	int    _rotatieKnop = 0;
	bool   _sleept   = false;
	double _laatsteX = 0.0;
	double _laatsteY = 0.0;

	modus   _modus  = mOnbekend;
	double  _wachtX = 0.0, _wachtY = 0.0; //gebufferde delta tot de pick de modus beslist
	planeetAanwijzer _aanwijzer;          //muis→texel-vertaling voor de pick
	std::function<void(double, double)> _zonRoteer;
};