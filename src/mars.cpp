#include "simulatie.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif

// ── Externe symbolen voor Emscripten-integratie ──────────────────────────────

static Simulatie *g_sim = nullptr;

extern "C" {
	//Wordt aangeroepen door emscriptenWebBackend.cpp's initWebPlatform() + main-loop
	void _schermStap() { if(g_sim && !g_sim->stopGewenst()) g_sim->stap(); }
	
	//Wordt aangeroepen vanuit JS na runtime-init (optioneel)
	class weergaveScherm;
	void initWebPlatform(weergaveScherm*, const char*, int, int);
}

static void toonHelp()
{
	std::cout <<
"Gebruik: mars [vlaggetjes]\n\n"
"  --zonder-water        start zonder water (waterHoogte = 0)\n"
"  --zonder-erosie       houdt het terrein stil (geen erosie/depositie)\n"
"  --zonder-leven        zet plantengroei uit\n"
"  --zonder-atmosfeer    houdt de lucht volledig stil (geen wind/verdamping/neerslag)\n"
"  --zonder-schaduw      zet de schaduwkaart uit (geen terreinschaduwen, volle zoninstraling)\n"
"  --schaduwGrootte <n>  resolutie van de schaduwkaart (standaard 4096; hoger = scherper, meer geheugen)\n"
"  --procedureel         genereer het terrein met ruis i.p.v. de MOLA-hoogtekaart\n"
"  --mars                laad MARS_Hoogte.png (standaard)\n"
"  --aarde               laad aarde.png i.p.v. MARS_Hoogte.png\n"
"  --maan                laad maan.jpg i.p.v. MARS_Hoogte.png\n"
"  --bestand <naam>      laad een expliciet bestand (bijv. maan.jpg)\n"
"  --zaadje <n>          vast zaadje voor het procedurele terrein (0 = willekeurig)\n"
  "  --water <n>           begin-waterhoogte per cel (standaard 0)\n"
  "  --bodemvocht <n>      begin-bodemvocht per cel (standaard 0)\n"
  "  --wolk <n>            begin-wolken per cel (standaard 0)\n"
  "  --leven <n>           begin-leven per cel (standaard 0)\n"
  "  --ijs <n>             begin-ijsdikte per cel (standaard 0)\n"
  "  --damp <n>            begin-luchtvocht per cel (standaard 0)\n"
  "  --zand <n>            begin-zanddeklaag per cel (standaard 0)\n"
  "  --temperatuur <n>     vlakke begintemperatuur in °C (standaard 0)\n"
  "  --diepte <n>          icosahedron-onderverdelingsniveau (standaard 5)\n"
"  --diagnose            print elke 25 frames de extremen van de reken-stand\n"
"  --diagnoseCsv <bestand>  dump de hele planeet naar een CSV\n"
"  --diagnoseCsvFrames <n>  interval voor het CSV-dumpen (standaard 25)\n"
"  --conservering [tol%]    check of het totale water constant blijft (hoofdloos; tol%=rel. driftsz, standaard 1)\n"
"  --hoofdloos           draai zonder venster (geen display/aqua nodig)\n"
"  --stappen <n>         stop na n rondes (samen met --hoofdloos)\n"
"  --schermafbeelding <bestand>  render een beeld naar een PNG (handig bij --hoofdloos)\n"
 "  --veldKaart <veld> [bestand]  volledige-planeet heatmap als PNG; herhaalbaar\n"
 "  --veldKaartFrames <n>  schrijf de laatste n frames als veldkaart0.png .. veldkaartN-1.png (grond-heatmap)\n"
 "  --veldKaartElkeFrames <n> <veld>  schrijf elke n frames als veldkaart_N.png (standaard veld: grond)\n"
 "  --kaartFactor <n>      veldkaart-resolutie gedeeld door n (standaard 1)\n"
 "  --overlay <n>          weergave-overlay bij start (0-10; ook headless)\n"
 "  --luchtstappen <n>    sim-stappen per beeld (standaard 1)\n"
 "  --stil               bevries alles vanaf het begin (sim, zon- en modelrotatie)\n"
 "  --help, -h            toon deze hulp\n"
"\n"
"Voorbeeld (headless analyse):\n"
"  mars --procedureel --hoofdloos --diepte 4 --stappen 3000 --diagnoseCsv uit.csv\n";
}

int main(int argc, char ** argv)
{
	SimulatieConfig cfg;

	for(int a = 1; a < argc; a++)
	{
		std::string vlag = argv[a];
		if(vlag == "--help" || vlag == "-h")
		{
			toonHelp();
			return 0;
		}
		else if(vlag == "--zonder-water")          cfg.startWater = 0.0f;
		else if(vlag == "--zonder-erosie")   cfg.erosieAan = false;
		else if(vlag == "--zonder-leven")      cfg.levenAan = false;
		else if(vlag == "--zonder-atmosfeer") cfg.atmosfeerAan = false;
		else if(vlag == "--zonder-schaduw")    cfg.schaduwAan = false;
		else if(vlag == "--schaduwGrootte")
		{
			if(a + 1 < argc) cfg.schaduwGrootte = std::clamp(std::atoi(argv[++a]), 256, 8192);
			else std::cerr << "--schaduwGrootte verwacht een getal" << std::endl;
		}
		else if(vlag == "--procedureel")   cfg.bronKeuze = "procedureel";
		else if(vlag == "--mars")          cfg.bronKeuze = "mars";
		else if(vlag == "--aarde")         cfg.bronKeuze = "aarde";
		else if(vlag == "--maan")         cfg.bronKeuze = "maan";
		else if(vlag == "--bestand")
		{
			if(a + 1 < argc) { cfg.bronKeuze = "bestand"; cfg.bestand = argv[++a]; }
			else std::cerr << "--bestand verwacht een bestandsnaam" << std::endl;
		}
		else if(vlag == "--zaadje")
		{
			if(a + 1 < argc) cfg.zaadje = (uint32_t)std::max(0, std::atoi(argv[++a]));
			else std::cerr << "--zaadje verwacht een getal" << std::endl;
		}
		else if(vlag == "--diagnose")          cfg.diagnoseAan = true;
		else if(vlag == "--conservering")
		{
			cfg.conservatieAan = true;
				if(a + 1 < argc)
				{
					double t = std::atof(argv[a + 1]);
					if(t > 0.0) { cfg.conservatieTol = t / 100.0; a++; }
				}
		}
		else if(vlag == "--hoofdloos")     cfg.hoofdloos = true;
		else if(vlag == "--diagnoseCsv")
		{
			if(a + 1 < argc) cfg.csvBestand = argv[++a];
			else std::cerr << "--diagnoseCsv verwacht een bestandsnaam" << std::endl;
		}
		else if(vlag == "--diagnoseCsvFrames")
		{
			if(a + 1 < argc) cfg.csvElkeFrames = (size_t)std::max(1, std::atoi(argv[++a]));
		}
		else if(vlag == "--stappen")
		{
			if(a + 1 < argc) cfg.stappenTotaal = (size_t)std::max(0, std::atoi(argv[++a]));
		}
		else if(vlag == "--schermafbeelding")
		{
			if(a + 1 < argc) cfg.schermafbeeldingBestand = argv[++a];
			else std::cerr << "--schermafbeelding verwacht een bestandsnaam" << std::endl;
		}
		else if(vlag == "--stil")
		{
			cfg.bevroren = true;
		}
		else if(vlag == "--diepte")
		{
			if(a + 1 < argc) cfg.subdiv = std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--diepte verwacht een getal" << std::endl;
		}
		else if(vlag == "--water")
		{
			if(a + 1 < argc) cfg.startWater = (float)std::max(0.0, std::atof(argv[++a]));
			else std::cerr << "--water verwacht een getal" << std::endl;
		}
		else if(vlag == "--bodemvocht")
		{
			if(a + 1 < argc) cfg.startBodemVocht = (float)std::max(0.0, std::atof(argv[++a]));
			else std::cerr << "--bodemvocht verwacht een getal" << std::endl;
		}
		else if(vlag == "--wolk")
		{
			if(a + 1 < argc) cfg.startWolken = (float)std::max(0.0, std::atof(argv[++a]));
			else std::cerr << "--wolk verwacht een getal" << std::endl;
		}
		else if(vlag == "--leven")
		{
			if(a + 1 < argc) cfg.startLeven = (float)std::max(0.0, std::atof(argv[++a]));
			else std::cerr << "--leven verwacht een getal" << std::endl;
		}
		else if(vlag == "--ijs")
		{
			if(a + 1 < argc) cfg.startIjs = (float)std::max(0.0, std::atof(argv[++a]));
			else std::cerr << "--ijs verwacht een getal" << std::endl;
		}
		else if(vlag == "--damp")
		{
			if(a + 1 < argc) cfg.startDamp = (float)std::max(0.0, std::atof(argv[++a]));
			else std::cerr << "--damp verwacht een getal" << std::endl;
		}
		else if(vlag == "--zand")
		{
			if(a + 1 < argc) cfg.startZandDeksel = (float)std::max(0.0, std::atof(argv[++a]));
			else std::cerr << "--zand verwacht een getal" << std::endl;
		}
		else if(vlag == "--temperatuur")
		{
			//Celsius in, Kelvin intern (net als de GUI).
			if(a + 1 < argc) cfg.startTemperatuur = (float)(std::atof(argv[++a]) + 273.15);
			else std::cerr << "--temperatuur verwacht een getal (°C)" << std::endl;
		}
		else if(vlag == "--luchtstappen")
		{
			if(a + 1 < argc) cfg.luchtStappen = std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--luchtstappen verwacht een getal" << std::endl;
		}
		else if(vlag == "--overlay")
		{
			if(a + 1 < argc) cfg.startOverlay = std::clamp(std::atoi(argv[++a]), 0, 10);
			else std::cerr << "--overlay verwacht een getal (0-10)" << std::endl;
		}
		else if(vlag == "--veldKaart")
		{
			if(a + 1 < argc)
			{
				std::string veld = argv[++a];
				std::string bestand = veld + ".png";
				if(a + 1 < argc && argv[a + 1][0] != '-')
					bestand = argv[++a];
				cfg.veldKaarten.emplace_back(veld, bestand);
			}
			else std::cerr << "--veldKaart verwacht een veldnaam (temperatuur, wind, druk, grond, ...)" << std::endl;
		}
		else if(vlag == "--kaartFactor")
		{
			if(a + 1 < argc) cfg.kaartFactor = std::clamp(std::atoi(argv[++a]), 1, 40);
			else std::cerr << "--kaartFactor verwacht een getal" << std::endl;
		}
		else if(vlag == "--veldKaartFrames")
		{
			if(a + 1 < argc) cfg.veldKaartFramesAantal = (size_t)std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--veldKaartFrames verwacht een getal" << std::endl;
		}
		else if(vlag == "--veldKaartElkeFrames")
		{
			if(a + 1 < argc) cfg.veldKaartElkeFramesAantal = (size_t)std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--veldKaartElkeFrames verwacht een getal" << std::endl;
			if(a + 1 < argc) cfg.veldKaartElkeFramesVeld = argv[++a];
		}
		else
		{
			std::cerr << "Onbekende vlag: " << vlag << "\n\n";
			toonHelp();
			return 1;
		}
	}

	if(cfg.hoofdloos && cfg.stappenTotaal == 0 && cfg.csvBestand.empty())
		std::cerr << "Let op: --hoofdloos zonder --stappen of --diagnoseCsv is een no-op.\n";

#ifdef __EMSCRIPTEN__
	//Web draait standaard op procedureel terrein: MARS_Hoogte.png zit bewust
	//niet in de preload (scheelt ~7 MB download).
	cfg.bronKeuze = "procedureel";
#endif

	Simulatie sim(std::move(cfg));
	if(!sim.init())
		return 1;

#ifdef __EMSCRIPTEN__
	//Web-build: RAF-loop + event-registratie via initWebPlatform()
	g_sim = &sim;
	initWebPlatform(sim.scherm(), nullptr, 0, 0);
#else
	while(!sim.stopGewenst())
		sim.stap();

	// Post-loop: eind-screenshot + veldkaarten (headless). Let op: cfg is naar de
	// simulatie gemoved — lees de instellingen dus via sim.config(), niet uit cfg.
	if(sim.config().hoofdloos && !sim.config().schermafbeeldingBestand.empty())
	{
		std::cout << "Eind-screenshot -> " << sim.config().schermafbeeldingBestand << std::endl;
		sim.slaScreenshot(sim.config().schermafbeeldingBestand);
	}

	sim.schrijfVeldKaarten();
#endif

	return sim.conservatieLek() ? 1 : 0;
}
