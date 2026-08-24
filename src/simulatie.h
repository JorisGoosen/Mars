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
	float	condenseer[4];  //basisVerzadiging, ongebruikt, neerslagFactor, orografieFactor
	float	fasen[4];       //verwarmtijdconstante, maxGrondHoogte, grondMult, ongebruikt
	float	schaduw[4];     //schaduwAan, schaduwKaartGrootte, stralingKracht, ongebruikt

	// ── Runtimetunables (GUI-sliders; defaults wijken de WGSL-waarden af) ──
	float	erosiePar[4];   //(zandErosie, rotsErosie, bezinkheid, zandRepose)
	float	erosiePar2[4];  //(hellingKracht, oplosheid, ijsRepose, ijsTempo)
	float	waterPar[4];    //(evapotranspiratie, infiltratie, bodemDiffusie, veldCapaciteit)
	float	levenPar[4];    //(levenGroeiBand, levenDroogTempo, levenVerwelk, levenKoudTempo)
	float	groeiPar[4];    //(zandGroei, zandBuur, rotsGroei, rotsBuur) — leven+burengroei
	float	wolkPar[4];     //(condensTempo, regenTempo, wolkVerdamp, wolkDiffusie)
	float	levenPar2[4];   //(levensDamp, waterDoodTempo, ongebruikt, ongebruikt)
};
static_assert(sizeof(rekenParameters) == 96 + 7 * 16, "rekenParameters moet byte-identiek zijn aan WGSL (96 + 7 vec4)");

// ── Configuratie ────────────────────────────────────────────────────────────

struct SimulatieConfig {
	std::string         bronKeuze         = "procedureel";
	std::string         bestand           = "";
	bool                erosieAan         = true;
	bool                levenAan          = true;
	bool                atmosfeerAan      = true;
	uint32_t            zaadje            = 0;   // 0 = willekeurig; >0 = reproduceerbaar (--zaadje)

	// ── Beginwaarden per cel (de "lege Mars" start op 0; temperatuur in Kelvin) ──
	float               startWater        = 0.0f; //waterHoogte per cel
	float               startBodemVocht   = 0.0f;
	float               startWolken       = 0.0f;
	float               startLeven        = 0.0f;
	float               startIjs          = 0.0f;
	float               startDamp         = 0.0f; //luchtVocht
	float               startZandDeksel   = 0.0f; //dikte van de begin-zandlaag
	float               startTemperatuur  = 273.0f; //vlakke begintemperatuur (K; = 0 °C)

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
	int                 startOverlay      = 0;   ///<weergave-overlay bij start (0 = natuurlijk; ook headless te gebruiken)
	bool                bevroren          = false;
	bool                conservatieAan    = false;
	double              conservatieTol    = 0.01;
	bool                diagnoseAan       = false;
	std::vector<std::pair<std::string, std::string>> veldKaarten;          ///<--veldKaart <veld> [bestand]; herhaalbaar
	int                 kaartFactor                 = 1;                   ///<--kaartFactor <n>: resolutie gedeeld door n
	size_t              veldKaartFramesAantal       = 0;                   ///<--veldKaartFrames <n>: laatste n frames als grond-heatmaps
	size_t              veldKaartElkeFramesAantal   = 0;                   ///<--veldKaartElkeFrames <n> <veld>
	std::string         veldKaartElkeFramesVeld     = "grond";
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
	bool gelukt = false;
};

//Toestand van de pick-readback. Web lost mapAsync pas tussen frames af (de
//JS-eventloop moet draaien), dus daar wordt het resultaat één frame uitgesteld
//geconsumeerd; native wacht synchroon in doePickPass en gebruikt dezelfde velden.
struct pickToestandje {
	WGPUBuffer buffer = nullptr;   ///< de readback-buffer (_pickLees)
	bool klaar = false;            ///< map afgerond, wacht op decode (_rondPickAf)
	bool inVoortgang = false;      ///< mapAsync in de lucht (render/copy zijn al gedaan)
	uint32_t id = 0xFFFFFFFFu;     ///< laatst gedecodeerde cel-ID (geenCelId = achtergrond)
};

struct rijSyncje {
	bool klaar = false;
};

class guiOverlay; //(globale GUI-klasse; gedefinieerd in gui.h)
class gereedschap; //(interactief muis-gereedschap; gedefinieerd in gereedschap.h)
class verplaatsGereedschap; //(basis muis/trackpad-gereedschap; gedefinieerd in gereedschap.h)
class penseelGereedschap; //(penseel-gereedschap; gedefinieerd in penseelGereedschap.h)

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

	/// Rendert één beeld naar het off-screen doel en bewaart het als PNG
	/// (headless verificatie; zet ook --schermafbeelding bij --hoofdloos).
	void slaScreenshot(const std::string & pad);

	/// Schrijft alle --veldKaart-resultaten weg (post-loop; headless).
	void schrijfVeldKaarten();

	/// Evalueert --conservering (watermassa-drift) en retourneert true bij LEK.
	bool conservatieLek() const;

	/// Schrijft RGBA-pixels (8 bit/component) naar een PNG (libpng).
	static bool bewaarPNG(const std::string& bestand, int breedte, int hoogte, const std::vector<unsigned char>& rgba);

	/// Alle live-tunables als pointers (voor de GUI: sliders/checkboxes).
	struct Tunables {
		float *zonKracht, *elips, *obliquity, *verwarmtijd, *stralingKracht;
		float *coriolisOmega, *wrijving, *diffusie;
		float *verdamping, *basisVerzadiging, *neerslagFactor, *orografieFactor;
		float *grondMult, *grondSchaal;
		//Erosie & sediment
		float *zandErosie, *rotsErosie, *bezinkheid, *zandRepose, *ijsRepose, *ijsTempo, *hellingKracht, *oplosheid;
		//Water & wolken
		float *evapotranspiratie, *infiltratie, *bodemDiffusie, *veldCapaciteit;
		float *condensTempo, *regenTempo, *wolkVerdamp, *wolkDiffusie;
		//Leven
		float *levenGroeiBand, *levenDroogTempo, *levenVerwelk, *levenKoudTempo;
		float *zandGroei, *zandBuur, *rotsGroei, *rotsBuur;
		float *levensDamp, *waterDoodTempo;
		bool  *bevroren, *waterStroomt, *tekenWater, *tekenIjs, *tekenWolken, *zonRoteert, *roteerMaar;
		bool  *schaduwAan, *erosieAan, *levenAan, *atmosfeerAan, *waterStap;
		int   *overlayKeuze;
		float *wolkAlpha;
		float *waterReflectie;
		float *atmosfeerSterkte;
		float *atmosfeerDikte;
		size_t* luchtStappen;
	};
	Tunables tunables();

	// Stats voor de GUI
	size_t aantalVakjes() const { return _geo ? _geo->aantalVakjes() : 0; }
	float  hoogsteGrond() const { return _geo ? _geo->hoogsteGrond() : 0.0f; }
	float  fps() const          { return _fps; }
	float  frameTijdMS() const  { return _frameTijdMS; }

	// ── Gereedschap / penseel ────────────────────────────────────────────
	penseelGereedschap* penseel() const      { return _penseelGereedschap; }
	bool penseelActief() const               { return _penseelActief; }
	void zetPenseelActief(bool aan)          { _penseelActief = aan; }

private:
	SimulatieConfig _cfg;

	// ── Weergave ──────────────────────────────────────────────────────────
	weergaveSchermPerspectief* _scherm = nullptr;
	WGPUBuffer                 _rekenParBuffer = nullptr;
	WGPUBuffer                 _diagLees       = nullptr;
	size_t                     _diagGrootte    = 0;
	WGPUTexture                _offScreen      = nullptr;
	WGPUBuffer                 _shotLees       = nullptr;

	// ── Penseel (gereedschappen) ─────────────────────────────────────────
	WGPUBuffer _penseelBuffer = nullptr; ///< penseelBuffer (header + gewichten)
	WGPUTexture _pickTextuur   = nullptr; ///< ID-pass doel (RGBA8Unorm)
	uint32_t    _pickBreedte   = 0, _pickHoogte = 0;
	WGPUBuffer  _pickLees      = nullptr; ///< 256 B readback-buffer

	// ── GUI (Dear ImGui) ────────────────────────────────────────────────
	guiOverlay * _gui = nullptr;
	verplaatsGereedschap* _gereedschap = nullptr; ///<actieve muis/trackpad-tool (verplaatsGereedschap)
	penseelGereedschap* _penseelGereedschap = nullptr; ///<penseel-gereedschap (scherm + planeet)
	bool _penseelActief = false; ///<welk gereedschap de muis krijgt (false = verplaatsen)
	bool _heeftRender = false;

	// ── Planeet ───────────────────────────────────────────────────────────
	planeet* _geo = nullptr;

	// ── Bufferdata ────────────────────────────────────────────────────────
	float _extra[16] = { 0.0f };

	// ── Toestand ──────────────────────────────────────────────────────────
	glm::vec3 _kijkPlek;
	glm::vec3 _zonPos = glm::normalize(glm::vec3(0.3f, 0.2f, 1.0f)); //render-zon als kijkrichting (vast in het beeld; alleen B/sleep-rotatie beweegt haar — cameradraai niet); per frame naar modelruimte voor de shaders
	float     _grondMult    = 100.0f;
	float     _grondSchaal  = 1.0f;
	float _verdamping   = 0.0001f;
	float _zonKracht    = 80.0f;
	float _elips        = 0.3f;     //excentriciteit van de baan: seizoensverschil in zonkracht (0 = cirkel, ~1 = sterke ellips)
	float _coriolisOmega = 0.2f;
	float _wrijving     = 0.05f;
	float _diffusie     = 0.65f;
	float _verwarmtijd  = 0.5f;
	float _stralingKracht = 0.05f;
	float _basisVerzadiging = 0.10f;
	float _neerslagFactor = 0.1f;
	float _orografieFactor = 0.4f;

	// ── Runtimetunables (WGSL-constanten die nu via de GUI aanpasbaar zijn) ──
	float _zandErosie    = 0.1f;
	float _rotsErosie    = 0.01f;
	float _bezinkheid    = 0.05f;
	float _zandRepose    = 1.0f;
	float _ijsRepose     = 0.2f;   //ijs-rusthelling: ijs zakt pas bij een hogere hellingsdrempel dan zand
	float _ijsTempo      = 1.0f / 200.0f; //tempo van de ijs-rusthelling: fractie van de drempeloverschrijding die per ronde verschuift (10x trager dan zand)
	float _hellingKracht = 2.0f;
	float _oplosheid     = 0.90f;
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
	float _levensDamp    = 0.004f; //transpiratie: bodemvocht → damp door leven (per ronde)
	float _waterDoodTempo = 4.0f;  //sterkte van leven-sterfte onder water (0 = uit)
	float _condensTempo  = 0.01f;
	float _regenTempo    = 0.30f;
	float _wolkVerdamp   = 0.006f;
	float _wolkDiffusie  = 1.0f;
	float     _obliquity    = 0.4f;
	size_t    _jaarTeller    = 0;    //0..3999: één omloop = één jaar (seizoen = 1000 frames)
	size_t    _zonSlotTeller = 0;    //0..359: rotatiehoek van de sim-zon (dagomloop van zonSchijn.comp)
	int       _overlayKeuze = 0;
	float     _wolkAlpha    = 0.333333f; //doorzichtigheid van het wolkendek (0..1)
	float     _waterReflectie = 1.0f; //sterkte waterspiegel + randreflectie (0..2)
	float     _atmosfeerSterkte = 1.0f; //sterkte van de atmosfeergloed (0 = uit, 1 = normaal)
	float     _atmosfeerDikte   = 0.5f; //dikte van de atmosfeerschil als fractie van de planeetstraal (0.05..1.5)

	bool _roteerMaar      = false;
	bool _waterStroomt    = true;
	bool _waterStap       = false;
	bool _tekenWater      = true;
	bool _tekenIjs        = true;
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
	pickToestandje   _pick;

	double _totaalWaterStart = -1.0, _totaalWaterMin = 0.0, _totaalWaterMax = 0.0;

	size_t _frameNummer = 0;
	std::chrono::steady_clock::time_point _loopStart = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point _vorigeFrameTijd = std::chrono::steady_clock::now();
	float _frameTijdMS = 0.0f;
	float _fps         = 0.0f;

	// ── Helpers ───────────────────────────────────────────────────────────
	void wachtOpRij(const WGPUQueue rij, WGPUInstance instantie);
	void drainVakken(WGPUBufferMapCallback callback, void * gebruiker, bool * klaar);
	void doeSchaduwPass();
	void doeRenderPassen();
	void doeHoogtepuntPass();
	void doeZonSchijnPass(); ///< één frame van het zonlicht-EMA (sim-zon, los van de render-zon)
	uint32_t doePickPass(uint32_t px, uint32_t py);   ///< rendert de ID-pass en leest de cel onder de cursor terug (web: resultaat van de vorige frame)
	void _rondPickAf();       ///< decodeert een afgeronde pick-map en unmap't de buffer
	void _maakSchaduwKaart();
	void _maakPenseelBuffer();
	bool _laadMola();        // true bij succes
	void _maakPlaneet();
	void _resetStaat();

	// ── MOLA-data (niet-proceduraal) ──────────────────────────────────────
	size_t   _molaBreedte = 0;
	size_t   _molaHoogte  = 0;
	std::vector<float> _molaData; // grijswaarden per pixel
};
