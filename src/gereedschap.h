#pragma once

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
///scroll/pinch zoomt.
class verplaatsGereedschap : public gereedschap {
public:
	/// rotatieKnop = de muisknop die de trackball-sleep start (GLFW-conventie;
	/// standaard 0 = links). Het penseel gebruikt rechts om de camera vrij te houden.
	explicit verplaatsGereedschap(weergaveSchermPerspectief* scherm, int rotatieKnop = 0) : _scherm(scherm), _rotatieKnop(rotatieKnop) {}

	void muisPos(double x, double y) override;
	void muisKnop(int knop, int actie, int mods) override;
	void muisWiel(double dx, double dy) override;
	bool isBezig() const override { return _sleept; }

private:
	weergaveSchermPerspectief* _scherm;
	int    _rotatieKnop = 0;
	bool   _sleept   = false;
	double _laatsteX = 0.0;
	double _laatsteY = 0.0;
};
