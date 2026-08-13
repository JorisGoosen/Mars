#include "weergaveSchermPerspectief.h"
#include "planeet.h"
#include "monsterPNG.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>
#include <fstream>
#include <chrono>
#include <vector>
#include <cstring>

//De parameters voor de reken-shaders (bind-groep 0, binding 3). Layout moet matchen
//met het rekenParameters-struct in shaders/planeetStructen.wgsl (80 bytes).
struct rekenParameters
{
	float	grondSchaal,
			verdamping,
			erosie,
			levenAan;
	float	atmosfeer[4];   //(zonkracht, rotatieOmega, wrijving, diffusie)
	float	zonRicht[4];    //zonrichting (dagzijde; de planeet draait t.o.v. de zon)
	float	condenseer[4];  //basisVerzadiging, hoogteKoel, neerslagFactor, orografieFactor
	float	fasen[4];       //verwarmtijdconstante, maxGrondHoogte, grondMult, ongebruikt
};
static_assert(sizeof(rekenParameters) == 80, "rekenParameters moet 80 bytes zijn (gelijk aan WGSL)");

//Diagnose (--diagnose): leest elke zoveel frames de laatste reken-stand terug van de
//GPU en meldt de extremen + de eerste Niet-eindige (NaN/inf) cel.
struct diagToestandje
{
	WGPUBuffer	buffer	= nullptr;
	size_t		grootte	= 0;
	planeet		* geo	= nullptr;
	size_t		teller	= 0;
	bool		klaar	= false;
};

static void diagVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)gebruiker2;
	diagToestandje * t = (diagToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success)
	{
		std::cerr << "[diag] lezen mislukt (status " << (uint32_t)status << ")" << std::endl;
		return;
	}

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);

	float	maxWaterHoogte = 0, maxSchijn = 0, maxBodemVocht = 0, maxLuchtVocht = 0,
			maxDroesem = 0, maxPijp = 0, maxSnelheid = 0, maxGrond = 0,
			maxTemp = 0, maxDruk = 0, maxWind = 0, maxWolken = 0;
	float maxIjs = 0, minTemp = 1.0e30f;
	size_t	piekWater = 0, piekDroesem = 0, piekPijp = 0, piekSnelheid = 0;
	bool	nietEindig = false;

	for(size_t i = 0; i < aantal; i++)
	{
		const vak & cel = cellen[i];
		const float snelhe = sqrtf(cel.snelheid.x * cel.snelheid.x + cel.snelheid.y * cel.snelheid.y);
		const float winds  = sqrtf(cel.wind.x * cel.wind.x + cel.wind.y * cel.wind.y);

		for(int p = 0; p < 6; p++)
		{
			const float absPijp = fabsf(cel.pijpen[p]);
			if(absPijp > maxPijp)	{ maxPijp = absPijp;	piekPijp = i; }
		}

		if		(cel.waterHoogte > maxWaterHoogte)	{ maxWaterHoogte = cel.waterHoogte; piekWater = i; }
		if		(cel.waterSchijn > maxSchijn)		maxSchijn = cel.waterSchijn;
		if		(cel.bodemVocht > maxBodemVocht)	maxBodemVocht = cel.bodemVocht;
		if		(cel.luchtVocht > maxLuchtVocht)	maxLuchtVocht = cel.luchtVocht;
		if		(fabsf(cel.droesem) > maxDroesem){ maxDroesem = fabsf(cel.droesem); piekDroesem = i; }
		if		(snelhe > maxSnelheid)				{ maxSnelheid = snelhe; piekSnelheid = i; }
		if		(cel.grondHoogte > maxGrond)		maxGrond = cel.grondHoogte;
		if		(cel.temperatuur > maxTemp)			maxTemp = cel.temperatuur;
		if		(cel.temperatuur < minTemp)			minTemp = cel.temperatuur;
		if		(cel.ijs > maxIjs)					maxIjs = cel.ijs;
		if		(cel.luchtdruk > maxDruk)			maxDruk = cel.luchtdruk;
		if		(winds > maxWind)					maxWind = winds;
		if		(cel.wolken > maxWolken)			maxWolken = cel.wolken;

		const bool eindig =
				cel.grondHoogte >= -1.0e30f && cel.grondHoogte <=  1.0e30f &&
				cel.rotsHoogte  >= -1.0e30f && cel.rotsHoogte  <=  1.0e30f &&
				cel.waterHoogte >= -1.0e30f && cel.waterHoogte <=  1.0e30f &&
				cel.waterSchijn >= -1.0e30f && cel.waterSchijn <=  1.0e30f &&
				cel.bodemVocht  >= -1.0e30f && cel.bodemVocht  <=  1.0e30f &&
				cel.luchtVocht  >= -1.0e30f && cel.luchtVocht  <=  1.0e30f &&
				cel.droesem     >= -1.0e30f && cel.droesem     <=  1.0e30f &&
				cel.temperatuur >= -1.0e30f && cel.temperatuur <=  1.0e30f &&
				cel.luchtdruk   >= -1.0e30f && cel.luchtdruk   <=  1.0e30f &&
				cel.wolken      >= -1.0e30f && cel.wolken      <=  1.0e30f;

		if(!nietEindig && !eindig)
		{
			glm::vec3 richting = t->geo->punt3(i);
			std::cerr << "[diag " << t->teller << " !!] cel " << i
					  << " richting (" << richting.x << ", " << richting.y << ", " << richting.z << ")"
					  << " NIET-eindig: grond=" << cel.grondHoogte << " rots=" << cel.rotsHoogte
					  << " water=" << cel.waterHoogte << " schijn=" << cel.waterSchijn
					  << " bodem=" << cel.bodemVocht << " lucht=" << cel.luchtVocht
					  << " droesem=" << cel.droesem << " temp=" << cel.temperatuur
					  << " druk=" << cel.luchtdruk << " wolken=" << cel.wolken << std::endl;
			nietEindig = true;
		}
	}

	std::cerr << "[diag " << t->teller << "]  water=" << maxWaterHoogte
			  << " (cel " << piekWater << ")  schijn=" << maxSchijn
			  << "  bodemvocht=" << maxBodemVocht << "  luchtvocht=" << maxLuchtVocht
			  << "  droesem=" << maxDroesem
			  << " (cel " << piekDroesem << ")  pijp=" << maxPijp
			  << " (cel " << piekPijp << ")  snelheid=" << maxSnelheid
			  << " (cel " << piekSnelheid << ")  grond=" << maxGrond
			  << "  temp=" << maxTemp << "K (" << (maxTemp - 273.15f) << "°C)"
			  << "  minTemp=" << minTemp << "K (" << (minTemp - 273.15f) << "°C)"
			  << "  ijs=" << maxIjs << "  druk=" << maxDruk
			  << "  wind=" << maxWind << "  wolken=" << maxWolken << std::endl;

	wgpuBufferUnmap(t->buffer);
}

//Diagnose voor de hele planeet (--diagnoseCsv <bestand>): schrijft álles weg naar een
//CSV (één rij per cel) zodat de berekening extern geanalyseerd kan worden. Bedoeld
//voor kleine grids (laag --diepte), waar --procedureel voor zorgt.
struct csvToestandje
{
	WGPUBuffer	buffer	= nullptr;
	WGPUBuffer	posBuffer = nullptr; //plaatst iedere cel apart: niet nodig, pos via geo
	size_t		grootte	= 0;
	planeet		* geo	= nullptr;
	size_t		teller	= 0;
	bool		klaar	= false;
	std::ofstream * uit = nullptr;
};

static void csvVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)gebruiker2;
	csvToestandje * t = (csvToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success)
	{
		std::cerr << "[diagnoseCsv] lezen mislukt (status " << (uint32_t)status << ")" << std::endl;
		return;
	}

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);

	std::ofstream & uit = *t->uit;
	if(t->teller == 0)
	{
		uit << "id,x,y,z,grond,rots,water,bodemVocht,leven,droesem,luchtVocht,"
		       "temperatuur,luchtdruk,windX,windY,wolken,ijs\n";
	}

	for(size_t i = 0; i < aantal; i++)
	{
		const vak & cel = cellen[i];
		glm::vec3 p = t->geo->punt3(i);
		uit << i << "," << p.x << "," << p.y << "," << p.z << ","
			<< cel.grondHoogte << "," << cel.rotsHoogte << "," << cel.waterHoogte << ","
			<< cel.bodemVocht << "," << cel.leven << "," << cel.droesem << ","
			<< cel.luchtVocht << "," << cel.temperatuur << "," << cel.luchtdruk << ","
			<< cel.wind.x << "," << cel.wind.y << "," << cel.wolken << ","
			<< cel.ijs << "\n";
	}

	wgpuBufferUnmap(t->buffer);
}

//Schrijft RGBA-pixels (8 bit/component) weg naar een PNG-bestand (via libpng).
//Mogelijk gemaakt zodat --hoofdloos --schermafbeelding kan renderen naar een
//off-screen framebuffer en die zonder display kan bewaren/controleren.
static bool bewaarPNG(const std::string & bestand, int breedte, int hoogte, const std::vector<unsigned char> & rgba)
{	png_image beeld;
	memset(&beeld, 0, sizeof(beeld));
	beeld.version = PNG_IMAGE_VERSION;
	beeld.width  = breedte;
	beeld.height = hoogte;
	beeld.format = PNG_FORMAT_RGBA;
	if(png_image_write_to_file(&beeld, bestand.c_str(), 0, rgba.data(), 0, nullptr) == 0)
	{
		std::cerr << "bewaarPNG: " << beeld.message << std::endl;
		return false;
	}
	return true;
}

struct shotToestandje
{
	bool klaar = false;
};

static void shotVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)status; (void)gebruiker2;
	((shotToestandje *)gebruiker1)->klaar = true;
}

//Blokkeert tot alle al verzonden GPU-werk op de rij klaar is. Voorkomt dat de CPU
//verder rent en per-frame uniform/opslag-buffers overschrijft terwijl de GPU die
//nog aan het lezen is (dat gaf sim-afhankelijke, prestatie-gebonden flikkering).
struct rijSyncje { bool klaar = false; };
static void rijSyncVerwerker(WGPUQueueWorkDoneStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)status; (void)gebruiker2;
	((rijSyncje *)gebruiker1)->klaar = true;
}
static void wachtOpRij(const WGPUQueue rij, WGPUInstance instantie)
{
	rijSyncje sync;
	WGPUQueueWorkDoneCallbackInfo info = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
	info.mode 		= WGPUCallbackMode_AllowSpontaneous;
	info.callback 	= rijSyncVerwerker;
	info.userdata1 	= &sync;
	WGPUFuture toekomst = wgpuQueueOnSubmittedWorkDone(rij, info);
	while(!sync.klaar)
		wgpuInstanceProcessEvents(instantie);
	(void)toekomst;
}

static void toonHelp()
{
	std::cout <<
"Gebruik: mars [vlaggetjes]\n\n"
"  --zonder-water        start zonder water (waterHoogte = 0)\n"
"  --zonder-erosie       houdt het terrein stil (geen erosie/depositie)\n"
"  --zonder-leven        zet plantengroei uit\n"
"  --zonder-atmosfeer    houdt de lucht volledig stil (geen wind/verdamping/neerslag)\n"
"  --procedureel         genereer het terrein met ruis i.p.v. de MOLA-hoogtekaart\n"
"  --diepte <n>          icosahedron-onderverdelingsniveau (standaard 5)\n"
"  --diagnose            print elke 25 frames de extremen van de reken-stand\n"
"  --diagnoseCsv <bestand>  dump de hele planeet naar een CSV\n"
"  --diagnoseCsvFrames <n>  interval voor het CSV-dumpen (standaard 25)\n"
"  --hoofdloos           draai zonder venster (geen display/aqua nodig)\n"
"  --stappen <n>         stop na n rondes (samen met --hoofdloos)\n"
"  --schermafbeelding <bestand>  render een beeld naar een PNG (handig bij --hoofdloos)\n"
"  --schermafbeeldingElkeFrames <n>  maak om de n frames een schermafbeelding\n"
"  --luchtstappen <n>    sim-stappen per beeld (standaard 1; hoger = ze zichtbaar sneller zie je wolken bewegen)\n"
"  --stil               bevries alles vanaf het begin (sim, zon- en modelrotatie)\n"
"  --help, -h            toon deze hulp\n"
"\n"
"Voorbeeld (headless analyse):\n"
"  mars --procedureel --hoofdloos --diepte 4 --stappen 3000 --diagnoseCsv uit.csv\n";
}

int main(int argc, char ** argv)
{
	//Testvlaggen: --zonder-water start zonder water, --zonder-erosie houdt het terrein
	//stil, --zonder-leven schakelt plantengroei uit, --zonder-atmosfeer houdt de lucht stil,
	//--procedureel genereert het terrein met ruis i.p.v. de MOLA-hoogtekaart,
	//--diepte <n> zet het icosahedron-onderverdelingsniveau (standaard 5),
	//--diagnose print elke zoveel frames de extremen, --diagnoseCsv <bestand> dumpt de hele
	//planeet naar een CSV, --hoofdloos draait zonder venster, --stappen <n> stopt na n stappen.
	bool beginMetWater = true;
	bool erosieAan = true;
	bool levenAan = true;
	bool atmosfeerAan = true;
	bool diagAan = false;
	bool procedural = false;
	bool hoofdloos = false;
	int  subdiv = 5;
	size_t stappenTotaal = 0; //0 = oneindig (interactief)
	size_t csvElkeFrames = 25;
	std::string csvBestand;
	std::string schermafbeeldingBestand;
	size_t schermElkeFrames = 0; //0 = alleen de eind-screenshot
	int  luchtStappen = 1;      //aantal atmosfeer-simstappen per beeld (wolken zichtbaar laten bewegen)
	bool bevroren = false;      //bevriest sim + zonrotatie + modelrotatie
	bool zonRoteert = true;     //of de bezonning (dag/nacht) vooruitloopt

	for(int a = 1; a < argc; a++)
	{
		std::string vlag = argv[a];
		if(vlag == "--help" || vlag == "-h")
		{
			toonHelp();
			return 0;
		}
		else if(vlag == "--zonder-water")          beginMetWater = false;
		else if(vlag == "--zonder-erosie")   erosieAan = false;
		else if(vlag == "--zonder-leven")      levenAan = false;
		else if(vlag == "--zonder-atmosfeer") atmosfeerAan = false;
		else if(vlag == "--procedureel")   procedural = true;
		else if(vlag == "--diagnose")         diagAan = true;
		else if(vlag == "--hoofdloos")     hoofdloos = true;
		else if(vlag == "--diagnoseCsv")
		{
			if(a + 1 < argc) csvBestand = argv[++a];
			else std::cerr << "--diagnoseCsv verwacht een bestandsnaam (bijv. --diagnoseCsv uit.csv)" << std::endl;
		}
		else if(vlag == "--diagnoseCsvFrames")
		{
			if(a + 1 < argc) csvElkeFrames = (size_t)std::max(1, std::atoi(argv[++a]));
		}
		else if(vlag == "--stappen")
		{
			if(a + 1 < argc) stappenTotaal = (size_t)std::max(0, std::atoi(argv[++a]));
		}
		else if(vlag == "--schermafbeelding")
		{
			if(a + 1 < argc) schermafbeeldingBestand = argv[++a];
			else std::cerr << "--schermafbeelding verwacht een bestandsnaam (bijv. --schermafbeelding beeld.png)" << std::endl;
		}
		else if(vlag == "--schermafbeeldingElkeFrames")
		{
			if(a + 1 < argc) schermElkeFrames = (size_t)std::max(1, std::atoi(argv[++a]));
		}
		else if(vlag == "--stil")
		{
			bevroren = true;  //bevriest vanaf de start (handig voor --hoofdloos bisectie)
		}
		else if(vlag == "--diepte")
		{
			if(a + 1 < argc) subdiv = std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--diepte verwacht een getal (bijv. --diepte 6)" << std::endl;
		}
		else if(vlag == "--luchtstappen")
		{
			if(a + 1 < argc) luchtStappen = std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--luchtstappen verwacht een getal (bijv. --luchtstappen 4)" << std::endl;
		}
		else
		{
			std::cerr << "Onbekende vlag: " << vlag << "\n\n";
			toonHelp();
			return 1;
		}
	}

	//Headless zonder stappen is zinloos; geef een kleine standaard-waarschuwing.
	if(hoofdloos && stappenTotaal == 0 && csvBestand.empty())
		std::cerr << "Let op: --hoofdloos zonder --stappen of --diagnoseCsv is een no-op.\n";

	weergaveSchermPerspectief scherm("Planeet", 1280, 720, 8, hoofdloos);

	//Render-assets zijn nodig in venstermodus óf voor een --schermafbeelding
	//(off-screen framebuffer, ook in --hoofdloos).
	const bool heeftRender = !hoofdloos || !schermafbeeldingBestand.empty();

	if(heeftRender)
	{
		scherm.maakShader(		"planeetgridLand", 	"shaders/planeetgridVertLand.wgsl", 	"shaders/planeetgridFragLand.wgsl"	);
		scherm.maakShader(		"planeetgridWater", "shaders/planeetgridVertWater.wgsl",	"shaders/planeetgridFragWater.wgsl"	);
		scherm.maakShader(		"planeetgridWolk", 	"shaders/planeetgridVertWolk.wgsl", 	"shaders/planeetgridFragWolk.wgsl"	);
	}

	scherm.maakRekenShader(	"waterStroming", 	"shaders/waterStroming.comp"											);
	scherm.maakRekenShader(	"waterDruk", 		"shaders/waterDruk.comp"												);
	scherm.maakRekenShader(	"grondGelijkmaker", "shaders/grondGelijkmaker.comp"										);
	scherm.maakRekenShader(	"waterGemiddelde", 	"shaders/waterGemiddelde.comp"											);
	scherm.maakRekenShader(	"luchtStroming", 	"shaders/luchtStroming.comp"											);
	scherm.maakRekenShader(	"waterLucht", 		"shaders/waterLucht.comp"												);

	scherm.zetWeergaveKleur(0, 0, 0, 1);

	//Hoogtebron voor de planeet: proceduraal (ruis) of uit de MOLA-hoogtekaart.
	std::function<float(glm::vec2)> hoogteMonsteraar;
	png_byte * MarsHoogte = nullptr;
	glm::uvec2 MarsHoogteBH(1, 1);

	if(!procedural)
	{
		size_t w, h, kanalen;
		MarsHoogte = laadPNG("MARS_Hoogte.png", w, h, kanalen);
		if(!MarsHoogte)
			throw std::runtime_error("Kon MARS_Hoogte.png niet laden (of gebruik --procedureel)!");

		//Spiegel de MOLA-hoogtekaart links-rechts (horizontaal), zodat het terrein
		//dat we op de planeet zetten gespiegeld is t.o.v. de ruwe kaart. Het beeld
		//is RGBA (4 kanalen); per rij wisselen we x met (w-1-x).
		for(size_t y = 0; y < h; y++)
		{
			for(size_t x = 0; x < w / 2; x++)
			{
				size_t links = (x + y * w) * 4;
				size_t rechts = ((w - 1 - x) + y * w) * 4;
				std::swap(MarsHoogte[links],     MarsHoogte[rechts]);
				std::swap(MarsHoogte[links + 1], MarsHoogte[rechts + 1]);
				std::swap(MarsHoogte[links + 2], MarsHoogte[rechts + 2]);
				std::swap(MarsHoogte[links + 3], MarsHoogte[rechts + 3]);
			}
		}

		MarsHoogteBH = glm::uvec2(w, h);
	}

	monsterPNG * MOLA = nullptr;
	if(!procedural)
	{
		MOLA = new monsterPNG(MarsHoogte, MarsHoogteBH);

		if(heeftRender)
		{
			scherm.maakTextuur("marsHoogteTex", MarsHoogteBH.x, MarsHoogteBH.y, true, false, false, GL_RGBA8, MarsHoogte, GL_RGBA, GL_UNSIGNED_BYTE);
		}

		delete[] MarsHoogte;
	}

	//voor de water-pass: een bump-kaart voor de golfjes (onafhankelijk van de hoogtebron)
	size_t wb, hb, kanalenB;
	png_byte * bumpData = laadPNG("plaatjes/zeewater_bump.png", wb, hb, kanalenB);
	if(!bumpData)
		throw std::runtime_error("Kon zeewater_bump.png niet laden!");
	if(heeftRender)
	{
		scherm.maakTextuur("waterBumpTex", wb, hb, true, true, false, GL_RGBA8, bumpData, GL_RGBA, GL_UNSIGNED_BYTE);
	}
	delete[] bumpData;

	float		grondMult	= 100.0;
	planeet	*	geo			= nullptr;

	if(procedural)
	{
		//Proceduraal terrein: fractal value-noise over de bol.
		auto hashN = [](glm::vec3 c) -> float
		{
			return glm::fract(glm::sin(c.x * 127.1f + c.y * 311.7f + c.z * 74.7f) * 43758.5453f);
		};
		auto ruis = [&](glm::vec3 p) -> float
		{
			glm::vec3 i = glm::floor(p), f = glm::fract(p);
			glm::vec3 u = f * f * (glm::vec3(3.0f) - 2.0f * f);
			float a0 = hashN(i), a1 = hashN(i + glm::vec3(1,0,0)),
				  b0 = hashN(i + glm::vec3(0,1,0)), b1 = hashN(i + glm::vec3(1,1,0)),
				  c0 = hashN(i + glm::vec3(0,0,1)), c1 = hashN(i + glm::vec3(1,0,1)),
				  d0 = hashN(i + glm::vec3(0,1,1)), d1 = hashN(i + glm::vec3(1,1,1));
			float x00 = glm::mix(a0, a1, u.x), x10 = glm::mix(b0, b1, u.x), z0 = glm::mix(x00, x10, u.y);
			float x01 = glm::mix(c0, c1, u.x), x11 = glm::mix(d0, d1, u.x), z1 = glm::mix(x01, x11, u.y);
			return glm::mix(z0, z1, u.z);
		};
		auto fbm = [&](glm::vec3 p) -> float
		{
			float waarde = 0.0f, amp = 0.5f;
			for(int octaaf = 0; octaaf < 5; octaaf++) { waarde += amp * ruis(p); p *= 2.03f; amp *= 0.5f; }
			return waarde;
		};
		std::function<float(glm::vec3)> proceduraalHoogte = [&](glm::vec3 pos) -> float
		{
			float h = 115.0f + 70.0f * (fbm(pos * 2.2f) * 2.0f - 1.0f) + 18.0f * (fbm(pos * 7.0f) * 2.0f - 1.0f);
			return glm::clamp(h, 10.0f, 200.0f);
		};
		geo = new planeet(subdiv, std::move(proceduraalHoogte), beginMetWater);
	}
	else
	{
		hoogteMonsteraar = [&](glm::vec2 plek) -> float { return grondMult + 10 * (*MOLA)(plek).x; };
		geo = new planeet(subdiv, hoogteMonsteraar, beginMetWater);
		delete MOLA;
	}

	bool 		roteerMaar		= false,
				waterStroomt	= true,
				waterStap		= false,
				tekenWater		= true,
				tekenWolken		= true,
				toonTemperatuur	= false;

	glm::vec3	kijkPlek		(0.0f)				,
				zonPos			(0.0f)				;
	float		grondSchaal		= 1.0,
				verdamping		= 0.002f;
	float						zonKracht		= 70.0f,
				rotatieOmega	= 0.003f,   //dag/nacht langzaam (minder zonne-flikker)
				coriolisOmega	= 0.1f,     //Coriolis-rotatie; losgekoppeld van dag/nacht
				wrijving		= 0.05f,
				diffusie		= 0.02f,
				verwarmtijd		= 0.5f;
	float		basisVerzadiging= 0.25f,
				hoogteKoel		= 0.4f,
				neerslagFactor	= 0.3f,
				orografieFactor	= 0.4f;
	float		obliquity		= 0.4f;
	float		dagHoek			= 0.0f;

	if(!hoofdloos)
	{
		weergaveScherm::toetsVerwerkerFunc toetsenbord = [&](int key, int scancode, int action, int mods)
		{
			(void)scancode; (void)mods;
			if(action == GLFW_PRESS)
				switch(key)
				{
				case GLFW_KEY_SPACE:
					bevroren = !bevroren;
					std::cout << "Je hebt op spatie gedrukt: de simulatie is "
							  << (bevroren ? "bevroren" : "ontdooid") << "." << std::endl;
					break;
				case GLFW_KEY_B:
					zonRoteert = !zonRoteert;
					std::cout << "Je hebt op B gedrukt: de zon " << (zonRoteert ? "loopt nu voort" : "staat nu stil")
							  << ", dus de dag en de nacht " << (zonRoteert ? "wisselen" : "wisselen niet meer") << "." << std::endl;
					break;
				case GLFW_KEY_R:
					roteerMaar = !roteerMaar;
					std::cout << "Je hebt op R gedrukt: de planeet "
							  << (roteerMaar ? "draait nu rond" : "staat nu stil") << "." << std::endl;
					break;
				case GLFW_KEY_X:
					tekenWater = !tekenWater;
					std::cout << "Je hebt op X gedrukt: het water is nu "
							  << (tekenWater ? "zichtbaar" : "onzichtbaar") << "." << std::endl;
					break;
				case GLFW_KEY_C:
					tekenWolken = !tekenWolken;
					std::cout << "Je hebt op C gedrukt: de wolken zijn nu "
							  << (tekenWolken ? "zichtbaar" : "onzichtbaar") << "." << std::endl;
					break;
				case GLFW_KEY_T:
					toonTemperatuur = !toonTemperatuur;
					std::cout << "Je hebt op T gedrukt: de temperatuuroverlay is nu "
							  << (toonTemperatuur ? "aan" : "uit")
							  << " (blauw is koud, groen is 0 °C, rood is warm)." << std::endl;
					break;
				case GLFW_KEY_ENTER:
					waterStap = true;
					std::cout << "Je hebt op Enter gedrukt: er wordt één simulatiestap uitgevoerd." << std::endl;
					break;
				case GLFW_KEY_SEMICOLON:
					grondMult = glm::max(1.0f, grondMult * 0.9f);
					std::cout << "Je hebt op ; gedrukt: de terreinhoogte is nu " << grondMult << "." << std::endl;
					break;
				case GLFW_KEY_APOSTROPHE:
					grondMult = glm::max(1.0f, grondMult * 1.1f);
					std::cout << "Je hebt op ' gedrukt: de terreinhoogte is nu " << grondMult << "." << std::endl;
					break;
				case GLFW_KEY_K:
					verdamping = glm::max(0.0f, verdamping - 0.001f);
					std::cout << "Je hebt op K gedrukt: de verdampingssnelheid is nu " << verdamping << "." << std::endl;
					break;
				case GLFW_KEY_L:
					verdamping = glm::max(0.0f, verdamping + 0.001f);
					std::cout << "Je hebt op L gedrukt: de verdampingssnelheid is nu " << verdamping << "." << std::endl;
					break;
				case GLFW_KEY_LEFT_BRACKET:
					rotatieOmega = glm::max(0.0f, rotatieOmega - 0.002f);
					std::cout << "Je hebt op [ gedrukt: de dag-en-nachtsnelheid is nu " << rotatieOmega << "." << std::endl;
					break;
				case GLFW_KEY_RIGHT_BRACKET:
					rotatieOmega = glm::min(0.2f, rotatieOmega + 0.002f);
					std::cout << "Je hebt op ] gedrukt: de dag-en-nachtsnelheid is nu " << rotatieOmega << "." << std::endl;
					break;
				case GLFW_KEY_G:
					coriolisOmega = glm::max(0.0f, coriolisOmega - 0.02f);
					std::cout << "Je hebt op G gedrukt: de Coriolis-sterkte is nu " << coriolisOmega << "." << std::endl;
					break;
				case GLFW_KEY_H:
					coriolisOmega = glm::min(2.0f, coriolisOmega + 0.02f);
					std::cout << "Je hebt op H gedrukt: de Coriolis-sterkte is nu " << coriolisOmega << "." << std::endl;
					break;
				case GLFW_KEY_U:
					zonKracht = glm::max(0.0f, zonKracht - 5.0f);
					std::cout << "Je hebt op U gedrukt: de zonkracht is nu " << zonKracht << "." << std::endl;
					break;
				case GLFW_KEY_I:
					zonKracht = glm::min(200.0f, zonKracht + 5.0f);
					std::cout << "Je hebt op I gedrukt: de zonkracht is nu " << zonKracht << "." << std::endl;
					break;
				case GLFW_KEY_O:
					wrijving = glm::max(0.0f, wrijving - 0.01f);
					std::cout << "Je hebt op O gedrukt: de wrijving is nu " << wrijving << "." << std::endl;
					break;
				case GLFW_KEY_P:
					wrijving = glm::min(1.0f, wrijving + 0.01f);
					std::cout << "Je hebt op P gedrukt: de wrijving is nu " << wrijving << "." << std::endl;
					break;
				case GLFW_KEY_PERIOD:
					neerslagFactor = glm::max(0.0f, neerslagFactor - 0.1f);
					std::cout << "Je hebt op . gedrukt: de neerslagfactor is nu " << neerslagFactor << "." << std::endl;
					break;
				case GLFW_KEY_SLASH:
					neerslagFactor = glm::max(0.0f, neerslagFactor + 0.1f);
					std::cout << "Je hebt op / gedrukt: de neerslagfactor is nu " << neerslagFactor << "." << std::endl;
					break;
				}
		};
		scherm.setCustomKeyhandler(toetsenbord);
	}

	//Opslag-buffer met de parameters voor de reken-shaders (bind-groep 0, binding 3)
	WGPUDevice apparaat = weergaveScherm::deelApparaat();
	WGPUQueue  rij 		= weergaveScherm::deelRij();

	WGPUBufferDescriptor beschrijving = WGPU_BUFFER_DESCRIPTOR_INIT;
	beschrijving.usage 	= WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
	beschrijving.size 	= sizeof(rekenParameters);
	WGPUBuffer rekenParBuffer = wgpuDeviceCreateBuffer(apparaat, &beschrijving);

	//Diagnose/Csv-buffers: aparte leesbare kopieën om de stand terug te lezen
	const size_t diagGrootte = geo->aantalVakjes() * sizeof(vak);
	WGPUBuffer diagLees = nullptr;
	if(diagAan || !csvBestand.empty())
	{
		WGPUBufferDescriptor leesBeschrijving = WGPU_BUFFER_DESCRIPTOR_INIT;
		leesBeschrijving.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
		leesBeschrijving.size  = diagGrootte;
		diagLees = wgpuDeviceCreateBuffer(apparaat, &leesBeschrijving);
	}

	std::ofstream csvUit;
	if(!csvBestand.empty())
	{
		csvUit.open(csvBestand, std::ios::trunc);
		if(!csvUit.is_open())
			std::cerr << "Kon " << csvBestand << " niet openen voor --diagnoseCsv!" << std::endl;
	}

	//de weergave-parameters (bind-groep 0, binding 2): layout volgens extraParameters in de WGSL
	float extra[16] = { 0.0f };

	auto berekenShaderBinden = [&]()
	{
		geo->bindVrwrkrOpslagen(scherm);
		scherm.verbindRekenBuffer(3, rekenParBuffer);
	};

	//Off-screen framebuffer voor --schermafbeelding (renderen zonder venster)
	const int  shotBreedte = 960, shotHoogte = 960;
	WGPUTexture offScreen = nullptr;
	WGPUBuffer   shotLees = nullptr;
	if(!schermafbeeldingBestand.empty() && hoofdloos)
	{
		WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
		td.usage      = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
		td.dimension  = WGPUTextureDimension_2D;
		td.size       = { (uint32_t)shotBreedte, (uint32_t)shotHoogte, 1u };
		td.format     = WGPUTextureFormat_RGBA8Unorm;
		td.mipLevelCount = 1;
		td.sampleCount   = 1;
		offScreen = wgpuDeviceCreateTexture(apparaat, &td);
		scherm.zetWeergaveDoel(offScreen, glm::uvec2(shotBreedte, shotHoogte));

		WGPUBufferDescriptor rb = WGPU_BUFFER_DESCRIPTOR_INIT;
		rb.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
		rb.size  = (uint64_t)shotBreedte * shotHoogte * 4;
		shotLees = wgpuDeviceCreateBuffer(apparaat, &rb);
	}

	//De drie weergave-passen (grond, water, wolken) plus het klaarzetten van de
	//weergave-parameters. Draait elke frame in venstermodus en één keer aan het
	//einde in --hoofdloos + --schermafbeelding.
	auto doeRenderPassen = [&]()
	{
		kijkPlek = glm::vec3(glm::inverse(scherm.modelZicht())[3]);
		scherm.zetExtraFloats(extra, 16);

		//grond-pass (achterkant-verwijdering aan)
		weergaveInstellingen grondInstellingen;
		grondInstellingen.cullMode = WGPUCullMode_Back;
		scherm.zetWeergaveInstellingen(grondInstellingen);

		scherm.bereidRenderVoor("planeetgridLand");
		geo->bindVrwrkrOpslagen(scherm);
		if(!procedural)
			scherm.bindTextuur("marsHoogteTex", 0);
		geo->tekenJezelf();
		scherm.pasRondRenderAf();

		//water-pass (blendt over de grond, deelt dezelfde diepte-buffer)
		if(tekenWater)
		{
			weergaveInstellingen waterInstellingen;
			waterInstellingen.blenden 			= true;
			waterInstellingen.cullMode 			= WGPUCullMode_Back;
			waterInstellingen.diepteSchrijven 	= false;
			waterInstellingen.diepteVergelijk 	= WGPUCompareFunction_LessEqual;
			scherm.zetWeergaveInstellingen(waterInstellingen);

			scherm.bereidRenderVoor("planeetgridWater", false);
			geo->bindVrwrkrOpslagen(scherm);
			scherm.bindTextuur("waterBumpTex", 0);
			geo->tekenJezelf();
			scherm.pasRondRenderAf();
		}

		//wolk-pass (boven het water; STRENGE diepte-test zodat verste wolken die
		//net achter de planeet staan niet door het maanoppervlak heen schijnen).
		if(tekenWolken)
		{
			weergaveInstellingen wolkInstellingen;
			wolkInstellingen.blenden 			= true;
			wolkInstellingen.cullMode 			= WGPUCullMode_Back;
			wolkInstellingen.diepteSchrijven 	= false;
			wolkInstellingen.diepteVergelijk 	= WGPUCompareFunction_Less;
			scherm.zetWeergaveInstellingen(wolkInstellingen);

			scherm.bereidRenderVoor("planeetgridWolk", false);
			geo->bindVrwrkrOpslagen(scherm);
			geo->tekenJezelf();
			scherm.pasRondRenderAf();
		}

		//één enkele submit voor grond+water+wolken, zodat de diepte-buffer van de
		//grond-pass behouden blijft (hergebruik i.p.v. opnieuw wissen!) en de
		//wolk-pass écht tegen die diepte test.
		scherm.rondRenderAf();
		scherm.zetWeergaveInstellingen(weergaveInstellingen());

		scherm.ontkoppelRekenBuffers();
	};

	//Rendert naar het off-screen framebuffer en bewaart die als PNG.
	auto slaScreenshot = [&](const std::string & pad)
	{
		if(!offScreen || !shotLees)
			return;
		doeRenderPassen();
		const uint64_t shotBytes = (uint64_t)shotBreedte * shotHoogte * 4;

		WGPUTexelCopyTextureInfo bron = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
		bron.texture  = offScreen;
		bron.mipLevel = 0;
		bron.aspect   = WGPUTextureAspect_All;

		WGPUTexelCopyBufferInfo bestemming = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
		bestemming.buffer = shotLees;
		bestemming.layout.offset      = 0;
		bestemming.layout.bytesPerRow = (uint32_t)(shotBreedte * 4);
		bestemming.layout.rowsPerImage= (uint32_t)shotHoogte;

		WGPUExtent3D omvang = { (uint32_t)shotBreedte, (uint32_t)shotHoogte, 1u };

		WGPUCommandEncoder leesEncoder = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
		wgpuCommandEncoderCopyTextureToBuffer(leesEncoder, &bron, &bestemming, &omvang);
		WGPUCommandBuffer leesCommando = wgpuCommandEncoderFinish(leesEncoder, nullptr);
		wgpuQueueSubmit(rij, 1, &leesCommando);
		wgpuCommandBufferRelease(leesCommando);
		wgpuCommandEncoderRelease(leesEncoder);

		shotToestandje toestand;
		WGPUBufferMapCallbackInfo leesInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
		leesInfo.mode 		= WGPUCallbackMode_AllowSpontaneous;
		leesInfo.callback 	= shotVerwerker;
		leesInfo.userdata1 	= &toestand;
		wgpuBufferMapAsync(shotLees, WGPUMapMode_Read, 0, shotBytes, leesInfo);
		while(!toestand.klaar)
			wgpuInstanceProcessEvents(scherm.instantie());

		const unsigned char * pixels = (const unsigned char *)wgpuBufferGetMappedRange(shotLees, 0, shotBytes);
		std::vector<unsigned char> kopie(pixels, pixels + shotBytes);
		wgpuBufferUnmap(shotLees);

		std::cout << "schermafbeelding -> " << pad << std::endl;
		bewaarPNG(pad, shotBreedte, shotHoogte, kopie);
	};

	//parameters voor de reken-shaders
	rekenParameters rekenPar = {};

	const uint32_t rekenGroepen = (uint32_t)((geo->aantalVakjes() + 63) / 64);

	size_t frameNummer = 0;
	auto loopStart = std::chrono::steady_clock::now(); //frametijd-meting

	//In loopconditie: in niet-hoofdloze modus stoppen we op vensterslot; in hoofdloze
	//modus op het aantal stappen (of nooit).
	auto moetStoppen = [&]() -> bool
	{
		if(hoofdloos)
			return stappenTotaal > 0 && frameNummer >= stappenTotaal;
		return scherm.stopGewenst();
	};

	while(!moetStoppen())
	{
		if(!hoofdloos)
		{
			//Is de planeet buiten beeld (venster geOccludeerd)? Stop dan de loop.
			if(!scherm.oppervlakZichtbaar())
			{
				scherm.wachtOpGebeurtenissen();
				continue;
			}
		}

		//---- rotatie-planeet <-> zon ----
		//De zon draait om de geografische noordpool (vast in modelruimte) met een
		//hoeksnelheid = rotatieOmega; de dagzijde is dot(normaal, zonRicht)>0. Dat is
		//equivalent aan een planeet die om haar noord-as draait. Obliquity blijft vast.
		//Bevroren (Space) of zonRoteert uit (B) houdt de bezonning stil.
		if(!bevroren && zonRoteert)
			dagHoek += rotatieOmega;
		dagHoek = glm::mod(dagHoek, 6.28318530718f); //houd de hoek klein (geen precisie-jitter)
		glm::mat4 zonRoteerder =
			glm::rotate(
				glm::rotate(
					glm::mat4(1.0f),
					dagHoek,
					glm::vec3(0.0f, 1.0f, 0.0f)
				),
				obliquity,
				glm::vec3(1.0f, 0.0f, 0.0f)
			);
		glm::vec4 zonRicht4 = zonRoteerder * glm::normalize(glm::vec4(0.0f, 0.2f, 1.0f, 0.0f));
		glm::vec3 zonRichtV = glm::normalize(glm::vec3(zonRicht4));
		zonPos = zonRichtV;

		if(!bevroren && roteerMaar)
			scherm.zetModelZicht(glm::rotate(scherm.modelZicht(), 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)));

		//weergave-parameters in de extra-buffer schrijven
		extra[0] 	= grondMult;
		extra[1] 	= grondSchaal;
		extra[4] 	= kijkPlek.x;	extra[5] = kijkPlek.y;	extra[6] = kijkPlek.z;
		extra[8] 	= zonPos.x;		extra[9] = zonPos.y;	extra[10] = zonPos.z;
		extra[12] 	= geo->hoogsteGrond();
		extra[13] 	= toonTemperatuur ? 1.0f : 0.0f;

		//parameters voor de reken-shaders
		rekenPar.grondSchaal 	= grondSchaal;
		rekenPar.verdamping 	= verdamping;
		rekenPar.erosie 		= erosieAan ? 1.0f : 0.0f;
		rekenPar.levenAan 		= levenAan ? 1.0f : 0.0f;
		rekenPar.atmosfeer[0] 	= atmosfeerAan ? zonKracht : 0.0f;
		rekenPar.atmosfeer[1] 	= coriolisOmega;
		rekenPar.atmosfeer[2] 	= atmosfeerAan ? wrijving : 0.0f;
		rekenPar.atmosfeer[3] 	= atmosfeerAan ? diffusie : 0.0f;
		rekenPar.zonRicht[0] 	= zonPos.x;		rekenPar.zonRicht[1] = zonPos.y;	rekenPar.zonRicht[2] = zonPos.z;	rekenPar.zonRicht[3] = 0.0f;
		rekenPar.condenseer[0] 	= basisVerzadiging;
		rekenPar.condenseer[1] 	= hoogteKoel;
		rekenPar.condenseer[2] 	= neerslagFactor;
		rekenPar.condenseer[3] 	= orografieFactor;
		rekenPar.fasen[0] 		= verwarmtijd;
		rekenPar.fasen[1] 		= geo->hoogsteGrond();
		rekenPar.fasen[2] 		= grondMult;
		rekenPar.fasen[3] 		= 0.0f;

		wgpuQueueWriteBuffer(rij, rekenParBuffer, 0, &rekenPar, sizeof(rekenParameters));

		if(!hoofdloos)
			doeRenderPassen();

		if(!bevroren && (waterStroomt || waterStap || hoofdloos))
		{
			//Multi-stap: loop de rekenketen meerdere keren per beeld. Daarmee wordt
			//de beweging (wind advecteert damp/wolken) op het scherm zichtbaar i.p.v.
			//dat één micro-stap per beeld te traag is om met het oog te volgen.
			for(int substap = 0; substap < luchtStappen; substap++)
			{
				scherm.doeRekenVerwerker("waterStroming", 		glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
				scherm.doeRekenVerwerker("waterDruk", 			glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
				scherm.doeRekenVerwerker("grondGelijkmaker", 	glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
				scherm.doeRekenVerwerker("waterGemiddelde", 	glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
				scherm.doeRekenVerwerker("luchtStroming", 		glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
				scherm.doeRekenVerwerker("waterLucht", 			glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
				geo->volgendeRonde();
			}
			waterStap = false;
		}

		//Periodieke schermafbeeldingen (--schermafbeeldingElkeFrames)
		if(hoofdloos && !schermafbeeldingBestand.empty() && schermElkeFrames > 0 &&
		   frameNummer > 0 && frameNummer % schermElkeFrames == 0)
		{
			std::string pad = schermafbeeldingBestand;
			size_t stip = pad.find_last_of('.');
			if(stip == std::string::npos)
				pad += "_" + std::to_string(frameNummer);
			else
				pad.insert(stip, "_" + std::to_string(frameNummer));
			slaScreenshot(pad);
		}

		if(diagAan && !hoofdloos && diagLees && frameNummer % 25 == 0)
		{
			diagToestandje toestand;
			toestand.buffer 	= diagLees;
			toestand.grootte 	= diagGrootte;
			toestand.geo 		= geo;
			toestand.teller 		= frameNummer;

			WGPUCommandEncoder leesEncoder = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
			wgpuCommandEncoderCopyBufferToBuffer(leesEncoder, geo->huidigeOpslag(), 0, diagLees, 0, diagGrootte);
			WGPUCommandBuffer leesCommando = wgpuCommandEncoderFinish(leesEncoder, nullptr);
			wgpuQueueSubmit(rij, 1, &leesCommando);
			wgpuCommandBufferRelease(leesCommando);
			wgpuCommandEncoderRelease(leesEncoder);

			WGPUBufferMapCallbackInfo leesInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
			leesInfo.mode 		= WGPUCallbackMode_AllowSpontaneous;
			leesInfo.callback 	= diagVerwerker;
			leesInfo.userdata1 	= &toestand;

			wgpuBufferMapAsync(diagLees, WGPUMapMode_Read, 0, diagGrootte, leesInfo);

			while(!toestand.klaar)
				wgpuInstanceProcessEvents(scherm.instantie());
		}

		if(!csvBestand.empty() && csvUit.is_open() && diagLees && frameNummer % csvElkeFrames == 0)
		{
			csvToestandje toestand;
			toestand.buffer 	= diagLees;
			toestand.grootte 	= diagGrootte;
			toestand.geo 		= geo;
			toestand.teller 		= frameNummer;
			toestand.uit 		= &csvUit;

			WGPUCommandEncoder leesEncoder = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
			wgpuCommandEncoderCopyBufferToBuffer(leesEncoder, geo->huidigeOpslag(), 0, diagLees, 0, diagGrootte);
			WGPUCommandBuffer leesCommando = wgpuCommandEncoderFinish(leesEncoder, nullptr);
			wgpuQueueSubmit(rij, 1, &leesCommando);
			wgpuCommandBufferRelease(leesCommando);
			wgpuCommandEncoderRelease(leesEncoder);

			WGPUBufferMapCallbackInfo leesInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
			leesInfo.mode 		= WGPUCallbackMode_AllowSpontaneous;
			leesInfo.callback 	= csvVerwerker;
			leesInfo.userdata1 	= &toestand;

			wgpuBufferMapAsync(diagLees, WGPUMapMode_Read, 0, diagGrootte, leesInfo);

			while(!toestand.klaar)
				wgpuInstanceProcessEvents(scherm.instantie());
		}

		if(hoofdloos && diagAan && frameNummer % 25 == 0)
		{
			//een compacte regel per 25 stappen in hoofdloze modus voor vinger-aan-de-polS
			std::vector<vak> cellen(geo->aantalVakjes());
			//(bewust leeg: teruglezen in hoofdloze modus gaat via --diagnoseCsv)
			(void)cellen;
			std::cerr << "[stap " << frameNummer << "]" << std::endl;
		}

		frameNummer++;

		//De CPU mag pas verder zodra de GPU de vorige frame (render + reken-passen)
		//heeft afgerond; anders overschrijft hij uniform/opslag-buffers die de GPU
		//nog leest -> prestatie-afhankelijke flikkering (vooral op hoge --diepte).
		if(!hoofdloos)
			wachtOpRij(rij, scherm.instantie());

		//Frametijd meten: regelmatig het lopende gemiddelde melden zodat we kunnen
		//zien of de tijd per frame stabiel is (of juist schommelt -> flikker/hikken).
		if(frameNummer % 120 == 0)
		{
			auto nu = std::chrono::steady_clock::now();
			double ms = std::chrono::duration<double, std::milli>(nu - loopStart).count();
			double gemPerFrame = ms / (double)(frameNummer == 0 ? 1 : frameNummer);
			std::cerr << "[tijd] frame " << frameNummer << ": gemiddeld " << gemPerFrame
					  << " ms/frame (" << (1000.0 / (gemPerFrame > 0.0 ? gemPerFrame : 1.0)) << " fps)\n";
			loopStart = nu;
		}

		wgpFoutControle("Frame: ");
	}

	//--schermafbeelding in --hoofdloos: render het eind-frame naar het off-screen
	//framebuffer en bewaar die als PNG (zonder display/aqua nodig).
	if(hoofdloos && !schermafbeeldingBestand.empty() && offScreen && shotLees)
		slaScreenshot(schermafbeeldingBestand);

	if(csvUit.is_open())
		csvUit.close();
	if(diagLees)
		wgpuBufferRelease(diagLees);
	if(shotLees)
		wgpuBufferRelease(shotLees);
	if(offScreen)
		wgpuTextureRelease(offScreen);
	wgpuBufferRelease(rekenParBuffer);

	delete geo;
}
