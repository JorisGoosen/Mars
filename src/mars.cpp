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
"  --diepte <n>          icosahedron-onderverdelingsniveau (standaard 5)\n"
"  --diagnose            print elke 25 frames de extremen van de reken-stand\n"
"  --diagnoseCsv <bestand>  dump de hele planeet naar een CSV\n"
"  --diagnoseCsvFrames <n>  interval voor het CSV-dumpen (standaard 25)\n"
"  --conservering [tol%]    check of het totale water constant blijft (hoofdloos; tol%=rel. driftsz, standaard 1)\n"
"  --hoofdloos           draai zonder venster (geen display/aqua nodig)\n"
"  --stappen <n>         stop na n rondes (samen met --hoofdloos)\n"
"  --schermafbeelding <bestand>  render een beeld naar een PNG (handig bij --hoofdloos)\n"
"  --veldKaart <veld> [bestand]  volledige-planeet heatmap als PNG; herhaalbaar\n"
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
		else if(vlag == "--zonder-water")          cfg.beginMetWater = false;
		else if(vlag == "--zonder-erosie")   cfg.erosieAan = false;
		else if(vlag == "--zonder-leven")      cfg.levenAan = false;
		else if(vlag == "--zonder-atmosfeer") cfg.atmosfeerAan = false;
		else if(vlag == "--zonder-schaduw")    cfg.schaduwAan = false;
		else if(vlag == "--schaduwGrootte")
		{
			if(a + 1 < argc) cfg.schaduwGrootte = std::clamp(std::atoi(argv[++a]), 256, 8192);
			else std::cerr << "--schaduwGrootte verwacht een getal" << std::endl;
		}
		else if(vlag == "--procedureel")   cfg.procedural = true;
		else if(vlag == "--diagnose")          /* native-only */;
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
		else if(vlag == "--luchtstappen")
		{
			if(a + 1 < argc) cfg.luchtStappen = std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--luchtstappen verwacht een getal" << std::endl;
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

	Simulatie sim(std::move(cfg));
	if(!sim.init())
		return 1;

#ifdef __EMSCRIPTEN__
	//Web-build: gebruik requestAnimationFrame-loop i.p.v. while-loop
	//Global pointer omdat emscripten_set_main_loop geen userdata-parameter heeft
	static Simulatie *g_sim = nullptr;
	g_sim = &sim;
	emscripten_set_main_loop([]() {
		if(g_sim && !g_sim->stopGewenst())
			g_sim->stap();
	}, 0, 1); //fps=0 = onbeperkt (RAF-snelheid), simulateInfiniteLoop=1
#else
	while(!sim.stopGewenst())
		sim.stap();

	// Post-loop: eind-screenshot (headless)
	if(cfg.hoofdloos && !cfg.schermafbeeldingBestand.empty())
	{
		std::cout << "Eind-screenshot -> " << cfg.schermafbeeldingBestand << std::endl;
	}
#endif

	return 0;
}
