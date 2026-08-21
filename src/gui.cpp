#include "gui.h"
#include "simulatie.h"
#include "weergaveScherm.h"
#include "penseelGereedschap.h"
#include "gereedschapUitvoer.h"
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
	_nieuwDiepte       = cfg.subdiv;
	_nieuwProcedureel  = cfg.procedural;
	_nieuwWater        = cfg.startWater;
	_nieuwBodemVocht   = cfg.startBodemVocht;
	_nieuwWolk         = cfg.startWolken;
	_nieuwLeven        = cfg.startLeven;
	_nieuwIjs          = cfg.startIjs;
	_nieuwDamp         = cfg.startDamp;
	_nieuwZandDeksel   = cfg.startZandDeksel;
	_nieuwTemperatuurC = cfg.startTemperatuur - 273.15f;

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
	//NB: DisplayFramebufferScale blijft (1,1): DisplaySize is al in framebuffer-
	//pixels, en de ImGui-backend schaalt de scissor/viewport met deze factor mee
	//(anders 2× = validatiefout). De muis wordt in verwerkMuisPos naar framebuffer geschaald.

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
	//Muis-coördinaten zijn vensterpunten; ImGui werkt op framebuffer-pixels (DisplaySize).
	glm::vec2 schaal = _sim.scherm()->inhoudSchaal();
	_io->AddMousePosEvent((float)(x * schaal.x), (float)(y * schaal.y));
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

	// ── Boven: start-parameters + Nieuw (rij 1) / sim-schakelaars (rij 2) ──
	if(ImGui::BeginViewportSideBar("startbalk", viewport, ImGuiDir_Up, 62,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		auto initFloat = [](const char* label, float* v, float vMin, float vMax)
		{
			//Label expliciet links (TextUnformatted) + unieke ID via PushID, zodat de
			//positie consistent is en er geen ID-botsingen zijn met de sim-checkboxes.
			ImGui::SameLine();
			ImGui::TextUnformatted(label);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(58);
			ImGui::PushID(label);
			ImGui::DragFloat("##waarde", v, 0.05f, vMin, vMax, "%.2f");
			ImGui::PopID();
		};

		ImGui::TextUnformatted("Wereld");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::DragInt("diepte", &_nieuwDiepte, 0.05f, 1, 10);
		ImGui::SameLine();
		ImGui::Checkbox("procedureel", &_nieuwProcedureel);
		initFloat("water",   &_nieuwWater,        0.0f, 20.0f);
		initFloat("bodem",   &_nieuwBodemVocht,   0.0f, 5.0f);
		initFloat("wolk",    &_nieuwWolk,         0.0f, 5.0f);
		initFloat("leven",   &_nieuwLeven,        0.0f, 1.0f);
		initFloat("ijs",     &_nieuwIjs,          0.0f, 20.0f);
		initFloat("damp",    &_nieuwDamp,         0.0f, 5.0f);
		initFloat("zand",    &_nieuwZandDeksel,   0.0f, 20.0f);
		initFloat("temp",    &_nieuwTemperatuurC, -100.0f, 100.0f);
		ImGui::SameLine();
		if(ImGui::Button("Nieuw"))
		{
			SimulatieConfig c = _sim.config();
			c.subdiv          = _nieuwDiepte;
			c.procedural      = _nieuwProcedureel;
			c.startWater      = _nieuwWater;
			c.startBodemVocht = _nieuwBodemVocht;
			c.startWolken     = _nieuwWolk;
			c.startLeven      = _nieuwLeven;
			c.startIjs        = _nieuwIjs;
			c.startDamp       = _nieuwDamp;
			c.startZandDeksel = _nieuwZandDeksel;
			c.startTemperatuur = _nieuwTemperatuurC + 273.15f;
			_sim.herstart(c);
		}

		// Rij 2: simulatie-schakelaars (niet de init — die staat op rij 1).
		ImGui::NewLine();
		ImGui::TextUnformatted("Simulatie");
		ImGui::SameLine();
		ImGui::Checkbox("water", t.waterStroomt);
		ImGui::SameLine();
		ImGui::Checkbox("erosie", t.erosieAan);
		ImGui::SameLine();
		ImGui::Checkbox("leven", t.levenAan);
		ImGui::SameLine();
		ImGui::Checkbox("atmosfeer", t.atmosfeerAan);
		ImGui::SameLine();
		ImGui::Checkbox("schaduw", t.schaduwAan);
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
			ImGui::SliderFloat("rotatieOmega", t.rotatieOmega, 0.0f, 1.0f, "%.4f");
			ImGui::SliderFloat("coriolis",     t.coriolisOmega, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("wrijving",     t.wrijving, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("diffusie",     t.diffusie, 0.0f, 1.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Water & klimaat", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("verdamping",    t.verdamping, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("basisverzadiging", t.basisVerzadiging, 0.0f, 0.5f, "%.3f");
			ImGui::SliderFloat("hoogtekoel",    t.hoogteKoel, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("neerslag",      t.neerslagFactor, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("orografie",     t.orografieFactor, 0.0f, 1.0f, "%.2f");
			//Bodem & infiltratie
			ImGui::SliderFloat("evapotranspiratie", t.evapotranspiratie, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("infiltratie",   t.infiltratie, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("bodemdiffusie", t.bodemDiffusie, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("veldcapaciteit", t.veldCapaciteit, 0.0f, 2.0f, "%.2f");
			//Wolken (twee-fasen vocht)
			ImGui::SliderFloat("condensatie",   t.condensTempo, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("regentempo",    t.regenTempo, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("wolkverdamping",t.wolkVerdamp, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("wolkdiffusie",  t.wolkDiffusie, 0.0f, 1.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Erosie & sediment", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("zanderosie",    t.zandErosie, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("rotserosie",    t.rotsErosie, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("bezinkheid",    t.bezinkheid, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("zandrusthelling", t.zandRepose, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("hellingkracht", t.hellingKracht, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("oplosheid",     t.oplosheid, 0.0f, 2.0f, "%.2f");
		}
		if(ImGui::CollapsingHeader("Leven & groei", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("groeiband",     t.levenGroeiBand, 0.0f, 30.0f, "%.1f");
			ImGui::SliderFloat("droogtestreft", t.levenDroogTempo, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("verwelkdrempel",t.levenVerwelk, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("koudestreft",   t.levenKoudTempo, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("watersterfte",  t.waterDoodTempo, 0.0f, 5.0f, "%.2f");
			ImGui::Separator();
			ImGui::TextUnformatted("Water & transpiratie");
			ImGui::SliderFloat("levensdamp",    t.levensDamp, 0.0f, 0.05f, "%f", ImGuiSliderFlags_Logarithmic);
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
		if(ImGui::CollapsingHeader("Gereedschap (penseel)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			penseelGereedschap *p = _sim.penseel();

			// ── Gereedschapskeuze ────────────────────────────────────────
			int huidig = _sim.penseelActief() && p ? (int)p->mode() : 0;
			const char *keuzes = "Verplaatsen\0Terrein\0Reliëf\0Water\0IJs\0Bodemvocht\0"
			                     "Temperatuur\0Leven\0Wolken\0Damp\0";
			if(ImGui::Combo("gereedschap", &huidig, keuzes))
			{
				if(huidig == 0)
					_sim.zetPenseelActief(false);
				else
				{
					if(p) p->zetMode((penseelMode)huidig);
					_sim.zetPenseelActief(true);
				}
			}

			if(p && _sim.penseelActief())
			{
				// ── Selectie-modus ────────────────────────────────────────
				bool bol = (p->selectieModus() == (uint32_t)selectieBol);
				if(ImGui::RadioButton("Grid (buren-stappen)", !bol)) p->zetSelectieModus(selectieGrid);
				ImGui::SameLine();
				if(ImGui::RadioButton("Bol (straal)", bol)) p->zetSelectieModus(selectieBol);

				if(!bol)
				{
					int stappen = (int)p->stappen();
					if(ImGui::DragInt("stappen", &stappen, 1.0f, 1, 30))
						p->zetStappen((uint32_t)stappen);
				}
				else
				{
					float straal = p->straal();
					if(ImGui::SliderFloat("straal", &straal, 0.005f, 1.0f, "%.3f"))
						p->zetStraal(straal);

					bool oz = (p->oppervlak() & oppervlakZand) != 0;
					bool ow = (p->oppervlak() & oppervlakWater) != 0;
					bool oi = (p->oppervlak() & oppervlakIjs) != 0;
					ImGui::TextUnformatted("Bol-oppervlak (rots = basis)");
					if(ImGui::Checkbox("zand", &oz))
						p->zetOppervlak((p->oppervlak() & ~oppervlakZand) | (oz ? oppervlakZand : 0u));
					ImGui::SameLine();
					if(ImGui::Checkbox("water", &ow))
						p->zetOppervlak((p->oppervlak() & ~oppervlakWater) | (ow ? oppervlakWater : 0u));
					ImGui::SameLine();
					if(ImGui::Checkbox("ijs", &oi))
						p->zetOppervlak((p->oppervlak() & ~oppervlakIjs) | (oi ? oppervlakIjs : 0u));
				}

				// ── Gemeenschappelijke penseel-params ─────────────────────
				float hardness = p->hardness();
				if(ImGui::SliderFloat("hardness", &hardness, 0.0f, 1.0f, "%.2f"))
					p->zetHardness(hardness);

				float kracht = p->kracht();
				if(ImGui::SliderFloat("kracht", &kracht, 0.0f, 1.0f, "%.3f"))
					p->zetKracht(kracht);

				bool verwijder = p->verwijderModus();
				if(ImGui::Checkbox("verwijderen", &verwijder))
					p->zetVerwijderModus(verwijder);
				ImGui::TextWrapped("Links-sleep = schilderen, Shift+links = verwijderen; "
				                   "rechts-sleep draait, wiel zoomt.");

				// ── Materiaalkeuze (Terrein/Reliëf) ──────────────────────
				if(p->mode() == penseelModeTerrein || p->mode() == penseelModeRelief)
				{
					bool zand = (p->materiaal() & penseelZand) != 0;
					bool rots = (p->materiaal() & penseelRots) != 0;
					ImGui::TextUnformatted("Bewerken");
					if(ImGui::Checkbox("zand", &zand))
						p->zetMateriaal((p->materiaal() & ~penseelZand) | (zand ? penseelZand : 0u));
					ImGui::SameLine();
					if(ImGui::Checkbox("rots", &rots))
						p->zetMateriaal((p->materiaal() & ~penseelRots) | (rots ? penseelRots : 0u));
				}
			}
		}
		if(ImGui::CollapsingHeader("Weergave", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("bevroren",   t.bevroren);
			ImGui::SameLine();
			ImGui::Checkbox("water zichtbaar", t.tekenWater);
			ImGui::Checkbox("ijs zichtbaar",   t.tekenIjs);
			ImGui::SameLine();
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

		ImGui::Separator();
		if(ImGui::CollapsingHeader("Besturing"))
		{
			ImGui::TextUnformatted("Muis & trackpad");
			ImGui::BulletText("Klik+slepen = planeet draaien (trackball)");
			ImGui::BulletText("Horizontale swipe = roteren");
			ImGui::BulletText("Verticaal scrollen / pinch = zoomen");
			ImGui::BulletText("Penseel (paneel 'Gereedschap'): links-sleep = schilderen, Shift+links = verwijderen, rechts-sleep = draaien, wiel = zoomen");
			ImGui::Separator();
			ImGui::TextUnformatted("Toetsen");
			ImGui::BulletText("WASD/QE = bewegen | pijltjes = draaien");
			ImGui::BulletText("Space = pauzeer/start | B = zon | N = schaduw");
			ImGui::BulletText("R = rotatie | X = water | C = wolken");
			ImGui::BulletText("1-9/0 = overlays | Enter = stap | ;/' = hoogte");
			ImGui::BulletText("K/L = verdamping | [/] = dagduur | G/H = coriolis");
			ImGui::BulletText("U/I = zonkracht | O/P = wrijving | ./ = neerslag");
			ImGui::Separator();
			ImGui::TextWrapped("Let op: diepte 9/10 betekent ~5M/20M vakjes "
			                   "(ongeveer 0,8/3 GB) — de meeste machines halen dat niet.");
		}
	}
	ImGui::End();

	// ── Onder: overlaykeuze (knoppen met highlight) ──────────────────────
	if(ImGui::BeginViewportSideBar("overlaybalk", viewport, ImGuiDir_Down, 38,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		static const char* overlays[11] = {
			"Natuurlijk", "Temperatuur", "Wind & druk", "Bodemvocht",
			"Lucht/Wolken", "IJs/Water", "Wolken", "ZonZicht", "Leven", "Hoogte",
			"Water & droesem"
		};
		const ImVec4 actiefAchterG = ImVec4(0.25f, 0.55f, 0.90f, 1.0f);  //opvallend blauw
		const ImVec4 actiefTekst    = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

		ImGui::BeginChild("ovk", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
		for(int i = 0; i < 11; i++)
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
			if(i < 10) ImGui::SameLine();
		}

		//Rechts: doorzichtigheid van het wolkendek (0 = onzichtbaar, 1 = dekkend).
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180.0f);
		ImGui::SetNextItemWidth(110);
		ImGui::SliderFloat("wolk-doorzichtig", t.wolkAlpha, 0.0f, 1.0f, "%.2f");
		ImGui::EndChild();
	}
	ImGui::End();
}
