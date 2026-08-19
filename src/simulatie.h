#pragma once

#include "planeet.h"
#include "weergaveSchermPerspectief.h"
#include <webgpu.h>
#include <functional>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <chrono>

// ── Shader-parameters (moet 96 bytes zijn, gelijk aan WGSL-struct) ──────────

struct rekenParameters {
	float	grondSchaal, verdamping, erosie, levenAan;
	float	atmosfeer[4];   //(zonkracht, rotatieOmega, wrijving, diffusie)
	float	zonRicht[4];    //zonrichting (dagzijde; de planeet draait t.o.v. de zon)
	float	condenseer[4];  //basisVerzadiging, hoogteKoel, neerslagFactor, orografieFactor
	float	fasen[4];       //verwarmtijdconstante, maxGrondHoogte, grondMult, ongebruikt
	float	schaduw[4];     //schaduwAan, schaduwKaartGrootte, ongebruikt, ongebruikt

	// ── Runtimetunables (GUI-sliders; defaults wijken de WGSL-waarden af) ──
	float	erosiePar[4];   //(zandErosie, rotsErosie, bezinkheid, zandRepose)
	float	erosiePar2[4];  //(hellingKracht, oplosheid, ongebruikt, ongebruikt)
	float	waterPar[4];    //(evapotranspiratie, infiltratie, bodemDiffusie, veldCapaciteit)
	float	levenPar[4];    //(levenGroeiBand, levenDroogTempo, levenVerwelk, levenKoudTempo)
	float	groeiPar[4];    //(zandGroei, zandBuur, rotsGroei, rotsBuur) — leven+burengroei
	float	wolkPar[4];     //(condensTempo, regenTempo, wolkVerdamp, wolkDiffusie)
};
static_assert(sizeof(rekenParameters) == 96 + 6 * 16, "rekenParameters moet byte-identiek zijn aan WGSL (96 + 6 vec4)");

// ── Configuratie ────────────────────────────────────────────────────────────

struct SimulatieConfig {
	bool                beginMetWater     = true;
	bool                erosieAan         = true;
	bool                levenAan          = true;
	bool                atmosfeerAan      = true;
	bool                procedural        = false;
	bool                hoofdloos         = false;
	int                 subdiv            = 5;
	int                 schaduwGrootte    = 4096;
	bool                schaduwAan        = true;
	std::string         csvBestand;
	std::string         schermafbeeldingBestand;
	size_t              schermElkeFrames  = 0;
	size_t              stappenTotaal     = 0;   // 0 = oneindig (interactief)
	size_t              csvElkeFrames     = 25;
	size_t              luchtStappen      = 1;
	bool                bevroren          = false;
	bool                conservatieAan    = false;
	double              conservatieTol    = 0.01;
};

// ── Interne structuren voor async-callbacks ──────────────────────────────────

struct diagToestandje {
	WGPUBuffer  buffer = nullptr;
	size_t      grootte = 0;
	planeet*    geo = nullptr;
	size_t      teller = 0;
	bool        klaar = false;
};

struct csvToestandje {
	WGPUBuffer  buffer = nullptr;
	size_t      grootte = 0;
	planeet*    geo = nullptr;
	size_t      teller = 0;
	bool        klaar = false;
	std::ofstream* uit = nullptr;
};

struct conservatieStat {
	double water = 0, bodem = 0, ijs = 0, damp = 0, wolk = 0;
	size_t teller = 0;
};

struct conservatieToestandje {
	WGPUBuffer      buffer = nullptr;
	size_t          grootte = 0;
	bool            klaar = false;
	conservatieStat* stat = nullptr;
};

struct veldKaartToestandje {
	WGPUBuffer                      buffer = nullptr;
	size_t                          grootte = 0;
	planeet*                        geo = nullptr;
	bool                            klaar = false;
	int                             factor = 1;
	std::vector<std::pair<std::string, std::string>> kaarten;
};

struct shotToestandje {
	bool klaar = false;
};

struct rijSyncje {
	bool klaar = false;
};

class guiOverlay; //(globale GUI-klasse; gedefinieerd in gui.h)

// ── Klasse ──────────────────────────────────────────────────────────────────

class Simulatie {
public:
	explicit Simulatie(SimulatieConfig cfg);
	~Simulatie();

	/// Initialiseert alles (scherm, shaders, textures, buffers, planeet).
	/// Retourneert true bij succes.
	bool init();

	/// Voert één frame/simulatiestap uit.
	void stap();

	/// Retourneert true als de applicatie moet stoppen.
	bool stopGewenst() const;

	/// Het onderliggende weergave-scherm (voor web: input-eventregistratie).
	weergaveScherm* scherm() const { return _scherm; }

	/// De huidige start-/loop-configuratie (gebruikt door de GUI).
	const SimulatieConfig & config() const { return _cfg; }

	/// Herstart de wereld met een nieuwe configuratie (andere diepte/procedureel/etc.)
	/// zonder het venster of de shaders opnieuw te maken.
	bool herstart(const SimulatieConfig & nieuweCfg);

	/// Alle live-tunables als pointers (voor de GUI: sliders/checkboxes).
	struct Tunables {
		float *zonKracht, *winterZonneKracht, *obliquity, *verwarmtijd;
		float *rotatieOmega, *coriolisOmega, *wrijving, *diffusie;
		float *verdamping, *basisVerzadiging, *hoogteKoel, *neerslagFactor, *orografieFactor;
		float *grondMult, *grondSchaal;
		//Erosie & sediment
		float *zandErosie, *rotsErosie, *bezinkheid, *zandRepose, *hellingKracht, *oplosheid;
		//Water & wolken
		float *evapotranspiratie, *infiltratie, *bodemDiffusie, *veldCapaciteit;
		float *condensTempo, *regenTempo, *wolkVerdamp, *wolkDiffusie;
		//Leven
		float *levenGroeiBand, *levenDroogTempo, *levenVerwelk, *levenKoudTempo;
		float *zandGroei, *zandBuur, *rotsGroei, *rotsBuur;
		bool  *bevroren, *waterStroomt, *tekenWater, *tekenWolken, *zonRoteert, *roteerMaar;
		bool  *schaduwAan, *erosieAan, *levenAan, *atmosfeerAan, *waterStap;
		int   *overlayKeuze;
		size_t* luchtStappen;
	};
	Tunables tunables();

	// Stats voor de GUI
	size_t aantalVakjes() const { return _geo ? _geo->aantalVakjes() : 0; }
	float  hoogsteGrond() const { return _geo ? _geo->hoogsteGrond() : 0.0f; }
	float  fps() const          { return _fps; }
	float  frameTijdMS() const  { return _frameTijdMS; }

private:
	SimulatieConfig _cfg;

	// ── Weergave ──────────────────────────────────────────────────────────
	weergaveSchermPerspectief* _scherm = nullptr;
	WGPUBuffer                 _rekenParBuffer = nullptr;
	WGPUBuffer                 _diagLees       = nullptr;
	WGPUTexture                _offScreen      = nullptr;
	WGPUBuffer                 _shotLees       = nullptr;

	// ── GUI (Dear ImGui) ────────────────────────────────────────────────
	guiOverlay * _gui = nullptr;
	bool _heeftRender = false;

	// ── Planeet ───────────────────────────────────────────────────────────
	planeet* _geo = nullptr;

	// ── Bufferdata ────────────────────────────────────────────────────────
	float _extra[16] = { 0.0f };

	// ── Toestand ──────────────────────────────────────────────────────────
	glm::vec3 _kijkPlek;
	glm::vec3 _zonPos;
	float     _grondMult    = 100.0f;
	float     _grondSchaal  = 1.0f;
	float _verdamping   = 0.0001f;
	float _zonKracht    = 50.0f;
	float _rotatieOmega = 0.009f;
	float _winterZonneKracht = 15.0f;
	float _coriolisOmega = 0.2f;
	float _wrijving     = 0.05f;
	float _diffusie     = 0.65f;
	float _verwarmtijd  = 0.5f;
	float _basisVerzadiging = 0.10f;
	float _hoogteKoel   = 0.4f;
	float _neerslagFactor = 0.3f;
	float _orografieFactor = 0.4f;

	// ── Runtimetunables (WGSL-constanten die nu via de GUI aanpasbaar zijn) ──
	float _zandErosie    = 0.001f;
	float _rotsErosie    = 0.0002f;
	float _bezinkheid    = 0.001f;
	float _zandRepose    = 1.5f;
	float _hellingKracht = 0.5f;
	float _oplosheid     = 0.70f;
	float _evapotranspiratie = 0.001f;
	float _infiltratie   = 0.3f;
	float _bodemDiffusie = 0.5f;
	float _veldCapaciteit = 1.0f;
	float _levenGroeiBand = 6.0f;
	float _levenDroogTempo = 0.0005f;
	float _levenVerwelk  = 0.25f;
	float _levenKoudTempo = 0.5f;
	float _zandGroei     = 1.0005f;
	float _zandBuur      = 0.05f;
	float _rotsGroei     = 1.000025f;
	float _rotsBuur      = 0.003333f;
	float _condensTempo  = 0.01f;
	float _regenTempo    = 0.30f;
	float _wolkVerdamp   = 0.006f;
	float _wolkDiffusie  = 1.0f;
	float     _obliquity    = 0.4f;
	float     _dagHoek      = 0.0f;
	float     _seizoenTeller = 0.0f;
	int       _overlayKeuze = 0;

	bool _roteerMaar      = false;
	bool _waterStroomt    = true;
	bool _waterStap       = false;
	bool _tekenWater      = true;
	bool _tekenWolken     = true;
	bool _zonRoteert      = true;

	bool _csvOpen         = false;
	std::ofstream _csvUit;

	// ── Async-toestanden ──────────────────────────────────────────────────
	rijSyncje      _rijSync;
	diagToestandje _diag;
	csvToestandje  _csv;
	conservatieToestandje _conservatie;
	conservatieStat  _conservatieStat;
	veldKaartToestandje _veldKaart;
	shotToestandje   _shot;

	size_t _frameNummer = 0;
	std::chrono::steady_clock::time_point _loopStart = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point _vorigeFrameTijd = std::chrono::steady_clock::now();
	float _frameTijdMS = 0.0f;
	float _fps         = 0.0f;

	// ── Helpers ───────────────────────────────────────────────────────────
	void wachtOpRij(const WGPUQueue rij, WGPUInstance instantie);
	static bool bewaarPNG(const std::string& bestand, int breedte, int hoogte, const std::vector<unsigned char>& rgba);
	void doeSchaduwPass();
	void doeRenderPassen();
	void slaScreenshot(const std::string& pad);
	void _maakSchaduwKaart();
	bool _laadMola();        // true bij succes
	void _maakPlaneet();
	void _resetStaat();

	// ── MOLA-data (niet-proceduraal) ──────────────────────────────────────
	size_t   _molaBreedte = 0;
	size_t   _molaHoogte  = 0;
	std::vector<float> _molaData; // grijswaarden per pixel
};
