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
#include <utility>
#include <cstring>
#include <algorithm>

//De parameters voor de reken-shaders (bind-groep 0, binding 3). Layout moet matchen
//met het rekenParameters-struct in shaders/planeetStructen.wgsl (96 bytes).
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
	float	schaduw[4];     //schaduwAan, schaduwKaartGrootte, ongebruikt, ongebruikt
};
static_assert(sizeof(rekenParameters) == 96, "rekenParameters moet 96 bytes zijn (gelijk aan WGSL)");

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
		       "temperatuur,luchtdruk,windX,windY,wolken,ijs,zonZicht,asym\n";
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
			<< cel.ijs << "," << cel.zonZicht << "," << t->geo->buurAsymmetrie(i) << "\n";
	}

	wgpuBufferUnmap(t->buffer);
}

//Waterconservering (--conservering <tol%>): leest de laatste reken-stand terug en
//telt de totale watermassa per reservoir op. Het "waterbewustzijn" is het budget
//waterHoogte + bodemVocht + ijs + luchtVocht + wolken; elk van deze kan in een ander
//reservoir stromen (regen/verdamping/condensatie/bevriezen/infiltratie), maar de som
//moet constant blijven zolang er geen externe waterbron is.
struct conservatieStat
{
	double water = 0, bodem = 0, ijs = 0, damp = 0, wolk = 0;
	size_t teller = 0;
};

//Draagt de buffer-informatie + het doel-struct over naar de map-callback.
struct conservatieToestandje
{
	WGPUBuffer	buffer = nullptr;
	size_t		grootte = 0;
	bool		klaar   = false;
	conservatieStat * stat = nullptr;
};

static void conservatieVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)gebruiker2;
	conservatieToestandje * t = (conservatieToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success)
	{
		std::cerr << "[conservering] lezen mislukt (status " << (uint32_t)status << ")" << std::endl;
		return;
	}

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);
	double w = 0, b = 0, ij = 0, d = 0, k = 0;
	for(size_t i = 0; i < aantal; i++)
	{
		const vak & c = cellen[i];
		w += c.waterHoogte; b += c.bodemVocht; ij += c.ijs; d += c.luchtVocht; k += c.wolken;
	}
	t->stat->water = w; t->stat->bodem = b; t->stat->ijs = ij; t->stat->damp = d; t->stat->wolk = k;
	t->stat->teller++;

	std::cerr << "[conservering " << t->stat->teller << "] water=" << w
			  << " bodem=" << b << " ijs=" << ij << " damp=" << d << " wolk=" << k
			  << "  TOTAAL=" << (w + b + ij + d + k) << std::endl;

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

//Kleurkaarten identiek aan de overlay-shaders (T en V) plus gladStap (smoothstep).
static float gladStap(float e0, float e1, float x)
{
	float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

static glm::vec3 temperatuurKleurC(float TC)
{
	float t = std::clamp((TC - 238.0f) / 70.0f, 0.0f, 1.0f);
	glm::vec3 blauw(0.2f, 0.35f, 1.0f), groen(0.2f, 0.85f, 0.2f), rood(1.0f, 0.25f, 0.1f);
	glm::vec3 kleur = glm::mix(blauw, groen, gladStap(0.0f, 0.5f, t));
	return glm::mix(kleur, rood, gladStap(0.5f, 1.0f, t));
}

static glm::vec3 windKleurC(const vak & c)
{
	float r = std::clamp(c.wind.x / 2.0f * 0.5f + 0.5f, 0.0f, 1.0f);
	float g = std::clamp(c.wind.y / 2.0f * 0.5f + 0.5f, 0.0f, 1.0f);
	float b = std::clamp((c.luchtdruk - 0.2f) / 4.8f, 0.0f, 1.0f);
	return glm::vec3(r, g, b);
}

static glm::vec3 oppervlakteKleurC(const vak & c)
{
	if(c.ijs > 0.01f)
		return glm::vec3(1.0f);
	if(c.waterHoogte > 0.01f)
		return glm::vec3(0.1f, 0.3f, 0.9f) * std::clamp(1.0f - c.waterHoogte * 0.5f, 0.4f, 1.0f);
	float h = std::clamp((c.grondHoogte - 10.0f) / (200.0f - 10.0f), 0.0f, 1.0f);
	return glm::mix(glm::vec3(0.45f, 0.32f, 0.18f), glm::vec3(0.62f, 0.52f, 0.38f), h);
}

static float veldWaardeC(const vak & c, const std::string & veld)
{
	if(veld == "druk" || veld == "luchtdruk") return c.luchtdruk;
	if(veld == "grond" || veld == "hoogte")   return c.grondHoogte;
	if(veld == "water")    return c.waterHoogte;
	if(veld == "ijs")      return c.ijs;
	if(veld == "wolken")   return c.wolken;
	if(veld == "leven")    return c.leven;
	if(veld == "droesem")  return c.droesem;
	if(veld == "zonZicht" || veld == "zonlicht") return c.zonZicht;
	if(veld == "bodemvocht") return c.bodemVocht;
	if(veld == "luchtvocht") return c.luchtVocht;
	return c.temperatuur;
}

//--veldKaart: zet de per-cel stand om in een equirectangulaire volledige-planeet
//heatmap (PNG). De resolutie schaalt mee met de celdichtheid (afgeleid uit het
//aantal cellen, dus onafhankelijk van --diepte). temperatuur en wind gebruiken
//dezelfde kleurkaarten als de T- en V-overlays; overige velden worden grijs.
static void schrijfVeldKaart(const std::string & bestand, const vak * cellen, size_t aantal,
                             planeet * geo, const std::string & veld, int factor)
{
	const double PI = 3.14159265358979323846;

	auto waarde = [&](size_t i) -> float {
		if(veld == "asym") return geo->buurAsymmetrie(i);
		return veldWaardeC(cellen[i], veld);
	};

	const bool kleur = (veld == "temperatuur" || veld == "wind" || veld == "oppervlakte");
	float vmin = 0.0f, vmax = 1.0f;
	if(veld == "wolken")
	{
		vmin = 0.0f; vmax = 0.3f; //vaste schaal: anders stretchen losse piekcellen de autoschaal zwart
	}
	else if(!kleur)
	{
		vmin = 1.0e30f; vmax = -1.0e30f;
		for(size_t i = 0; i < aantal; i++)
		{
			float v = waarde(i);
			if(v < vmin) vmin = v;
			if(v > vmax) vmax = v;
		}
		if(vmax - vmin < 1.0e-6f) { vmin = 0.0f; vmax = 1.0f; }
	}

	double theta = std::sqrt(4.0 * PI / (double)aantal);
	double cellenRond = 2.0 * PI / theta;
	int breedte = (int)(cellenRond * 3.4);
	breedte = std::clamp(breedte, 600, 4800);
	breedte = std::max(96, breedte / std::max(1, factor));
	if(breedte % 2) breedte += 1;
	int hoogte = breedte / 2;

	double pxPerCel = (double)breedte / cellenRond;
	int straal = std::max(2, (int)std::ceil(0.75 * pxPerCel));

	std::vector<unsigned char> rgba((size_t)breedte * hoogte * 4, 0);

	for(size_t i = 0; i < aantal; i++)
	{
		glm::vec3 p = geo->punt3(i);
		float plen = glm::length(p);
		if(plen > 0.0f) p /= plen;

		float lat = std::asin(std::clamp(p.y, -1.0f, 1.0f));
		float lon = std::atan2(p.x, p.z);
		int px = (int)std::floor((double)(lon / (2.0 * PI) + 0.5) * breedte);
		int py = (int)std::floor((double)(0.5 - lat / PI) * hoogte);
		px = ((px % breedte) + breedte) % breedte;
		py = std::clamp(py, 0, hoogte - 1);

		glm::vec3 rgb;
		if(veld == "temperatuur") rgb = temperatuurKleurC(cellen[i].temperatuur);
		else if(veld == "wind")   rgb = windKleurC(cellen[i]);
		else if(veld == "oppervlakte") rgb = oppervlakteKleurC(cellen[i]);
		else
		{
			float t = std::clamp((waarde(i) - vmin) / (vmax - vmin), 0.0f, 1.0f);
			rgb = glm::vec3(t);
		}

		unsigned char rr = (unsigned char)(std::clamp(rgb.r, 0.0f, 1.0f) * 255.0f + 0.5f);
		unsigned char gg = (unsigned char)(std::clamp(rgb.g, 0.0f, 1.0f) * 255.0f + 0.5f);
		unsigned char bb = (unsigned char)(std::clamp(rgb.b, 0.0f, 1.0f) * 255.0f + 0.5f);

		for(int dy = -straal; dy <= straal; dy++)
		{
			int qy = py + dy;
			if(qy < 0 || qy >= hoogte) continue;
			for(int dx = -straal; dx <= straal; dx++)
			{
				if(dx * dx + dy * dy > straal * straal) continue;
				int qx = px + dx;
				if(qx < 0) qx += breedte;
				if(qx >= breedte) qx -= breedte;
				size_t k = ((size_t)qy * breedte + qx) * 4;
				rgba[k + 0] = rr; rgba[k + 1] = gg; rgba[k + 2] = bb; rgba[k + 3] = 255;
			}
		}
	}

	if(bewaarPNG(bestand, breedte, hoogte, rgba))
		std::cout << "veldkaart '" << veld << "' -> " << bestand << " (" << breedte << "x" << hoogte << ")" << std::endl;
}

struct veldKaartToestandje
{
	WGPUBuffer	buffer	= nullptr;
	size_t		grootte	= 0;
	planeet	*	geo		= nullptr;
	bool		klaar	= false;
	int			factor	= 1;
	std::vector<std::pair<std::string, std::string>> kaarten;
};

static void veldKaartVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)gebruiker2;
	veldKaartToestandje * t = (veldKaartToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success)
	{
		std::cerr << "[veldKaart] lezen mislukt (status " << (uint32_t)status << ")" << std::endl;
		return;
	}

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);
	for(const auto & kaart : t->kaarten)
		schrijfVeldKaart(kaart.second, cellen, aantal, t->geo, kaart.first, t->factor);
	wgpuBufferUnmap(t->buffer);
}

static void veldKaartFrameVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)gebruiker2;
	veldKaartToestandje * t = (veldKaartToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success)
	{
		std::cerr << "[veldKaart] lezen mislukt (status " << (uint32_t)status << ")" << std::endl;
		return;
	}

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);
	for(const auto & kaart : t->kaarten)
		schrijfVeldKaart(kaart.second, cellen, aantal, t->geo, kaart.first, t->factor);
	wgpuBufferUnmap(t->buffer);
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
"  --zonder-schaduw      zet de schaduwkaart uit (geen terreinschaduwen, volle zoninstraling)\n"
"  --schaduwGrootte <n>  resolutie van de schaduwkaart (standaard 4096; hoger = scherper, meer geheugen)\n"
"  --dumpSchaduw <bestand>  dump de rauwe schaduwkaart (diepte) als PNG en stop (debug)\n"
"  --procedureel         genereer het terrein met ruis i.p.v. de MOLA-hoogtekaart\n"
"  --diepte <n>          icosahedron-onderverdelingsniveau (standaard 5)\n"
"  --diagnose            print elke 25 frames de extremen van de reken-stand\n"
"  --diagnoseCsv <bestand>  dump de hele planeet naar een CSV\n"
"  --diagnoseCsvFrames <n>  interval voor het CSV-dumpen (standaard 25)\n"
"  --conservering [tol%]    check of het totale water constant blijft (hoofdloos; tol%=rel. driftsz, standaard 1)\n"
"  --hoofdloos           draai zonder venster (geen display/aqua nodig)\n"
"  --stappen <n>         stop na n rondes (samen met --hoofdloos)\n"
"  --schermafbeelding <bestand>  render een beeld naar een PNG (handig bij --hoofdloos)\n"
"  --schermafbeeldingElkeFrames <n>  maak om de n frames een schermafbeelding\n"
"  --veldKaart <veld> [bestand]  volledige-planeet heatmap als PNG (temperatuur, wind, druk, oppervlakte, grond, water, ijs, wolken, ...); herhaalbaar voor meerdere kaarten in één draai\n"
"  --veldKaartFrames <n>         schrijf de laatste n frames als veldkaart0.png .. veldkaartN-1.png (grond-heatmap)\n"
"  --veldKaartElkeFrames <n> <veld>  schrijf elke n frames als veldkaart_N.png (bijv. --veldKaartElkeFrames 10 grond)\n"
"  --kaartFactor <n>     veldkaart-resolutie gedeeld door n (standaard 1)\n"
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
	std::vector<std::pair<std::string, std::string>> veldKaarten; //--veldKaart <veld> [bestand], herhaalbaar
	int kaartFactor = 1; //--kaartFactor <n>: veldkaart-resolutie gedeeld door n
	size_t veldKaartFramesAantal = 0; //--veldKaartFrames <n>: schrijf de laatste n frames als veldkaartN.png
	size_t veldKaartElkeFramesAantal = 0; //--veldKaartElkeFrames <n> <veld>: schrijf elke n frames als veldkaart_N.png
	std::string veldKaartElkeFramesVeld = "grond"; //--veldKaartElkeFrames <n> <veld>
	int  luchtStappen = 1;      //aantal atmosfeer-simstappen per beeld (wolken zichtbaar laten bewegen)
	bool bevroren = false;      //bevriest sim + zonrotatie + modelrotatie
	bool zonRoteert = true;     //of de bezonning (dag/nacht) vooruitloopt
	bool schaduwAan = true;     //schaduwkaart: visuele schaduwen + zonlicht-benadering in de sim
	int schaduwGrootte = 4096;  //resolutie van de schaduwkaart (pixels per zijde; 64 MB bij 4096)
	std::string dumpSchaduwBestand; //--dumpSchaduw <bestand>: dump de rauwe schaduwkaart en stop
		bool conservatieAan = false; //--conservering <tol%>: controleer of het totale water constant blijft
		double conservatieTol = 0.01; //relatieve tolerantie op de totale watermassa (standaard 1%)
		int  conservatieElke = 200;   //om de zoveel frames één meting
		double totaalWaterStart = -1.0, totaalWaterMin = 0.0, totaalWaterMax = 0.0;

	for(int a = 1; a < argc; a++)
	{
		std::string vlag = argv[a];
		if(vlag == "--help" || vlag == "-h")
		{
			toonHelp();
			return 0;
		}
		else if(vlag == "--conservering")
		{
			conservatieAan = true;
			if(a + 1 < argc)
			{
				double t = std::atof(argv[a + 1]);
				if(t > 0.0) { conservatieTol = t / 100.0; a++; }
			}
		}
		else if(vlag == "--zonder-water")          beginMetWater = false;
		else if(vlag == "--zonder-erosie")   erosieAan = false;
		else if(vlag == "--zonder-leven")      levenAan = false;
		else if(vlag == "--zonder-atmosfeer") atmosfeerAan = false;
		else if(vlag == "--zonder-schaduw")    schaduwAan = false;
		else if(vlag == "--schaduwGrootte")
		{
			if(a + 1 < argc) schaduwGrootte = std::clamp(std::atoi(argv[++a]), 256, 8192);
			else std::cerr << "--schaduwGrootte verwacht een getal (bijv. --schaduwGrootte 4096)" << std::endl;
		}
		else if(vlag == "--dumpSchaduw")
		{
			if(a + 1 < argc) dumpSchaduwBestand = argv[++a];
			else std::cerr << "--dumpSchaduw verwacht een bestandsnaam (bijv. --dumpSchaduw kaart.png)" << std::endl;
		}
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
		else if(vlag == "--veldKaart")
		{
			if(a + 1 < argc)
			{
				std::string veld = argv[++a];
				std::string bestand = veld + ".png";
				if(a + 1 < argc && argv[a + 1][0] != '-')
					bestand = argv[++a];
				veldKaarten.emplace_back(veld, bestand);
			}
			else std::cerr << "--veldKaart verwacht een veldnaam (temperatuur, wind, druk, grond, ...)" << std::endl;
		}
		else if(vlag == "--kaartFactor")
		{
			if(a + 1 < argc) kaartFactor = std::clamp(std::atoi(argv[++a]), 1, 40);
			else std::cerr << "--kaartFactor verwacht een getal (bijv. --kaartFactor 10)" << std::endl;
		}
		else if(vlag == "--veldKaartFrames")
		{
			if(a + 1 < argc) veldKaartFramesAantal = (size_t)std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--veldKaartFrames verwacht een getal (bijv. --veldKaartFrames 10)" << std::endl;
		}
		else if(vlag == "--veldKaartElkeFrames")
		{
			if(a + 1 < argc) veldKaartElkeFramesAantal = (size_t)std::max(1, std::atoi(argv[++a]));
			else std::cerr << "--veldKaartElkeFrames verwacht een getal (bijv. --veldKaartElkeFrames 10)" << std::endl;
			if(a + 1 < argc) veldKaartElkeFramesVeld = argv[++a];
		}
		else
		{
			std::cerr << "Onbekende vlag: " << vlag << "\n\n";
			toonHelp();
			return 1;
		}
	}

	//Bestandsnamen voor --veldKaartFrames: reserveer paden voor de laatste n frames
	std::vector<std::pair<size_t, std::string>> veldKaartBestanden;
	if(veldKaartFramesAantal > 0)
	{
		veldKaartBestanden.reserve(veldKaartFramesAantal);
		for(size_t i = 0; i < veldKaartFramesAantal; i++)
			veldKaartBestanden.emplace_back(stappenTotaal - veldKaartFramesAantal + i,
			                               "veldkaart" + std::to_string(i) + ".png");
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
	scherm.maakRekenShader(	"waterGemiddelde", 	"shaders/waterGemiddelde.comp"											);
	scherm.maakRekenShader(	"luchtStroming", 	"shaders/luchtStroming.comp"											);
	scherm.maakRekenShader(	"vochtStroming", 	"shaders/vochtStroming.comp"											);
	scherm.maakRekenShader(	"waterLucht", 		"shaders/waterLucht.comp"												);

	//Schaduwkaart: een orthografische dieptekaart (Depth32Float) bekeken vanuit de
	//zon, elke frame opnieuw gerenderd (de zon draait t.o.v. de planeet en het
	//terrein erodeert). De kaart wordt drie keer gebruikt:
	// 1) per-fragment PCF-schaduwen in de land/water-render,
	// 2) per-cel zonlicht-benadering in luchtStroming.comp (instraling energiebalans),
	// 3) verdamplings-licht in waterLucht.comp (via het zonZicht-veld).
	//Ook in --hoofdloos zonder venster gerenderd: de simulatie heeft de kaart nodig.
	scherm.maakDiepteShader("planeetSchaduw", "shaders/planeetgridVertSchaduw.wgsl");
	scherm.maakTextuur("zonSchaduwKaart", schaduwGrootte, schaduwGrootte, false, false, false, GL_DEPTH_COMPONENT32F, nullptr);
	scherm.bindSchaduwKaart("zonSchaduwKaart");

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

	float 		grondMult	= 100.0;
	int 		overlayKeuze	= 0;
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
			//Deviatie rond de basis in [-1, 1].
			float n = fbm(pos * 2.2f) * 2.0f - 1.0f;
			//Niet-lineaire versterking: middengebieden (kleine |n|) groeien ~2x,
			//pieken/dalen (grote |n|) groeien ~3x mee.
			float versterking = 2.0f + std::abs(n);
			float h = 80.0f + 5.0f * n * versterking;
			return glm::clamp(h, 40.0f, 90.0f);
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
				tekenWolken		= true;

	glm::vec3	kijkPlek		(0.0f)				,
				zonPos			(0.0f)				;
	float		grondSchaal		= 1.0,
				verdamping		= 0.0001f;
	float						zonKracht		= 50.0f,
				rotatieOmega	= 0.009f,   //dag/nacht (3x sneller dan 0.003)
				winterZonneKracht	= 15.0f,
				coriolisOmega	= 0.2f,     //Coriolis-rotatie; losgekoppeld van dag/nacht
				wrijving		= 0.05f,
				diffusie		= 0.65f,
				verwarmtijd		= 0.5f;
	float		basisVerzadiging= 0.10f,
				hoogteKoel		= 0.4f,
				neerslagFactor	= 0.3f,
				orografieFactor	= 0.4f;
	float		obliquity		= 0.4f;
	float		dagHoek			= 0.0f,
				seizoenTeller	= 0.0f;

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
				case GLFW_KEY_N:
					schaduwAan = !schaduwAan;
					std::cout << "Je hebt op N gedrukt: de schaduwkaart is nu "
							  << (schaduwAan ? "aan (terreinschaduwen + gedempte instraling in de schaduw)"
											 : "uit (volle zon overal)") << "." << std::endl;
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
				case GLFW_KEY_ESCAPE:
					if(overlayKeuze != 0)
					{
						//Overlay was open: ga niet afsluiten, maar terug naar de
						//normale weergave. De overschrijf-vlag pakt Escape af van de
						//standaard afsluit-functor.
						weergaveScherm::zetEscapeOverladen();
						overlayKeuze = 0;
						std::cout << "Overlay uit: normale weergave." << std::endl;
					}
					//In de normale weergave valt Escape terug op de standaard
					//afsluit-functor (window sluiten).
					break;
				case GLFW_KEY_1:
				case GLFW_KEY_T:
					overlayKeuze = 0;
					std::cout << "Overlay uit: normale weergave." << std::endl;
					break;
				case GLFW_KEY_2:
					overlayKeuze = 1;
					std::cout << "Temperatuuroverlay aan (blauw = koud, groen = 0 °C, rood = warm)." << std::endl;
					break;
				case GLFW_KEY_3:
				case GLFW_KEY_V:
					overlayKeuze = 2;
					std::cout << "Wind+drukoverlay aan (rood = oost-west, groen = noord-zuid, blauw = luchtdruk)." << std::endl;
					break;
				case GLFW_KEY_4:
					overlayKeuze = 3;
					std::cout << "Bodemvochtoverlay aan (lila = droog, groen = nat)." << std::endl;
					break;
				case GLFW_KEY_5:
					overlayKeuze = 4;
					std::cout << "Luchtvocht/wolken/druk-overlay aan (blauw = luchtvocht, groen = wolken, rood = druk)." << std::endl;
					break;
				case GLFW_KEY_6:
					overlayKeuze = 5;
					std::cout << "IJs/water/bodemvocht-overlay aan (rood = ijs, groen = bodemvocht, blauw = water)." << std::endl;
					break;
				case GLFW_KEY_7:
					overlayKeuze = 6;
					std::cout << "Wolkenoverlay aan." << std::endl;
					break;
				case GLFW_KEY_8:
					overlayKeuze = 7;
					std::cout << "ZonZicht-overlay aan (donker = schaduw, fel = volle zon)." << std::endl;
					break;
				case GLFW_KEY_9:
					overlayKeuze = 8;
					std::cout << "Levens-overlay aan (donker = kaal, fel groen = dicht leven)." << std::endl;
					break;
				case GLFW_KEY_0:
					overlayKeuze = 9;
					std::cout << "Terreinhoogte-overlay aan (donker = laag, fel = hoog)." << std::endl;
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
					if(mods & GLFW_MOD_SHIFT)
					{
						winterZonneKracht = glm::max(0.0f, winterZonneKracht - 5.0f);
						std::cout << "Je hebt op { gedrukt: de winterzonkracht is nu " << winterZonneKracht << "." << std::endl;
					}
					else
					{
						rotatieOmega = glm::max(0.0f, rotatieOmega - 0.002f);
						std::cout << "Je hebt op [ gedrukt: de dag-en-nachtsnelheid is nu " << rotatieOmega << "." << std::endl;
					}
					break;
				case GLFW_KEY_RIGHT_BRACKET:
					if(mods & GLFW_MOD_SHIFT)
					{
						winterZonneKracht = glm::min(zonKracht, winterZonneKracht + 5.0f);
						std::cout << "Je hebt op } gedrukt: de winterzonkracht is nu " << winterZonneKracht << "." << std::endl;
					}
					else
					{
						rotatieOmega = glm::min(0.2f, rotatieOmega + 0.002f);
						std::cout << "Je hebt op ] gedrukt: de dag-en-nachtsnelheid is nu " << rotatieOmega << "." << std::endl;
					}
					break;
				case GLFW_KEY_G:
					coriolisOmega = glm::max(0.0f, coriolisOmega - 0.05f);
					std::cout << "Je hebt op G gedrukt: de Coriolis-sterkte is nu " << coriolisOmega << "." << std::endl;
					break;
				case GLFW_KEY_H:
					coriolisOmega = glm::min(2.0f, coriolisOmega + 0.05f);
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
	if(diagAan || !csvBestand.empty() || conservatieAan || !veldKaarten.empty() || veldKaartFramesAantal > 0 || veldKaartElkeFramesAantal > 0)
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

	//Voor luchtStroming: dezelfde opslag-buffers plus de schaduwkaart als textuur
	//(bind-groep 1 van de reken-pipeline), zodat de instraling de terreinschaduw
	//kan meenemen. Zonder schaduwkaart bindt het framework een 1x1 wit hulpje.
	auto berekenShaderBindenMetSchaduw = [&]()
	{
		berekenShaderBinden();
		scherm.bindTextuur(schaduwAan ? "zonSchaduwKaart" : "", 0);
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

	//Rendert het terrein in de schaduwkaart van de zon (depth-only, orthografisch).
	//Moet vóór zowel de weergave-passes (visuele schaduwen) als de reken-passes
	//(zonlicht-benadering) draaien. De projectie is analytisch (zonProjectie in de
	//shader, uit extra.zonPos alleen), dus er hoeven geen matrices geschreven te
	//worden. Front-face culling: de achterkanten van het terrein gaan de kaart in,
	//zodat het terrein zichzelf niet per ongeluk overschaduwt (acne).
	auto doeSchaduwPass = [&]()
	{
		if(!schaduwAan)
			return;

		weergaveInstellingen schaduwInstellingen;
		//De naar de zon gekeerde vlakken (met deze winding de "front"-vlakken) mogen
		//niet in de kaart: anders vergelijkt elk oppervlaktepunt met zichzelf en geven
		//interpolatieverschillen tussen de twee projecties valse schaduwvlakken. Met
		//cullMode Front blijven alleen de achterkanten over (incl. de van de zon
		//afgewende flanken die als occluder dienen).
		schaduwInstellingen.cullMode = WGPUCullMode_Front;
		scherm.zetWeergaveInstellingen(schaduwInstellingen);

		//Zolang de kaart het dieptedoel is mag hij niet óók als textuur gebonden zijn
		//(WebGPU: DEPTH_STENCIL_WRITE is exclusief), dus bind het witte hulpje.
		scherm.bindSchaduwKaart("");

		scherm.zetDiepteDoel(scherm.textuurId("zonSchaduwKaart"), glm::uvec2(schaduwGrootte, schaduwGrootte));
		scherm.bereidRenderVoor("planeetSchaduw");
		geo->bindVrwrkrOpslagen(scherm);
		geo->tekenJezelf();
		scherm.pasRondRenderAf();
		scherm.rondRenderAf();
		scherm.zetDiepteDoel(nullptr);
		scherm.zetWeergaveInstellingen(weergaveInstellingen());
		scherm.ontkoppelRekenBuffers();

		scherm.bindSchaduwKaart("zonSchaduwKaart");
	};

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
		if(tekenWater && overlayKeuze == 0)
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
		if(tekenWolken && overlayKeuze == 0)
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
		{
			dagHoek += rotatieOmega;
			seizoenTeller += rotatieOmega;
		}
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
		extra[2] 	= (float)schaduwGrootte;
		extra[4] 	= kijkPlek.x;	extra[5] = kijkPlek.y;	extra[6] = kijkPlek.z;
		extra[8] 	= zonPos.x;		extra[9] = zonPos.y;	extra[10] = zonPos.z;
		extra[12] 	= geo->hoogsteGrond();
		extra[13] 	= (float)overlayKeuze;
		extra[14] 	= 0.0f;
		extra[15] 	= schaduwAan ? 1.0f : 0.0f;
		scherm.zetExtraFloats(extra, 16); //ook nodig voor de schaduw-pass (headless rendert geen weergave-passes)

		//parameters voor de reken-shaders
		rekenPar.grondSchaal 	= grondSchaal;
		rekenPar.verdamping 	= verdamping;
		rekenPar.erosie 		= erosieAan ? 1.0f : 0.0f;
		rekenPar.levenAan 		= levenAan ? 1.0f : 0.0f;
		//seizoenscyclus: 4 dagen (8π), zomer→herfst→winter→lente
		float seizoenPos = glm::mod(seizoenTeller, 25.1327412f) / 12.5663706f; //0..1 over 4 dagen
		float winterVerhouding = zonKracht > 0.001f ? winterZonneKracht / zonKracht : 0.0f;
		float seizoenFactor = 1.0f;
		if(seizoenPos < 0.25f)
			seizoenFactor = 1.0f; //zomer
		else if(seizoenPos < 0.5f)
			seizoenFactor = winterVerhouding + (1.0f - winterVerhouding) * (1.0f - (seizoenPos - 0.25f) / 0.25f); //herfst dalend
		else if(seizoenPos < 0.75f)
			seizoenFactor = winterVerhouding; //winter
		else
			seizoenFactor = winterVerhouding + (1.0f - winterVerhouding) * (seizoenPos - 0.75f) / 0.25f; //lente oplopend
		rekenPar.atmosfeer[0] 	= atmosfeerAan ? zonKracht * seizoenFactor : 0.0f;
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
		rekenPar.fasen[3] 		= std::pow(4.0f, (float)(subdiv - 6)); //diffusie-compensatie voor fijnere cellen (1.0 bij diepte 6)
		rekenPar.schaduw[0] 	= schaduwAan ? 1.0f : 0.0f;
		rekenPar.schaduw[1] 	= (float)schaduwGrootte;

		wgpuQueueWriteBuffer(rij, rekenParBuffer, 0, &rekenPar, sizeof(rekenParameters));

		//Eerst de schaduwkaart verversen (de zon draait en het terrein erodeert),
		//dan pas de weergave-passes en de reken-passes die er allebei uit lezen.
		doeSchaduwPass();

		//Debug-hulp: dump de rauwe schaduwkaart (diepte, wit = dicht bij de zon) als
		//PNG en stop meteen — om de kaart zelf te kunnen inspecteren.
		if(!dumpSchaduwBestand.empty() && frameNummer == 0 && schaduwAan)
		{
			const uint32_t N = (uint32_t)schaduwGrootte;
			const uint32_t bytesPerRij = ((N * 4u + 255u) / 256u) * 256u;

			WGPUBufferDescriptor rb = WGPU_BUFFER_DESCRIPTOR_INIT;
			rb.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
			rb.size  = (uint64_t)bytesPerRij * N;
			WGPUBuffer leesBuf = wgpuDeviceCreateBuffer(apparaat, &rb);

			WGPUTexelCopyTextureInfo bron = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
			bron.texture  = scherm.textuurId("zonSchaduwKaart");
			bron.mipLevel = 0;
			bron.aspect   = WGPUTextureAspect_DepthOnly;

			WGPUTexelCopyBufferInfo dest = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
			dest.buffer = leesBuf;
			dest.layout.offset       = 0;
			dest.layout.bytesPerRow  = bytesPerRij;
			dest.layout.rowsPerImage = N;

			WGPUExtent3D omvang = { N, N, 1u };

			WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
			wgpuCommandEncoderCopyTextureToBuffer(enc, &bron, &dest, &omvang);
			WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
			wgpuQueueSubmit(rij, 1, &cmd);
			wgpuCommandBufferRelease(cmd);
			wgpuCommandEncoderRelease(enc);

			shotToestandje toestand;
			WGPUBufferMapCallbackInfo info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
			info.mode 	 = WGPUCallbackMode_AllowSpontaneous;
			info.callback 	= shotVerwerker;
			info.userdata1 = &toestand;
			wgpuBufferMapAsync(leesBuf, WGPUMapMode_Read, 0, rb.size, info);
			while(!toestand.klaar)
				wgpuInstanceProcessEvents(scherm.instantie());

			const float * diepte = (const float *)wgpuBufferGetMappedRange(leesBuf, 0, rb.size);
			std::vector<unsigned char> rgba((size_t)N * N * 4);
			for(uint32_t y = 0; y < N; y++)
				for(uint32_t x = 0; x < N; x++)
				{
					float d = diepte[y * bytesPerRij / 4 + x];
					unsigned char v = (unsigned char)(std::clamp(1.0f - d, 0.0f, 1.0f) * 255.0f + 0.5f);
					size_t k = ((size_t)y * N + x) * 4;
					rgba[k] = rgba[k+1] = rgba[k+2] = v;
					rgba[k+3] = 255;
				}
			wgpuBufferUnmap(leesBuf);
			wgpuBufferRelease(leesBuf);

			bewaarPNG(dumpSchaduwBestand, N, N, rgba);
			std::cout << "schaduwkaart -> " << dumpSchaduwBestand << std::endl;
			return 0;
		}

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
				scherm.doeRekenVerwerker("waterGemiddelde", 	glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
				scherm.doeRekenVerwerker("luchtStroming", 		glm::uvec3(rekenGroepen, 1, 1), berekenShaderBindenMetSchaduw);
				scherm.bindTextuur("", 0); //schaduwkaart weer loslaten voor de volgende passes
				scherm.doeRekenVerwerker("vochtStroming", 		glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
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

		//Periodieke veldkaarten (--veldKaartFrames): schrijf de laatste n frames als grond-heatmaps
		if(hoofdloos && veldKaartFramesAantal > 0 && diagLees &&
		   frameNummer > 0 && frameNummer < stappenTotaal &&
		   frameNummer >= stappenTotaal - veldKaartFramesAantal)
		{
			size_t idx = frameNummer - (stappenTotaal - veldKaartFramesAantal);
			veldKaartToestandje toestand;
			toestand.buffer 	= diagLees;
			toestand.grootte 	= diagGrootte;
			toestand.geo 		= geo;
			toestand.factor 	= kaartFactor;
			toestand.kaarten 	= {{"grond", veldKaartBestanden[idx].second}};

			WGPUCommandEncoder leesEncoder = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
			wgpuCommandEncoderCopyBufferToBuffer(leesEncoder, geo->huidigeOpslag(), 0, diagLees, 0, diagGrootte);
			WGPUCommandBuffer leesCommando = wgpuCommandEncoderFinish(leesEncoder, nullptr);
			wgpuQueueSubmit(rij, 1, &leesCommando);
			wgpuCommandBufferRelease(leesCommando);
			wgpuCommandEncoderRelease(leesEncoder);

			WGPUBufferMapCallbackInfo leesInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
			leesInfo.mode      = WGPUCallbackMode_AllowSpontaneous;
			leesInfo.callback  = veldKaartFrameVerwerker;
			leesInfo.userdata1 = &toestand;
			wgpuBufferMapAsync(diagLees, WGPUMapMode_Read, 0, diagGrootte, leesInfo);

			while(!toestand.klaar)
				wgpuInstanceProcessEvents(scherm.instantie());
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

		//Waterconservering (--conservering): lees regelmatig de totale watermassa terug
		if(conservatieAan && diagLees && frameNummer > 0 && frameNummer % conservatieElke == 0)
		{
			conservatieStat stat;
			conservatieToestandje toestand;
			toestand.buffer  = diagLees;
			toestand.grootte = diagGrootte;
			toestand.stat	 = &stat;

			WGPUCommandEncoder leesEncoder = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
			wgpuCommandEncoderCopyBufferToBuffer(leesEncoder, geo->huidigeOpslag(), 0, diagLees, 0, diagGrootte);
			WGPUCommandBuffer leesCommando = wgpuCommandEncoderFinish(leesEncoder, nullptr);
			wgpuQueueSubmit(rij, 1, &leesCommando);
			wgpuCommandBufferRelease(leesCommando);
			wgpuCommandEncoderRelease(leesEncoder);

			WGPUBufferMapCallbackInfo cb = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
			cb.mode 	 = WGPUCallbackMode_AllowSpontaneous;
			cb.callback  = conservatieVerwerker;
			cb.userdata1 = &toestand;
			wgpuBufferMapAsync(diagLees, WGPUMapMode_Read, 0, diagGrootte, cb);

			while(!toestand.klaar)
				wgpuInstanceProcessEvents(scherm.instantie());

			double totaal = stat.water + stat.bodem + stat.ijs + stat.damp + stat.wolk;
			if(totaalWaterStart < 0.0)
			{
				totaalWaterStart = totaal;
				totaalWaterMin = totaal;
				totaalWaterMax = totaal;
			}
			else
			{
				totaalWaterMin = std::min(totaalWaterMin, totaal);
				totaalWaterMax = std::max(totaalWaterMax, totaal);

				if(hoofdloos && stappenTotaal > 0 && frameNummer >= stappenTotaal)
				{
					double afwijking = (totaalWaterStart > 0.0)
						? std::max(std::abs((totaalWaterMax - totaalWaterStart) / totaalWaterStart),
								   std::abs((totaalWaterMin - totaalWaterStart) / totaalWaterStart))
						: 0.0;
					std::cerr << "[conservering] EVALUATIE: start=" << totaalWaterStart
							  << " min=" << totaalWaterMin << " max=" << totaalWaterMax
							  << "  max-afwijking=" << (100.0 * afwijking) << "%  (tolerantie "
							  << (100.0 * conservatieTol) << "%)  => "
							  << (afwijking <= conservatieTol ? "GEVANGEN: constant" : "LEK: niet constant")
							  << std::endl;
				}
			}
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

	//--veldKaart: lees de laatste rekenstand terug en schrijf een volledige-planeet
	//heatmap (temperatuur, wind, druk, ...) als equirectangulaire PNG.
	if(!veldKaarten.empty() && diagLees)
	{
		veldKaartToestandje toestand;
		toestand.buffer  = diagLees;
		toestand.grootte = diagGrootte;
		toestand.geo     = geo;
		toestand.kaarten = veldKaarten;
		toestand.factor  = kaartFactor;

		WGPUCommandEncoder leesEncoder = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
		wgpuCommandEncoderCopyBufferToBuffer(leesEncoder, geo->huidigeOpslag(), 0, diagLees, 0, diagGrootte);
		WGPUCommandBuffer leesCommando = wgpuCommandEncoderFinish(leesEncoder, nullptr);
		wgpuQueueSubmit(rij, 1, &leesCommando);
		wgpuCommandBufferRelease(leesCommando);
		wgpuCommandEncoderRelease(leesEncoder);

		WGPUBufferMapCallbackInfo leesInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
		leesInfo.mode      = WGPUCallbackMode_AllowSpontaneous;
		leesInfo.callback  = veldKaartVerwerker;
		leesInfo.userdata1 = &toestand;
		wgpuBufferMapAsync(diagLees, WGPUMapMode_Read, 0, diagGrootte, leesInfo);

			while(!toestand.klaar)
				wgpuInstanceProcessEvents(scherm.instantie());
		}

		//Periodieke veldkaarten (--veldKaartElkeFrames): schrijf elke N frames als grond-heatmap
		if(hoofdloos && veldKaartElkeFramesAantal > 0 && diagLees &&
		   frameNummer > 0 && frameNummer % veldKaartElkeFramesAantal == 0)
		{
			size_t idx = frameNummer / veldKaartElkeFramesAantal;
			veldKaartToestandje toestand;
			toestand.buffer 	= diagLees;
			toestand.grootte 	= diagGrootte;
			toestand.geo 		= geo;
			toestand.factor 	= kaartFactor;
			toestand.kaarten 	= {{veldKaartElkeFramesVeld, "veldkaart_" + std::to_string(idx) + ".png"}};

			WGPUCommandEncoder leesEncoder = wgpuDeviceCreateCommandEncoder(apparaat, nullptr);
			wgpuCommandEncoderCopyBufferToBuffer(leesEncoder, geo->huidigeOpslag(), 0, diagLees, 0, diagGrootte);
			WGPUCommandBuffer leesCommando = wgpuCommandEncoderFinish(leesEncoder, nullptr);
			wgpuQueueSubmit(rij, 1, &leesCommando);
			wgpuCommandBufferRelease(leesCommando);
			wgpuCommandEncoderRelease(leesEncoder);

			WGPUBufferMapCallbackInfo leesInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
			leesInfo.mode      = WGPUCallbackMode_AllowSpontaneous;
			leesInfo.callback  = veldKaartFrameVerwerker;
			leesInfo.userdata1 = &toestand;
			wgpuBufferMapAsync(diagLees, WGPUMapMode_Read, 0, diagGrootte, leesInfo);

			while(!toestand.klaar)
				wgpuInstanceProcessEvents(scherm.instantie());
		}

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

	//Bevestiging --veldKaartFrames
	if(veldKaartFramesAantal > 0)
		std::cout << "[veldKaartFrames] " << veldKaartFramesAantal << " kaarten geschreven" << std::endl;

	//Terugkeer-waarde voor --conservering: 0 = water blijft constant, 1 = LEK.
	if(conservatieAan && totaalWaterStart > 0.0)
	{
		double afwijking = std::max(std::abs((totaalWaterMax - totaalWaterStart) / totaalWaterStart),
									std::abs((totaalWaterMin - totaalWaterStart) / totaalWaterStart));
		if(afwijking > conservatieTol)
		{
			std::cerr << "[conservering] LEK gedetecteerd: afwijking " << (100.0 * afwijking)
					  << "% > tolerantie " << (100.0 * conservatieTol) << "%" << std::endl;
			return 1;
		}
	}
	return 0;
}
