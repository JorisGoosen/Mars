#include "gui.h"
#include "simulatie.h"
#include "weergaveScherm.h"
#include <GLFW/glfw3.h>
#include "imgui_internal.h"

// ── Invoermapping GLFW -> ImGui ──────────────────────────────────────────────

static ImGuiKey glfwNaarImGuiKey(int key)
{
	switch(key)
	{
	case GLFW_KEY_SPACE:       return ImGuiKey_Space;
	case GLFW_KEY_ENTER:       return ImGuiKey_Enter;
	case GLFW_KEY_ESCAPE:      return ImGuiKey_Escape;
	case GLFW_KEY_TAB:         return ImGuiKey_Tab;
	case GLFW_KEY_BACKSPACE:   return ImGuiKey_Backspace;
	case GLFW_KEY_DELETE:      return ImGuiKey_Delete;
	case GLFW_KEY_LEFT:        return ImGuiKey_LeftArrow;
	case GLFW_KEY_RIGHT:       return ImGuiKey_RightArrow;
	case GLFW_KEY_UP:          return ImGuiKey_UpArrow;
	case GLFW_KEY_DOWN:        return ImGuiKey_DownArrow;
	case GLFW_KEY_HOME:        return ImGuiKey_Home;
	case GLFW_KEY_END:         return ImGuiKey_End;
	case GLFW_KEY_PAGE_UP:     return ImGuiKey_PageUp;
	case GLFW_KEY_PAGE_DOWN:   return ImGuiKey_PageDown;
	case GLFW_KEY_SEMICOLON:   return ImGuiKey_Semicolon;
	case GLFW_KEY_APOSTROPHE:  return ImGuiKey_Apostrophe;
	case GLFW_KEY_COMMA:       return ImGuiKey_Comma;
	case GLFW_KEY_PERIOD:      return ImGuiKey_Period;
	case GLFW_KEY_SLASH:       return ImGuiKey_Slash;
	case GLFW_KEY_LEFT_BRACKET:  return ImGuiKey_LeftBracket;
	case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
	case GLFW_KEY_LEFT_SHIFT:  return ImGuiKey_LeftShift;
	case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
	case GLFW_KEY_LEFT_CONTROL:  return ImGuiKey_LeftCtrl;
	case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
	case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
	case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
	default: break;
	}

	if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z)      return (ImGuiKey)(ImGuiKey_A + (key - GLFW_KEY_A));
	if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9)      return (ImGuiKey)(ImGuiKey_0 + (key - GLFW_KEY_0));
	if(key >= GLFW_KEY_F1 && key <= GLFW_KEY_F24)   return (ImGuiKey)(ImGuiKey_F1 + (key - GLFW_KEY_F1));
	return ImGuiKey_None;
}

guiOverlay::guiOverlay(Simulatie& sim) : _sim(sim)
{
	const SimulatieConfig & cfg = sim.config();
	_nieuwDiepte      = cfg.subdiv;
	_nieuwProcedureel = cfg.procedural;
	_nieuwWater       = cfg.beginMetWater;

	_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(_context);
	_io = &ImGui::GetIO();
	//Geen toetsenbord-navigatie activeren: anders claimt ImGui alle toetsen en
	//krijgt de simulatie (spatie/B/N/...) er niets meer van. Zonder dit wordt
	//WantCaptureKeyboard alleen waar bij actief bewerkte widgets (bijv. diepte-veld).
	_io->IniFilename = nullptr; //geen sessiebestand schrijven op web/native-venster

	ImGui::StyleColorsDark();

	weergaveScherm* scherm = _sim.scherm();
	ImGui_ImplWGPU_InitInfo info;
	info.Device                = weergaveScherm::deelApparaat();
	info.NumFramesInFlight     = 2;
	info.RenderTargetFormat    = scherm ? scherm->oppervlakFormat() : WGPUTextureFormat_RGBA8Unorm;
	info.DepthStencilFormat    = WGPUTextureFormat_Undefined;
	ImGui_ImplWGPU_Init(&info);
	ImGui_ImplWGPU_CreateDeviceObjects();
}

guiOverlay::~guiOverlay()
{
	ImGui_ImplWGPU_Shutdown();
	ImGui::DestroyContext(_context);
}

void guiOverlay::beginFrame(float deltaTijd)
{
	weergaveScherm* scherm = _sim.scherm();
	_io->DeltaTime   = deltaTijd > 0.0f ? deltaTijd : 0.001f;
	_io->DisplaySize = ImVec2((float)scherm->oppervlakBreedte(), (float)scherm->oppervlakHoogte());

	ImGui_ImplWGPU_NewFrame();
	ImGui::NewFrame();

	bouwen();
}

void guiOverlay::tekenInPass(weergaveScherm* scherm)
{
	if(!_io) return;

	ImGui::Render();

	scherm->bereidGuiPass();
	if(scherm->weergavePass())
	{
		ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), scherm->weergavePass());
		scherm->pasRondRenderAf();	}
}

void guiOverlay::verwerkToets(int key, int scancode, int actie, int mods)
{
	(void)scancode; (void)mods;
	ImGuiKey imgui = glfwNaarImGuiKey(key);
	if(imgui != ImGuiKey_None)
	{
		_io->AddKeyEvent(imgui, actie == GLFW_PRESS);
		_io->SetKeyEventNativeData(imgui, key, scancode, 0);
	}
}

void guiOverlay::verwerkMuisPos(double x, double y)
{
	_io->AddMousePosEvent((float)x, (float)y);
}

void guiOverlay::verwerkMuisKnop(int knop, int actie, int mods)
{
	(void)mods;
	if(knop >= 0 && knop < 5)
		_io->AddMouseButtonEvent(knop, actie == GLFW_PRESS);
}

void guiOverlay::verwerkWiel(double dx, double dy)
{
	_io->AddMouseWheelEvent((float)dx, (float)dy);
}

void guiOverlay::verwerkChar(unsigned int codepunt)
{
	if(codepunt > 0)
		_io->AddInputCharacter((unsigned short)codepunt);
}

// ── Layouts ─────────────────────────────────────────────────────────────────

void guiOverlay::bouwen()
{
	auto t = _sim.tunables();
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	// ── Boven: start-parameters + Nieuw ──────────────────────────────────
	if(ImGui::BeginViewportSideBar("startbalk", viewport, ImGuiDir_Up, 40,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::TextUnformatted("Wereld");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::DragInt("diepte", &_nieuwDiepte, 0.05f, 1, 8);
		ImGui::SameLine();
		ImGui::Checkbox("procedureel", &_nieuwProcedureel);
		ImGui::SameLine();
		ImGui::Checkbox("water", &_nieuwWater);
		ImGui::SameLine();
		ImGui::Checkbox("erosie", t.erosieAan);
		ImGui::SameLine();
		ImGui::Checkbox("leven", t.levenAan);
		ImGui::SameLine();
		ImGui::Checkbox("atmosfeer", t.atmosfeerAan);
		ImGui::SameLine();
		ImGui::Checkbox("schaduw", t.schaduwAan);
		ImGui::SameLine();
		if(ImGui::Button("Nieuw"))
		{
			SimulatieConfig c = _sim.config();
			c.subdiv       = _nieuwDiepte;
			c.procedural   = _nieuwProcedureel;
			c.beginMetWater= _nieuwWater;
			_sim.herstart(c);
		}
		ImGui::SameLine();
		ImGui::TextUnformatted("fps");
		ImGui::SameLine();
		ImGui::Text("%.1f (%.2f ms)", _sim.fps(), _sim.frameTijdMS());
	}
	ImGui::End();

	// ── Links: live-tunables, gegroepeerd per categorie ──────────────────
	if(ImGui::BeginViewportSideBar("tunabalk", viewport, ImGuiDir_Left, 330,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::TextUnformatted("Live-tunables");
		ImGui::Separator();

		if(ImGui::CollapsingHeader("Zon & verwarming", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("zonkracht",   t.zonKracht, 0.0f, 200.0f, "%.1f");
			ImGui::SliderFloat("winterzon",   t.winterZonneKracht, 0.0f, 200.0f, "%.1f");
			ImGui::SliderFloat("askanteling", t.obliquity, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("verwarmtijd", t.verwarmtijd, 0.01f, 1.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Atmosfeer", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("rotatieOmega", t.rotatieOmega, 0.0f, 0.2f, "%.4f");
			ImGui::SliderFloat("coriolis",     t.coriolisOmega, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("wrijving",     t.wrijving, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("diffusie",     t.diffusie, 0.0f, 1.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Water & klimaat", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("verdamping",    t.verdamping, 0.0f, 0.01f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("basisverzadiging", t.basisVerzadiging, 0.0f, 0.5f, "%.3f");
			ImGui::SliderFloat("hoogtekoel",    t.hoogteKoel, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("neerslag",      t.neerslagFactor, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("orografie",     t.orografieFactor, 0.0f, 1.0f, "%.2f");
			//Bodem & infiltratie
			ImGui::SliderFloat("evapotranspiratie", t.evapotranspiratie, 0.0f, 0.01f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("infiltratie",   t.infiltratie, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("bodemdiffusie", t.bodemDiffusie, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("veldcapaciteit", t.veldCapaciteit, 0.0f, 2.0f, "%.2f");
			//Wolken (twee-fasen vocht)
			ImGui::SliderFloat("condensatie",   t.condensTempo, 0.0f, 0.1f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("regentempo",    t.regenTempo, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("wolkverdamping",t.wolkVerdamp, 0.0f, 0.1f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("wolkdiffusie",  t.wolkDiffusie, 0.0f, 2.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Erosie & sediment", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("zanderosie",    t.zandErosie, 0.0f, 0.01f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("rotserosie",    t.rotsErosie, 0.0f, 0.001f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("bezinkheid",    t.bezinkheid, 0.0f, 0.01f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("zandrusthelling", t.zandRepose, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("hellingkracht", t.hellingKracht, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("oplosheid",     t.oplosheid, 0.0f, 2.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Leven & groei", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("groeiband",     t.levenGroeiBand, 0.0f, 30.0f, "%.1f");
			ImGui::SliderFloat("droogtestreft", t.levenDroogTempo, 0.0f, 0.01f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("verwelkdrempel",t.levenVerwelk, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("koudestreft",   t.levenKoudTempo, 0.0f, 2.0f, "%.2f");
			ImGui::Separator();
			ImGui::TextUnformatted("Groei & burengroei");
			ImGui::SliderFloat("zandgroei",     t.zandGroei, 0.9f, 1.1f, "%.4f");
			ImGui::SliderFloat("zandburen",     t.zandBuur, 0.0f, 0.2f, "%.4f");
			ImGui::SliderFloat("rotsgroei",     t.rotsGroei, 0.9f, 1.1f, "%.4f");
			ImGui::SliderFloat("rotsburen",     t.rotsBuur, 0.0f, 0.05f, "%.4f");
		}
		if(ImGui::CollapsingHeader("Terrein", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("grondmult",  t.grondMult, 1.0f, 1000.0f, "%.0f");
			ImGui::SliderFloat("grondschaal", t.grondSchaal, 0.1f, 5.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Weergave", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("bevroren",   t.bevroren);
			ImGui::SameLine();
			ImGui::Checkbox("water zichtbaar", t.tekenWater);
			ImGui::Checkbox("wolken zichtbaar", t.tekenWolken);
			ImGui::SameLine();
			ImGui::Checkbox("schaduw", t.schaduwAan);
			ImGui::Checkbox("zon-rotatie", t.zonRoteert);
			ImGui::SameLine();
			ImGui::Checkbox("planeet-rotatie", t.roteerMaar);
			if(ImGui::Button("Eén sim-stap"))
				*t.waterStap = true;
			ImGui::SameLine();
			ImGui::Text("luchtstappen");
			ImGui::SameLine();
			int lucht = (int)*t.luchtStappen;
			ImGui::SetNextItemWidth(60);
			if(ImGui::DragInt("##lucht", &lucht, 1.0f, 1, 100))
				*t.luchtStappen = (size_t)std::max(1, lucht);
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Stats");
		ImGui::Text("vakjes: %zu", _sim.aantalVakjes());
		ImGui::Text("hoogste grond: %.1f", _sim.hoogsteGrond());
	}
	ImGui::End();

	// ── Onder: overlaykeuze (knoppen met highlight) ──────────────────────
	if(ImGui::BeginViewportSideBar("overlaybalk", viewport, ImGuiDir_Down, 38,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		static const char* overlays[10] = {
			"Natuurlijk", "Temperatuur", "Wind & druk", "Bodemvocht",
			"Lucht/Wolken", "IJs/Water", "Wolken", "ZonZicht", "Leven", "Hoogte"
		};
		const ImVec4 actiefAchterG = ImVec4(0.25f, 0.55f, 0.90f, 1.0f);  //opvallend blauw
		const ImVec4 actiefTekst    = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

		ImGui::BeginChild("ovk", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
		for(int i = 0; i < 10; i++)
		{
			const bool gekozen = (*t.overlayKeuze == i);
			if(gekozen)
			{
				ImGui::PushStyleColor(ImGuiCol_Button,      actiefAchterG);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, actiefAchterG);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,  actiefAchterG);
				ImGui::PushStyleColor(ImGuiCol_Text,        actiefTekst);
			}
			if(ImGui::Button(overlays[i]))
			{
				*t.overlayKeuze = i;
				std::cout << "Overlay: " << overlays[i] << std::endl;
			}
			if(gekozen)
				ImGui::PopStyleColor(4);
			if(i < 9) ImGui::SameLine();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}
