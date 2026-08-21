#pragma once

#include "imgui.h"
#include "imgui_impl_wgpu.h"

class Simulatie;
class weergaveScherm;

///Dear ImGui-overlay voor Mars: boven-statusbalk (start-parameters + "Nieuw"),
///linkerpaneel met live-tunables en een onderbalk met de overlaykeuze.
class guiOverlay {
public:
	explicit guiOverlay(Simulatie& sim);
	~guiOverlay();

	///Start een nieuw frame (DeltaTime, ImGui_ImplWGPU_NewFrame + bouwen).
	void beginFrame(float deltaTijd);

	///Tekent de GUI bovenop de getekende planeet (in de open command-encoder).
	void tekenInPass(weergaveScherm* scherm);

	bool wilToetsen() const { return _io && _io->WantCaptureKeyboard; }
	bool wilMuis() const    { return _io && _io->WantCaptureMouse; }

	//Inputverwerkers (aangeroepen vanuit weergaveScherm/emscriptenWebBackend)
	void verwerkToets(int key, int scancode, int actie, int mods);
	void verwerkMuisPos(double x, double y);
	void verwerkMuisKnop(int knop, int actie, int mods);
	void verwerkWiel(double dx, double dy);
	void verwerkChar(unsigned int codepunt);

private:
	void bouwen();

	Simulatie&   _sim;
	ImGuiContext* _context = nullptr;
	ImGuiIO*      _io      = nullptr;

	//Wereld-parameters voor "Nieuw" (via herstart)
	int   _nieuwDiepte       = 5;
	bool  _nieuwProcedureel  = true;
	float _nieuwWater        = 0.0f;
	float _nieuwBodemVocht   = 0.0f;
	float _nieuwWolk         = 0.0f;
	float _nieuwLeven        = 0.0f;
	float _nieuwIjs          = 0.0f;
	float _nieuwDamp         = 0.0f;
	float _nieuwZandDeksel   = 0.0f;
	float _nieuwTemperatuurC = 0.0f; //GUI toont Celsius
};
