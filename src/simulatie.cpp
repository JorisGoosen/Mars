#include "simulatie.h"
#include "gui.h"
#include "gereedschap.h"
#include "penseelGereedschap.h"
#include "planeetAanwijzer.h"
#include "helpers.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <png.h>
#include <stb_image.h>
#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
//Lazy-laden van MARS_Hoogte.png in de browser: fetch van de server en schrijf
//naar het virtuele bestandssysteem, zodat laadPNG hem gewoon kan lezen. Blokkeert
//(asyncify) tot de bytes binnen zijn; daarna is alles verder zoals native.
EM_ASYNC_JS(int, marsLaadHoogteUitBrowser, (), {
	if (FS.analyzePath('/MARS_Hoogte.png').exists) return 0;
	const resp = await fetch('MARS_Hoogte.png');
	if (!resp.ok) return 1;
	const buf  = new Uint8Array(await resp.arrayBuffer());
	FS.writeFile('/MARS_Hoogte.png', buf);
	return 0;
});
#endif

struct stbiDeleter
{
	void operator()(unsigned char *p) const { if(p) stbi_image_free(p); }
};

// ── Callbacks (namespace scope) ─────────────────────────────────────────────

static void diagVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void *)
{
	diagToestandje * t = (diagToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success) { std::cerr << "[diag] lezen mislukt\n"; wgpuBufferUnmap(t->buffer); return; }

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);

	float maxWaterHoogte = 0, maxSchijn = 0, maxBodemVocht = 0, maxLuchtVocht = 0,
		  maxDroesem = 0, maxPijp = 0, maxSnelheid = 0, maxGrond = 0, maxZand = 0,
		  maxTemp = 0, minTemp = 1.0e30f, maxDruk = 0, maxWind = 0, maxWolken = 0, maxIjs = 0;
	size_t piekWater = 0, piekDroesem = 0, piekPijp = 0, piekSnelheid = 0;
	bool nietEindig = false;

	for(size_t i = 0; i < aantal; i++)
	{
		const vak & cel = cellen[i];
		const float snelhe = sqrtf(cel.snelheid.x * cel.snelheid.x + cel.snelheid.y * cel.snelheid.y);
		const float winds  = sqrtf(cel.wind.x * cel.wind.x + cel.wind.y * cel.wind.y);
		const float grond  = cel.rotsHoogte + cel.zandHoogte;

		for(int p = 0; p < 6; p++)
		{
			const float absPijp = fabsf(cel.pijpen[p]);
			if(absPijp > maxPijp) { maxPijp = absPijp;	piekPijp = i; }
		}
		if		(cel.waterHoogte > maxWaterHoogte)	{ maxWaterHoogte = cel.waterHoogte; piekWater = i; }
		if		(cel.waterSchijn > maxSchijn)		maxSchijn = cel.waterSchijn;
		if		(cel.bodemVocht > maxBodemVocht)	maxBodemVocht = cel.bodemVocht;
		if		(cel.luchtVocht > maxLuchtVocht)	maxLuchtVocht = cel.luchtVocht;
		if		(fabsf(cel.droesem) > maxDroesem){ maxDroesem = fabsf(cel.droesem); piekDroesem = i; }
		if		(snelhe > maxSnelheid)				{ maxSnelheid = snelhe; piekSnelheid = i; }
		if		(grond > maxGrond)					maxGrond = grond;
		if		(cel.zandHoogte > maxZand)			maxZand = cel.zandHoogte;
		if		(cel.temperatuur > maxTemp)			maxTemp = cel.temperatuur;
		if		(cel.temperatuur < minTemp)			minTemp = cel.temperatuur;
		if		(cel.ijs > maxIjs)					maxIjs = cel.ijs;
		if		(cel.luchtdruk > maxDruk)			maxDruk = cel.luchtdruk;
		if		(winds > maxWind)					maxWind = winds;
		if		(cel.wolken > maxWolken)			maxWolken = cel.wolken;

		const bool eindig =
				cel.zandHoogte  >= -1.0e30f && cel.zandHoogte  <=  1.0e30f &&
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
					  << " NIET-eindig: grond=" << grond << " rots=" << cel.rotsHoogte
					  << " water=" << cel.waterHoogte << " temp=" << cel.temperatuur << std::endl;
			nietEindig = true;
		}
	}

	std::cerr << "[diag " << t->teller << "] water=" << maxWaterHoogte
			  << " (cel " << piekWater << ") schijn=" << maxSchijn
			  << " bodem=" << maxBodemVocht << " lucht=" << maxLuchtVocht
			  << " droesem=" << maxDroesem << " snelheid=" << maxSnelheid
			  << " grond=" << maxGrond << " zand=" << maxZand
			  << " temp=" << maxTemp << "K (" << (maxTemp - 273.15f) << "°C)"
			  << " min=" << minTemp << "K ijs=" << maxIjs
			  << " druk=" << maxDruk << " wind=" << maxWind << " wolken=" << maxWolken << std::endl;

	wgpuBufferUnmap(t->buffer);
}

static void csvVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void *)
{
	csvToestandje * t = (csvToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success) { std::cerr << "[csv] lezen mislukt\n"; wgpuBufferUnmap(t->buffer); return; }

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);
	std::ofstream & uit = *t->uit;
	if(t->teller == 0)
		uit << "id,x,y,z,grond,rots,zand,water,bodemVocht,leven,droesem,luchtVocht,"
		       "temperatuur,luchtdruk,windX,windY,wolken,ijs,zonlicht,asym\n";

	for(size_t i = 0; i < aantal; i++)
	{
		const vak & cel = cellen[i];
		glm::vec3 p = t->geo->punt3(i);
		uit << i << "," << p.x << "," << p.y << "," << p.z << ","
			<< (cel.rotsHoogte + cel.zandHoogte) << "," << cel.rotsHoogte << "," << cel.zandHoogte << "," << cel.waterHoogte << ","
			<< cel.bodemVocht << "," << cel.leven << "," << cel.droesem << ","
			<< cel.luchtVocht << "," << cel.temperatuur << "," << cel.luchtdruk << ","
			<< cel.wind.x << "," << cel.wind.y << "," << cel.wolken << ","
			<< cel.ijs << "," << cel.zonlicht << "," << t->geo->buurAsymmetrie(i) << "\n";
	}

	wgpuBufferUnmap(t->buffer);
}

static void conservatieVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void *)
{
	conservatieToestandje * t = (conservatieToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success) { std::cerr << "[conservering] lezen mislukt\n"; wgpuBufferUnmap(t->buffer); return; }

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
			  << " TOTAAL=" << (w + b + ij + d + k) << std::endl;

	wgpuBufferUnmap(t->buffer);
}

static void shotVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)gebruiker2;
	shotToestandje * t = (shotToestandje *)gebruiker1;
	t->klaar  = true;
	t->gelukt = (status == WGPUMapAsyncStatus_Success);
}

static void pickVerwerker(WGPUMapAsyncStatus status, WGPUStringView boodschap, void * gebruiker1, void *)
{
	pickToestandje * pick = (pickToestandje *)gebruiker1;
	pick->inVoortgang = false;
	pick->klaar = (status == WGPUMapAsyncStatus_Success);

	if(!pick->klaar)
	{
		pick->id = geenCelId;
		std::string tekst = boodschap.data ? std::string(boodschap.data, boodschap.length) : "";
		std::cerr << "[pick] mapAsync mislukt (status " << (int)status << "): " << tekst << std::endl;
	}
}

// ── Veldkaarten (--veldKaart e.a.): kleurhelpers + equirectangulaire heatmaps ─

//Kleurkaarten identiek aan de overlay-shaders plus gladStap (smoothstep).
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
	float h = std::clamp((c.rotsHoogte + c.zandHoogte - 10.0f) / (200.0f - 10.0f), 0.0f, 1.0f);
	return glm::mix(glm::vec3(0.45f, 0.32f, 0.18f), glm::vec3(0.62f, 0.52f, 0.38f), h);
}

static float veldWaardeC(const vak & c, const std::string & veld)
{
	if(veld == "druk" || veld == "luchtdruk") return c.luchtdruk;
	if(veld == "grond" || veld == "hoogte")   return c.rotsHoogte + c.zandHoogte;
	if(veld == "rots" || veld == "rotsHoogte") return c.rotsHoogte;
	if(veld == "zand" || veld == "zandHoogte") return c.zandHoogte;
	if(veld == "water")    return c.waterHoogte;
	if(veld == "ijs")      return c.ijs;
	if(veld == "wolken")   return c.wolken;
	if(veld == "leven")    return c.leven;
	if(veld == "droesem")  return c.droesem;
	if(veld == "zonZicht" || veld == "zonlicht") return c.zonlicht;
	if(veld == "bodemvocht") return c.bodemVocht;
	if(veld == "luchtvocht") return c.luchtVocht;
	return c.temperatuur;
}

//Zet de per-cel stand om in een equirectangulaire volledige-planeet heatmap (PNG).
//De resolutie schaalt mee met de celdichtheid (afgeleid uit het aantal cellen).
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

	if(Simulatie::bewaarPNG(bestand, breedte, hoogte, rgba))
		std::cout << "veldkaart '" << veld << "' -> " << bestand << " (" << breedte << "x" << hoogte << ")" << std::endl;
}

static void veldKaartVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void * gebruiker2)
{
	(void)gebruiker2;
	veldKaartToestandje * t = (veldKaartToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success)
	{
		std::cerr << "[veldKaart] lezen mislukt (status " << (uint32_t)status << ")" << std::endl;
		wgpuBufferUnmap(t->buffer);
		return;
	}

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);
	for(const auto & kaart : t->kaarten)
		schrijfVeldKaart(kaart.second, cellen, aantal, t->geo, kaart.first, t->factor);
	wgpuBufferUnmap(t->buffer);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void Simulatie::wachtOpRij(const WGPUQueue rij, WGPUInstance instantie)
{
	_rijSync.klaar = false;
	WGPUQueueWorkDoneCallbackInfo info = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
	info.mode 		= WGPUCallbackMode_AllowSpontaneous;
	info.callback 	= [](WGPUQueueWorkDoneStatus, WGPUStringView, void * ud, void *) { ((rijSyncje *)ud)->klaar = true; };
	info.userdata1 	= &_rijSync;
	wgpuQueueOnSubmittedWorkDone(rij, info);
	while(!_rijSync.klaar)
		wgpuInstanceProcessEvents(instantie);
}

bool Simulatie::bewaarPNG(const std::string & bestand, int breedte, int hoogte, const std::vector<unsigned char> & rgba)
{
	png_image beeld;
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

// ── Klasse-implementatie ────────────────────────────────────────────────────

Simulatie::Simulatie(SimulatieConfig cfg) : _cfg(std::move(cfg))
{
	_overlayKeuze = _cfg.startOverlay; //start-overlay (bijv. --overlay 10 headless)
}

Simulatie::~Simulatie()
{
	if(_csvUit.is_open()) _csvUit.close();
	if(_diagLees) wgpuBufferRelease(_diagLees);
	if(_shotLees) wgpuBufferRelease(_shotLees);
	if(_offScreen) wgpuTextureRelease(_offScreen);
	if(_rekenParBuffer) wgpuBufferRelease(_rekenParBuffer);
	if(_penseelBuffer) wgpuBufferRelease(_penseelBuffer);
	if(_pickLees) wgpuBufferRelease(_pickLees);
	if(_pickTextuur) wgpuTextureRelease(_pickTextuur);
	delete _geo;
	delete _scherm;

	delete _gui;
	delete _gereedschap;
	delete _penseelGereedschap;
}

bool Simulatie::init()
{
	const bool heeftRender = !_cfg.hoofdloos || !_cfg.schermafbeeldingBestand.empty();
	_heeftRender = heeftRender;

	// ── Scherm ──────────────────────────────────────────────────────────
	_scherm = new weergaveSchermPerspectief("Planeet", 1280, 720, 8, _cfg.hoofdloos);

	// ── Shaders ─────────────────────────────────────────────────────────
	if(heeftRender)
	{
		_scherm->maakShader("planeetgridLand",  "shaders/planeetgridVertLand.wgsl",   "shaders/planeetgridFragLand.wgsl");
		_scherm->maakShader("planeetgridWater", "shaders/planeetgridVertWater.wgsl",  "shaders/planeetgridFragWater.wgsl");
		_scherm->maakShader("planeetgridIjs",   "shaders/planeetgridVertIjs.wgsl",    "shaders/planeetgridFragIjs.wgsl");
		_scherm->maakShader("planeetgridIjsOnder", "shaders/planeetgridVertIjsOnder.wgsl", "shaders/planeetgridFragIjs.wgsl");
		_scherm->maakShader("planeetgridWolk",    "shaders/planeetgridVertWolk.wgsl", "shaders/planeetgridFragWolk.wgsl");
		_scherm->maakShader("planeetgridAtmosfeer", "shaders/planeetgridVertAtmosfeer.wgsl", "shaders/planeetgridFragAtmosfeer.wgsl");
		_scherm->maakShader("planeetgridPick",  "shaders/planeetgridVertPick.wgsl",   "shaders/planeetgridFragPick.wgsl");
		_scherm->maakShader("planeetgridHoogtepunt", "shaders/planeetgridVertHoogtepunt.wgsl", "shaders/planeetgridFragHoogtepunt.wgsl");
	}

	_scherm->maakRekenShader("waterStroming",  "shaders/waterStroming.comp");
	_scherm->maakRekenShader("waterDruk",      "shaders/waterDruk.comp");
	_scherm->maakRekenShader("waterGemiddelde", "shaders/waterGemiddelde.comp");
	_scherm->maakRekenShader("waterSchijn",     "shaders/waterSchijn.comp");
	_scherm->maakRekenShader("luchtStroming",  "shaders/luchtStroming.comp");
	_scherm->maakRekenShader("vochtStroming",  "shaders/vochtStroming.comp");
	_scherm->maakRekenShader("waterLucht",     "shaders/waterLucht.comp");
	_scherm->maakRekenShader("zonSchijn",      "shaders/zonSchijn.comp");
	_scherm->maakRekenShader("penseel",        "shaders/penseel.comp");

	_scherm->maakDiepteShader("planeetSchaduw", "shaders/planeetgridVertSchaduw.wgsl");
	_maakSchaduwKaart();

	_scherm->zetWeergaveKleur(0, 0, 0, 1);

	// ── Texturen ────────────────────────────────────────────────────────
	if(!_cfg.procedural)
	{
		if(!_laadMola())
			return false;
	}
	else
	{
		_molaBreedte = _molaHoogte = 0;
		_molaData.clear();
	}

	// Bump-kaart voor water
	size_t bumpW, bumpH, bumpK;
	std::unique_ptr<unsigned char[], stbiDeleter> bumpData(laadAfbeelding("plaatjes/zeewater_bump.png", bumpW, bumpH, bumpK));
	if(!bumpData)
	{
		std::cerr << "Kon zeewater_bump.png niet laden!" << std::endl;
		return false;
	}
	if(heeftRender)
		_scherm->maakTextuur("waterBumpTex", bumpW, bumpH, true, true, false, GL_RGBA8, bumpData.get(), GL_RGBA, GL_UNSIGNED_BYTE);

	// ── Buffers ─────────────────────────────────────────────────────────
	WGPUDevice apparaat = weergaveScherm::deelApparaat();
	WGPUQueue  rij      = weergaveScherm::deelRij();

	WGPUBufferDescriptor parDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	parDesc.usage  = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
	parDesc.size   = sizeof(rekenParameters);
	_rekenParBuffer = wgpuDeviceCreateBuffer(apparaat, &parDesc);

	// CSV-bestand openen
	if(!_cfg.csvBestand.empty())
	{
		_csvUit.open(_cfg.csvBestand, std::ios::trunc);
		if(!_csvUit.is_open())
			std::cerr << "Kon " << _cfg.csvBestand << " niet openen!" << std::endl;
		else
			_csvOpen = true;
	}

	// Off-screen framebuffer voor screenshots
	if(!_cfg.schermafbeeldingBestand.empty() && _cfg.hoofdloos)
	{
		const int shotBreedte = 960, shotHoogte = 960;
		WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
		td.usage       = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
		td.dimension   = WGPUTextureDimension_2D;
		td.size        = {(uint32_t)shotBreedte, (uint32_t)shotHoogte, 1u};
		td.format      = WGPUTextureFormat_RGBA8Unorm;
		td.mipLevelCount = 1;
		td.sampleCount = 1;
		_offScreen = wgpuDeviceCreateTexture(apparaat, &td);
		_scherm->zetWeergaveDoel(_offScreen, glm::uvec2(shotBreedte, shotHoogte));

		WGPUBufferDescriptor rb = WGPU_BUFFER_DESCRIPTOR_INIT;
		rb.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
		rb.size  = (uint64_t)shotBreedte * shotHoogte * 4;
		_shotLees = wgpuDeviceCreateBuffer(apparaat, &rb);
	}

	// ── Planeet ─────────────────────────────────────────────────────────
	_maakPlaneet();

	// ── Penseel-buffer (header + dichte gewichten) ──────────────────────
	_maakPenseelBuffer();

	// ── Readback-buffer (--diagnose/--diagnoseCsv/--conservering/--veldKaart) ──
	const bool wilVeldKaart = !_cfg.veldKaarten.empty()
		|| _cfg.veldKaartFramesAantal > 0 || _cfg.veldKaartElkeFramesAantal > 0;
	if(_cfg.diagnoseAan || !_cfg.csvBestand.empty() || _cfg.conservatieAan || wilVeldKaart)
	{
		_diagGrootte = _geo->aantalVakjes() * sizeof(vak);
		WGPUBufferDescriptor leesBeschrijving = WGPU_BUFFER_DESCRIPTOR_INIT;
		leesBeschrijving.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
		leesBeschrijving.size = _diagGrootte;
		_diagLees = wgpuDeviceCreateBuffer(apparaat, &leesBeschrijving);
	}

	// ── Toetsenhandler en GUI ───────────────────────────────────────────
	if(!_cfg.hoofdloos)
	{
		weergaveScherm::toetsVerwerkerFunc toetsenbord = [this](int key, int scancode, int action, int mods)
		{
			if(_gui)
			{
				_gui->verwerkToets(key, scancode, action, mods);
				weergaveScherm::zetToetsGevangen(_gui->wilToetsen());
				if(_gui->wilToetsen())
					return;
			}
			(void)scancode; (void)mods;
			if(action == GLFW_PRESS)
				switch(key)
				{
				case GLFW_KEY_SPACE:
					_cfg.bevroren = !_cfg.bevroren;
					std::cout << "Simulatie " << (_cfg.bevroren ? "bevroren" : "ontdooid") << "." << std::endl;
					break;
				case GLFW_KEY_B:
					_zonRoteert = !_zonRoteert;
					std::cout << "Zon " << (_zonRoteert ? "draait om de oorsprong" : "staat stil in de viewer") << "." << std::endl;
					break;
				case GLFW_KEY_N:
					_cfg.schaduwAan = !_cfg.schaduwAan;
					std::cout << "Schaduw " << (_cfg.schaduwAan ? "aan" : "uit") << "." << std::endl;
					break;
				case GLFW_KEY_R:
					_roteerMaar = !_roteerMaar;
					std::cout << "Planeetrotatie " << (_roteerMaar ? "aan" : "uit") << "." << std::endl;
					break;
				case GLFW_KEY_X:
					_tekenWater = !_tekenWater;
					std::cout << "Water " << (_tekenWater ? "zichtbaar" : "onzichtbaar") << "." << std::endl;
					break;
				case GLFW_KEY_C:
					_tekenWolken = !_tekenWolken;
					std::cout << "Wolken " << (_tekenWolken ? "zichtbaar" : "onzichtbaar") << "." << std::endl;
					break;
				case GLFW_KEY_ESCAPE:
					if(_overlayKeuze != 0)
					{
						weergaveScherm::zetEscapeOverladen();
						_overlayKeuze = 0;
						std::cout << "Overlay uit." << std::endl;
					}
					break;
				case GLFW_KEY_1:
					_overlayKeuze = 0;
					std::cout << "Normale weergave." << std::endl;
					break;
				case GLFW_KEY_2:
					_overlayKeuze = 1;
					std::cout << "Temperatuuroverlay." << std::endl;
					break;
				case GLFW_KEY_3:
					_overlayKeuze = 2;
					std::cout << "Wind+drukoverlay." << std::endl;
					break;
				case GLFW_KEY_4:
					_overlayKeuze = 3;
					std::cout << "Bodemvochtoverlay." << std::endl;
					break;
				case GLFW_KEY_5:
					_overlayKeuze = 4;
					std::cout << "Luchtvocht/wolken/druk-overlay." << std::endl;
					break;
				case GLFW_KEY_6:
					_overlayKeuze = 5;
					std::cout << "IJs/water/bodemvocht-overlay." << std::endl;
					break;
				case GLFW_KEY_7:
					_overlayKeuze = 6;
					std::cout << "Wolkenoverlay." << std::endl;
					break;
				case GLFW_KEY_8:
					_overlayKeuze = 7;
					std::cout << "Zonlicht-overlay." << std::endl;
					break;
				case GLFW_KEY_9:
					_overlayKeuze = 8;
					std::cout << "Levens-overlay." << std::endl;
					break;
				case GLFW_KEY_0:
					_overlayKeuze = 9;
					std::cout << "Terreinhoogte-overlay." << std::endl;
					break;
				case GLFW_KEY_MINUS:
					_overlayKeuze = 11;
					std::cout << "Schaduw-overlay (ruwe schaduwkaart-factor)." << std::endl;
					break;
				case GLFW_KEY_ENTER:
					_waterStap = true;
					std::cout << "Eén sim-stap." << std::endl;
					break;
				case GLFW_KEY_SEMICOLON:
					_grondMult = glm::max(1.0f, _grondMult * 0.9f);
					std::cout << "GrondMult = " << _grondMult << "." << std::endl;
					break;
				case GLFW_KEY_APOSTROPHE:
					_grondMult = glm::max(1.0f, _grondMult * 1.1f);
					std::cout << "GrondMult = " << _grondMult << "." << std::endl;
					break;
				case GLFW_KEY_K:
					_verdamping = glm::max(0.0f, _verdamping - 0.001f);
					std::cout << "Verdamping = " << _verdamping << "." << std::endl;
					break;
				case GLFW_KEY_L:
					_verdamping = glm::max(0.0f, _verdamping + 0.001f);
					std::cout << "Verdamping = " << _verdamping << "." << std::endl;
					break;
				case GLFW_KEY_G:
					_coriolisOmega = glm::max(0.0f, _coriolisOmega - 0.05f);
					std::cout << "Coriolis = " << _coriolisOmega << "." << std::endl;
					break;
				case GLFW_KEY_H:
					_coriolisOmega = glm::min(2.0f, _coriolisOmega + 0.05f);
					std::cout << "Coriolis = " << _coriolisOmega << "." << std::endl;
					break;
				case GLFW_KEY_U:
					_zonKracht = glm::max(0.0f, _zonKracht - 5.0f);
					std::cout << "Zonkracht = " << _zonKracht << "." << std::endl;
					break;
				case GLFW_KEY_I:
					_zonKracht = glm::min(200.0f, _zonKracht + 5.0f);
					std::cout << "Zonkracht = " << _zonKracht << "." << std::endl;
					break;
				case GLFW_KEY_O:
					_wrijving = glm::max(0.0f, _wrijving - 0.01f);
					std::cout << "Wrijving = " << _wrijving << "." << std::endl;
					break;
				case GLFW_KEY_P:
					_wrijving = glm::min(1.0f, _wrijving + 0.01f);
					std::cout << "Wrijving = " << _wrijving << "." << std::endl;
					break;
				case GLFW_KEY_PERIOD:
					_neerslagFactor = glm::max(0.0f, _neerslagFactor - 0.1f);
					std::cout << "NeerslagFactor = " << _neerslagFactor << "." << std::endl;
					break;
				case GLFW_KEY_SLASH:
					_neerslagFactor = glm::max(0.0f, _neerslagFactor + 0.1f);
					std::cout << "NeerslagFactor = " << _neerslagFactor << "." << std::endl;
					break;
				}
		};
		_scherm->setCustomKeyhandler(toetsenbord);

		//GUI en gereedschap aanmaken; de muis/wiel/tekst-verwerkers koppelen.
		//De GUI krijgt alles het eerst; het gereedschap volgt zodra de GUI de muis
		//niet opeist (geen hovering/sleep boven een paneel of widget).
		_gui = new guiOverlay(*this);
		_gereedschap = new verplaatsGereedschap(_scherm);
		_penseelGereedschap = new penseelGereedschap(_scherm, _geo);

		//Sleep op de ACHTERGROND (pick mist de planeet) roteert de zichtbare zon in
		//het beeld (view-ruimte); sleep op de planeet draait de camera. De zon
		//beweegt dus ALLEEN door zonrotatie (B of deze sleep) — cameradraai van
		//de planeet laat haar vast staan in de viewer. De sim-zon merkt niets.
		_gereedschap->zetZonRoteer([this](double dx, double dy)
		{
			glm::mat4 zonDraai = glm::rotate(glm::mat4(1.0f), (float)(-dx * 0.005f), glm::vec3(0.0f, 1.0f, 0.0f))
			                   * glm::rotate(glm::mat4(1.0f), (float)(dy * 0.005f), glm::vec3(1.0f, 0.0f, 0.0f));
			_zonPos = glm::normalize(glm::vec3(zonDraai * glm::vec4(_zonPos, 0.0f)));
		});

		//Readback-buffer voor de pick-pass (1 texel, 256 bytes; bytesPerRow-alignment).
		WGPUBufferDescriptor pickDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
		pickDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
		pickDesc.size  = 256;
		_pickLees = wgpuDeviceCreateBuffer(weergaveScherm::deelApparaat(), &pickDesc);
		_pick.buffer = _pickLees;

		weergaveScherm::zetMuisPosVerwerker([this](double x, double y)
		{
			if(_gui) _gui->verwerkMuisPos(x, y);
			//Altijd doorgeven: het actieve gereedschap houdt zijn ankerpunt bij en
			//schildert/roteert alleen tijdens een lopende sleep (ook boven de GUI heen).
			gereedschap *t = _penseelActief ? (gereedschap*)_penseelGereedschap : (gereedschap*)_gereedschap;
			if(t) t->muisPos(x, y);
		});
		weergaveScherm::zetMuisKnopVerwerker([this](int knop, int actie, int mods)
		{
			if(_gui) _gui->verwerkMuisKnop(knop, actie, mods);
			gereedschap *t = _penseelActief ? (gereedschap*)_penseelGereedschap : (gereedschap*)_gereedschap;
			//Bovenop de GUI begint het gereedschap niet; een lopende sleep mag er
			//wel losgelaten worden (anders blijft de trackball/penseel hangen).
			if(t && (!(_gui && _gui->wilMuis()) || t->isBezig()))
				t->muisKnop(knop, actie, mods);
		});
		weergaveScherm::zetMuisWielVerwerker([this](double dx, double dy)
		{
			if(_gui) _gui->verwerkWiel(dx, dy);
			gereedschap *t = _penseelActief ? (gereedschap*)_penseelGereedschap : (gereedschap*)_gereedschap;
			//Horizontale swipe roteert / verticale scroll zoomt — behalve boven
			//de GUI, daar scrolt het paneel zelf.
			if(t && !(_gui && _gui->wilMuis()))
				t->muisWiel(dx, dy);
		});
		weergaveScherm::zetCharVerwerker([this](unsigned int c){ if(_gui) _gui->verwerkChar(c); });
	}

	return true;
}

void Simulatie::_maakSchaduwKaart()
{
	_scherm->vervangTextuur("zonSchaduwKaart", (size_t)_cfg.schaduwGrootte, (size_t)_cfg.schaduwGrootte,
	                        false, false, false, GL_DEPTH_COMPONENT32F, nullptr);
	_scherm->bindSchaduwKaart("zonSchaduwKaart");
}

void Simulatie::_maakPenseelBuffer()
{
	if(_penseelBuffer) { wgpuBufferRelease(_penseelBuffer); _penseelBuffer = nullptr; }

	WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT;
	bd.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
	bd.size  = sizeof(penseelBuffer) + _geo->aantalVakjes() * sizeof(float);
	_penseelBuffer = wgpuDeviceCreateBuffer(weergaveScherm::deelApparaat(), &bd);
}

bool Simulatie::_laadMola()
{
#ifdef __EMSCRIPTEN__
	//Web: haal MARS_Hoogte.png lazy van de server naar het virtuele FS.
	if(int rc = marsLaadHoogteUitBrowser())
	{
		std::cerr << "Kon MARS_Hoogte.png niet uit de browser laden (rc=" << rc << ")!" << std::endl;
		return false;
	}
#endif

	size_t w, h, kanalen;
	std::unique_ptr<unsigned char[], stbiDeleter> MarsHoogte(laadAfbeelding(_cfg.aarde ? "aarde.jpg" : "MARS_Hoogte.png", w, h, kanalen));
	if(!MarsHoogte)
	{
		std::cerr << "Kon " << (_cfg.aarde ? "aarde.jpg" : "MARS_Hoogte.png") << " niet laden! Gebruik --procedureel." << std::endl;
		return false;
	}

	// Spiegel boven-onder (verticaal) (RGBA, 4 kanalen per pixel)
	for(size_t y = 0; y < h / 2; y++)
		for(size_t x = 0; x < w; x++)
		{
			size_t boven   = (x + y * w) * 4;
			size_t onder   = (x + (h - 1 - y) * w) * 4;
			std::swap(MarsHoogte[boven],     MarsHoogte[onder]);
			std::swap(MarsHoogte[boven + 1], MarsHoogte[onder + 1]);
			std::swap(MarsHoogte[boven + 2], MarsHoogte[onder + 2]);
			std::swap(MarsHoogte[boven + 3], MarsHoogte[onder + 3]);
		}

	if(_heeftRender)
		_scherm->vervangTextuur("marsHoogteTex", w, h, true, false, false, GL_RGBA8, MarsHoogte.get(), GL_RGBA, GL_UNSIGNED_BYTE);

	// Bewaar grijswaarden voor de altura-lambda (rode kanaal / 255.0)
	_molaBreedte = w;
	_molaHoogte  = h;
	_molaData.resize(w * h);
	for(size_t y = 0; y < h; y++)
		for(size_t x = 0; x < w; x++)
			_molaData[y * w + x] = (float)MarsHoogte[(y * w + x) * 4] / 255.0f;

	return true;
}

void Simulatie::_maakPlaneet()
{
	planeetInit init;
	init.water       = _cfg.startWater;
	init.bodemVocht  = _cfg.startBodemVocht;
	init.wolken      = _cfg.startWolken;
	init.leven       = _cfg.startLeven;
	init.ijs         = _cfg.startIjs;
	init.damp        = _cfg.startDamp;
	init.zandDeksel  = _cfg.startZandDeksel;
	init.temperatuur = _cfg.startTemperatuur;

	if(_cfg.procedural)
	{
		//Vast zaadje meegeven (--zaadje N) maakt het terrein reproduceerbaar; anders
		//wordt elk draaien een nieuwe wereld (nodig voor de headless A/B-workflow).
		uint32_t zaad = _cfg.zaadje ? _cfg.zaadje : (uint32_t)std::random_device{}();
		std::cout << "Zaadje: " << zaad << std::endl;

		//Zaad-afgeleide verschuiving van het ruisrooster: elk zaadje verschuift de
		//hash-lattice over een andere afstand, zodat het terrein per zaadje verschilt.
		auto zaadOffset = [](uint32_t z) -> glm::vec3
		{
			float a = glm::fract(glm::sin(float(z) * 12.9898f) * 43758.5453f);
			float b = glm::fract(glm::sin(float(z ^ 0x9E3779B9u) * 12.9898f) * 43758.5453f);
			float c = glm::fract(glm::sin(float(z ^ 0x85EBCA6Bu) * 12.9898f) * 43758.5453f);
			return glm::vec3(a, b, c) * 64.0f;
		};
		glm::vec3 offset = zaadOffset(zaad);

		auto hashN = [offset](glm::vec3 c) -> float
		{
			return glm::fract(glm::sin((c.x + offset.x) * 127.1f + (c.y + offset.y) * 311.7f + (c.z + offset.z) * 74.7f) * 43758.5453f);
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
		auto altura = [&, this](glm::vec3 pos) -> float
		{
			float n = fbm(pos * 2.2f) * 2.0f - 1.0f;
			float versterking = 2.0f + std::abs(n);
			float h = 60.0f + 10.0f * n * versterking;
			return glm::clamp(h, 40.0f, 140.0f);
		};
		_geo = new planeet(_cfg.subdiv, altura, init);
	}
	else
	{
		std::function<float(glm::vec2)> molaHoogte = [this](glm::vec2 plek) -> float
		{
			size_t px = (size_t)std::floor(plek.x * (_molaBreedte - 1));
			size_t py = (size_t)std::floor(plek.y * (_molaHoogte - 1));
			px = std::min(px, _molaBreedte - 1);
			py = std::min(py, _molaHoogte - 1);
			return _grondMult + 10.0f * _molaData[py * _molaBreedte + px];
		};
		_geo = new planeet(_cfg.subdiv, molaHoogte, init);
	}
}

void Simulatie::_resetStaat()
{
	_frameNummer     = 0;
	_jaarTeller      = 0;
	_zonSlotTeller   = 0;
	_waterStap       = false;
	_loopStart       = std::chrono::steady_clock::now();
	_vorigeFrameTijd = _loopStart;
}

bool Simulatie::herstart(const SimulatieConfig & nieuweCfg)
{
	_cfg = nieuweCfg;

	_maakSchaduwKaart();

	_molaData.clear();
	_molaBreedte = _molaHoogte = 0;
	if(!_cfg.procedural && !_laadMola())
	{
		std::cerr << "MOLA kon niet geladen worden; val terug op procedureel." << std::endl;
		_cfg.procedural = true;
	}

	delete _geo;
	_geo = nullptr;
	_maakPlaneet();

	//Readback-buffer op maat van het nieuwe aantal vakjes: bij een andere diepte
	//past de oude grootte niet meer bij huidigeOpslag() (overrun of dubbele map-fout).
	if(_diagLees)
	{
		_diagGrootte = _geo->aantalVakjes() * sizeof(vak);
		wgpuBufferRelease(_diagLees);
		WGPUBufferDescriptor leesBeschrijving = WGPU_BUFFER_DESCRIPTOR_INIT;
		leesBeschrijving.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
		leesBeschrijving.size = _diagGrootte;
		_diagLees = wgpuDeviceCreateBuffer(weergaveScherm::deelApparaat(), &leesBeschrijving);
	}

	//Penseel-buffer (andere celgrootte) + gereedschap opnieuw koppelen aan de planeet.
	_maakPenseelBuffer();
	if(_penseelGereedschap)
	{
		delete _penseelGereedschap;
		_penseelGereedschap = new penseelGereedschap(_scherm, _geo);
	}

	_resetStaat();
	return true;
}

Simulatie::Tunables Simulatie::tunables()
{
	return {
		&_zonKracht, &_elips, &_obliquity, &_verwarmtijd, &_stralingKracht,
		&_coriolisOmega, &_wrijving, &_diffusie,
		&_verdamping, &_basisVerzadiging, &_neerslagFactor, &_orografieFactor,
		&_grondMult, &_grondSchaal,
		&_zandErosie, &_rotsErosie, &_bezinkheid, &_zandRepose, &_ijsRepose, &_ijsTempo, &_hellingKracht, &_oplosheid,
		&_evapotranspiratie, &_infiltratie, &_bodemDiffusie, &_veldCapaciteit,
		&_condensTempo, &_regenTempo, &_wolkVerdamp, &_wolkDiffusie,
		&_levenGroeiBand, &_levenDroogTempo, &_levenVerwelk, &_levenKoudTempo,
		&_zandGroei, &_zandBuur, &_rotsGroei, &_rotsBuur,
		&_levensDamp, &_waterDoodTempo,
		&_cfg.bevroren, &_waterStroomt, &_tekenWater, &_tekenIjs, &_tekenWolken, &_zonRoteert, &_roteerMaar,
		&_cfg.schaduwAan, &_cfg.erosieAan, &_cfg.levenAan, &_cfg.atmosfeerAan, &_waterStap,
		&_overlayKeuze, &_wolkAlpha, &_waterReflectie, &_atmosfeerSterkte, &_atmosfeerDikte, &_cfg.luchtStappen
	};
}

bool Simulatie::stopGewenst() const
{
	if(_cfg.hoofdloos)
		return _cfg.stappenTotaal > 0 && _frameNummer >= _cfg.stappenTotaal;
	return _scherm->stopGewenst();
}

void Simulatie::stap()
{
	if(stopGewenst()) return;

	// ── Frametijd + GUI new-frame ───────────────────────────────────────
	{
		auto nu = std::chrono::steady_clock::now();
		double dt = std::chrono::duration<double>(nu - _vorigeFrameTijd).count();
		_vorigeFrameTijd = nu;
		if(dt <= 0.0) dt = 0.001;
		_frameTijdMS = (float)(dt * 1000.0);
		_fps         = (float)(1.0 / dt);
		if(_gui) _gui->beginFrame((float)dt);
	}

	// ── Zon: de render-zon staat vast in de VIEWER (een kijkrichting) ─────
	//Alleen zonrotatie beweegt haar: B draait haar omhoog/opzij in het beeld,
	//sleep op de achtergrond roteert haar met de hand. Camera-/planeetdraai doet
	//niets met _zonPos (de extra-uniformz wordt per frame naar modelruimte omgezet
	//zodat de schaduwkaart/fragments hem in het beeld stilhouden). De sim-zon
	//(rekenPar.zonRicht hieronder) staat daar volledig los van.
	if(!_cfg.bevroren && _zonRoteert)
		_zonPos = glm::normalize(glm::vec3(
			glm::rotate(glm::mat4(1.0f), 0.003f, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::vec4(_zonPos, 0.0f)));

	if(!_cfg.bevroren && _roteerMaar)
		_scherm->zetModelZicht(glm::rotate(_scherm->modelZicht(), 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)));

	// ── Uniformen ───────────────────────────────────────────────────────
	_kijkPlek = glm::vec3(glm::inverse(_scherm->modelZicht())[3]);
	//De viewer-zon is een kijkrichting; modelruimte-versie voor shaders en
	//schaduwkaart: inverse(MZ) — cameradraai laat haar daardoor in beeld vast.
	glm::vec3 zonModel = glm::normalize(glm::vec3(
		glm::inverse(_scherm->modelZicht()) * glm::vec4(_zonPos, 0.0f)));
	_extra[0]  = _grondMult;
	_extra[1]  = _grondSchaal;
	_extra[2]  = (float)_cfg.schaduwGrootte;
	_extra[3]  = _wolkAlpha;
	_extra[11] = _atmosfeerSterkte;
	_extra[4]  = _kijkPlek.x; _extra[5] = _kijkPlek.y; _extra[6] = _kijkPlek.z;
	_extra[7]  = _waterReflectie;
	_extra[8]  = zonModel.x;  _extra[9] = zonModel.y;   _extra[10] = zonModel.z;
	_extra[12] = _geo->hoogsteGrond();
	_extra[13] = (float)_overlayKeuze;
	_extra[14] = _atmosfeerDikte;
	_extra[15] = _cfg.schaduwAan ? 1.0f : 0.0f;
	_scherm->zetExtraFloats(_extra, 16);

	rekenParameters rekenPar = {};
	rekenPar.grondSchaal  = _grondSchaal;
	rekenPar.verdamping   = _verdamping;
	rekenPar.erosie       = _cfg.erosieAan ? 1.0f : 0.0f;
	rekenPar.levenAan     = _cfg.levenAan ? 1.0f : 0.0f;

	//Seizoenen uit de baanellips: hoe excentrieker de baan, hoe sterker het
	//zomer/winter-verschil in zonkracht. Gemiddeld blijft de zonkracht gelijk;
	//de sim-zon hieronder gaat óók met de askanteling de declinatie in, zodat
	//het ene halfrond zomer heeft als het andere winter (seizoen = 1000 frames).
	constexpr float pi       = 3.14159265f;
	constexpr float dagSlots = 360.0f;
	constexpr float jaarLengte = 4000.0f;
	constexpr float perihelFase = 0.0f;
	float jaarHoek    = 2.0f * pi * (float)(_jaarTeller % (size_t)jaarLengte) / jaarLengte;
	float seizoenFactor = 1.0f + _elips * glm::cos(jaarHoek - perihelFase);
	rekenPar.atmosfeer[0] = _cfg.atmosfeerAan ? _zonKracht * seizoenFactor : 0.0f;
	rekenPar.atmosfeer[1] = _coriolisOmega;
	rekenPar.atmosfeer[2] = _cfg.atmosfeerAan ? _wrijving : 0.0f;
	rekenPar.atmosfeer[3] = _cfg.atmosfeerAan ? _diffusie : 0.0f;
	//Sim-zonrichting van deze frame: declinatie uit de askanteling + jaarhoek,
	//dag-rotatie uit de slot-teller (dezelfde daglengte als het zonlicht-EMA).
	//De sim start in de WINTER: declinatie = −askanteling bij frame 0 (noordpool
	//donker, zuidpool helder), lente-equinox op frame 1000, zomer op 2000.
	float declinatie = -_obliquity * glm::cos(jaarHoek);
	float slotHoek   = 2.0f * pi * (float)(_zonSlotTeller % (size_t)dagSlots) / dagSlots;
	glm::vec3 simZon   = glm::normalize(glm::vec3(
		glm::cos(declinatie) * glm::sin(slotHoek),
		glm::sin(declinatie),
		glm::cos(declinatie) * glm::cos(slotHoek)));
	rekenPar.zonRicht[0]  = simZon.x; rekenPar.zonRicht[1] = simZon.y;
	rekenPar.zonRicht[2]  = simZon.z; rekenPar.zonRicht[3] = 0.0f;
	rekenPar.condenseer[0] = _basisVerzadiging;
	rekenPar.condenseer[1] = 0.0f; //voormalig hoogteKoel: de lapse zit nu in het WGSL-gedeelde lapse
	rekenPar.condenseer[2] = _neerslagFactor;
	rekenPar.condenseer[3] = _orografieFactor;
	rekenPar.fasen[0]      = _verwarmtijd;
	rekenPar.fasen[1]      = _geo->hoogsteGrond();
	rekenPar.fasen[2]      = _grondMult;
	rekenPar.fasen[3]      = std::pow(4.0f, (float)(_cfg.subdiv - 6));
	rekenPar.schaduw[0]    = _cfg.schaduwAan ? 1.0f : 0.0f;
	rekenPar.schaduw[1]    = (float)_cfg.schaduwGrootte;
	rekenPar.schaduw[2]    = _stralingKracht;

	// ── Runtimetunables (erosie/water/leven/wolken) ────────────────────
	rekenPar.erosiePar[0]  = _zandErosie;
	rekenPar.erosiePar[1]  = _rotsErosie;
	rekenPar.erosiePar[2]  = _bezinkheid;
	rekenPar.erosiePar[3]  = _zandRepose;
	rekenPar.erosiePar2[0] = _hellingKracht;
	rekenPar.erosiePar2[1] = _oplosheid;
	rekenPar.erosiePar2[2] = _ijsRepose;
	rekenPar.erosiePar2[3] = _ijsTempo;
	rekenPar.waterPar[0]   = _evapotranspiratie;
	rekenPar.waterPar[1]   = _infiltratie;
	rekenPar.waterPar[2]   = _bodemDiffusie;
	rekenPar.waterPar[3]   = _veldCapaciteit;
	rekenPar.levenPar[0]   = _levenGroeiBand;
	rekenPar.levenPar[1]   = _levenDroogTempo;
	rekenPar.levenPar[2]   = _levenVerwelk;
	rekenPar.levenPar[3]   = _levenKoudTempo;
	rekenPar.groeiPar[0]   = _zandGroei;
	rekenPar.groeiPar[1]   = _zandBuur;
	rekenPar.groeiPar[2]   = _rotsGroei;
	rekenPar.groeiPar[3]   = _rotsBuur;
	rekenPar.wolkPar[0]    = _condensTempo;
	rekenPar.wolkPar[1]    = _regenTempo;
	rekenPar.wolkPar[2]    = _wolkVerdamp;
	rekenPar.wolkPar[3]    = _wolkDiffusie;
	rekenPar.levenPar2[0]  = _levensDamp;
	rekenPar.levenPar2[1]  = _waterDoodTempo;

	wgpuQueueWriteBuffer(weergaveScherm::deelRij(), _rekenParBuffer, 0, &rekenPar, sizeof(rekenParameters));

	// ── Penseel: pick (cel onder de cursor) + buffer bijwerken ──────────
	if(!_cfg.hoofdloos && _penseelActief && _penseelGereedschap)
	{
		if((_gui && _gui->wilMuis()) || !_penseelGereedschap->heeftPlek())
			_penseelGereedschap->zetCenterId(geenCelId);
		else
		{
			uint32_t w = _scherm->oppervlakBreedte(), h = _scherm->oppervlakHoogte();
			_penseelGereedschap->zetCenterId(doePickPass(
				(uint32_t)_penseelGereedschap->texelX((int)w),
				(uint32_t)_penseelGereedschap->texelY((int)h)));
		}
	}
	else if(_penseelGereedschap && _penseelGereedschap->heeftCursor())
	{
		//Penseel niet langer actief: cursor/highlight weghalen.
		_penseelGereedschap->zetCenterId(geenCelId);
	}
	//Verplaats-gereedschap: één pick bij het begin van een sleep beslist de modus:
	//planeet geraakt = cameradraai (trackball); achtergrond = de zichtbare zon
	//roteert om de origin. De gebufferde eerste delta wordt bij zetModus geflushed.
	if(!_cfg.hoofdloos && !_penseelActief && _gereedschap &&
	   _gereedschap->isBezig() && _gereedschap->modusOnbekend())
	{
		uint32_t w = _scherm->oppervlakBreedte(), h = _scherm->oppervlakHoogte();
		uint32_t id = doePickPass((uint32_t)_gereedschap->cursorTexelX((int)w),
		                          (uint32_t)_gereedschap->cursorTexelY((int)h));
		_gereedschap->zetModus(id == geenCelId ? verplaatsGereedschap::mZon : verplaatsGereedschap::mPlaneet);
	}
	if(_penseelGereedschap && _penseelGereedschap->bufferVuil())
	{
		_penseelGereedschap->vulBuffer();
		wgpuQueueWriteBuffer(weergaveScherm::deelRij(), _penseelBuffer, 0,
		                     &_penseelGereedschap->buffer(), sizeof(penseelBuffer));
		wgpuQueueWriteBuffer(weergaveScherm::deelRij(), _penseelBuffer, sizeof(penseelBuffer),
		                     _penseelGereedschap->gewichten().data(),
		                     _penseelGereedschap->gewichten().size() * sizeof(float));
		_penseelGereedschap->markeerSchoon();
	}

	// ── Schaduwpass ─────────────────────────────────────────────────────
	doeSchaduwPass();

	// ── Renderpasses ────────────────────────────────────────────────────
	if(!_cfg.hoofdloos)
		doeRenderPassen();

	// ── Penseel-toepassing (schilderen; werkt ook als de sim bevroren is) ──
	if(_penseelGereedschap && _penseelGereedschap->schildert())
	{
		const uint32_t penseelGroepen = (uint32_t)((_geo->aantalVakjes() + 63) / 64);
		_scherm->doeRekenVerwerker("penseel", glm::uvec3(penseelGroepen, 1, 1), [this]()
		{
			_geo->bindVrwrkrOpslagen(*_scherm);
			_scherm->verbindRekenBuffer(4, _penseelBuffer);
		});

		//Een water-edit verandert waterHoogte, maar de render toont waterSchijn (het
		//gladgestreken gemiddelde). Bij een bevroren sim draait de rekenketen niet, dus
		//herberekenen we waterSchijn hier direct zodat de edit meteen zichtbaar is.
		//Bij ijs-edit moet ook ijsSchijn worden bijgewerkt.
		if(_penseelGereedschap->mode() == penseelModeWater || _penseelGereedschap->mode() == penseelModeIjs)
		{
			_scherm->doeRekenVerwerker("waterSchijn", glm::uvec3(penseelGroepen, 1, 1), [this]()
			{
				_geo->bindVrwrkrOpslagen(*_scherm);
			});
		}
	}

	// ── Rekenketen ──────────────────────────────────────────────────────
	if(!_cfg.bevroren && (_waterStroomt || _waterStap || _cfg.hoofdloos))
	{
		const uint32_t rekenGroepen = (uint32_t)((_geo->aantalVakjes() + 63) / 64);

		auto berekenShaderBinden = [this]()
		{
			_geo->bindVrwrkrOpslagen(*_scherm);
			_scherm->verbindRekenBuffer(3, _rekenParBuffer);
		};

		for(int substap = 0; substap < _cfg.luchtStappen; substap++)
		{
			_scherm->doeRekenVerwerker("waterStroming",  glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("waterDruk",      glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("waterGemiddelde", glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("luchtStroming",  glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("vochtStroming",  glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("waterLucht",     glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_geo->volgendeRonde();
		}
		//Jaar/dag lopen met de rekenketen mee; daarna één frame zonlicht-EMA
		//(zonSchijn, op de verse vakken0), zodat de dagomloop compleet wordt.
		_jaarTeller++;
		_zonSlotTeller++;
		doeZonSchijnPass();
		_waterStap = false;
	}

	// ── Async-readbacks (--diagnose/--diagnoseCsv/--conservering/--veldKaart*) ──
	if(_diagLees)
	{
		//--diagnose: print de extremen van de reken-stand (interactief native;
		//hoofdloos alleen een compacte vinger-aan-de-pols-regel).
		if(_cfg.diagnoseAan && !_cfg.hoofdloos && _frameNummer % 25 == 0)
		{
			diagToestandje toestand;
			toestand.buffer  = _diagLees;
			toestand.grootte = _diagGrootte;
			toestand.geo     = _geo;
			toestand.teller  = _frameNummer;
			drainVakken(diagVerwerker, &toestand, &toestand.klaar);
		}
		else if(_cfg.diagnoseAan && _cfg.hoofdloos && _frameNummer % 25 == 0)
		{
			//compacte vinger-aan-de-pols-regel in hoofdloze modus; het volledige
			//teruglezen gaat daar via --diagnoseCsv.
			std::cerr << "[stap " << _frameNummer << "]" << std::endl;
		}

		//--diagnoseCsv: dump de hele planeet naar CSV.
		if(!_cfg.csvBestand.empty() && _csvUit.is_open() && _frameNummer % _cfg.csvElkeFrames == 0)
		{
			csvToestandje toestand;
			toestand.buffer  = _diagLees;
			toestand.grootte = _diagGrootte;
			toestand.geo     = _geo;
			toestand.teller  = _frameNummer;
			toestand.uit     = &_csvUit;
			drainVakken(csvVerwerker, &toestand, &toestand.klaar);
		}

		//--conservering: houd de totale watermassa bij (evaluatie gebeurt post-loop).
		if(_cfg.conservatieAan && _frameNummer > 0 && _frameNummer % 200 == 0)
		{
			conservatieStat stat;
			conservatieToestandje toestand;
			toestand.buffer  = _diagLees;
			toestand.grootte = _diagGrootte;
			toestand.stat    = &stat;
			drainVakken(conservatieVerwerker, &toestand, &toestand.klaar);

			double totaal = stat.water + stat.bodem + stat.ijs + stat.damp + stat.wolk;
			if(_totaalWaterStart < 0.0)
			{
				_totaalWaterStart = totaal;
				_totaalWaterMin   = totaal;
				_totaalWaterMax   = totaal;
			}
			else
			{
				_totaalWaterMin = std::min(_totaalWaterMin, totaal);
				_totaalWaterMax = std::max(_totaalWaterMax, totaal);
			}
		}
	}

	//--veldKaartFrames: schrijf de laatste n frames (hoofdloos) als grond-heatmaps.
	if(_cfg.hoofdloos && _cfg.veldKaartFramesAantal > 0 && _diagLees &&
	   _cfg.stappenTotaal >= _cfg.veldKaartFramesAantal &&
	   _frameNummer < _cfg.stappenTotaal &&
	   _frameNummer >= _cfg.stappenTotaal - _cfg.veldKaartFramesAantal)
	{
		size_t idx = _frameNummer - (_cfg.stappenTotaal - _cfg.veldKaartFramesAantal);
		veldKaartToestandje toestand;
		toestand.buffer  = _diagLees;
		toestand.grootte = _diagGrootte;
		toestand.geo     = _geo;
		toestand.factor  = _cfg.kaartFactor;
		toestand.kaarten = {{"grond", "veldkaart" + std::to_string(idx) + ".png"}};
		drainVakken(veldKaartVerwerker, &toestand, &toestand.klaar);
	}

	//--veldKaartElkeFrames: schrijf elke n frames (hoofdloos) als veldkaart_N.png.
	if(_cfg.hoofdloos && _cfg.veldKaartElkeFramesAantal > 0 && _diagLees &&
	   _frameNummer > 0 && _frameNummer % _cfg.veldKaartElkeFramesAantal == 0)
	{
		size_t idx = _frameNummer / _cfg.veldKaartElkeFramesAantal;
		veldKaartToestandje toestand;
		toestand.buffer  = _diagLees;
		toestand.grootte = _diagGrootte;
		toestand.geo     = _geo;
		toestand.factor  = _cfg.kaartFactor;
		toestand.kaarten = {{_cfg.veldKaartElkeFramesVeld, "veldkaart_" + std::to_string(idx) + ".png"}};
		drainVakken(veldKaartVerwerker, &toestand, &toestand.klaar);
	}

	_frameNummer++;

	// ── Frametijd meten ─────────────────────────────────────────────────
	if(_frameNummer % 120 == 0)
	{
		auto nu = std::chrono::steady_clock::now();
		double ms = std::chrono::duration<double, std::milli>(nu - _loopStart).count();
		double gemPerFrame = ms / (double)(std::max((size_t)1, _frameNummer));
		std::cerr << "[tijd] frame " << _frameNummer << ": gemiddeld " << gemPerFrame
				  << " ms/frame (" << (1000.0 / (gemPerFrame > 0.0 ? gemPerFrame : 1.0)) << " fps)\n";
		_loopStart = nu;
	}

	wgpFoutControle("Frame: ");
}

// ── Private helpers ─────────────────────────────────────────────────────────

void Simulatie::doeZonSchijnPass()
{
	//Eén frame van het zonlicht-EMA: vakken0.zonlicht += (nieuw − oud)/dagSlots,
	//met nieuw = invalsFactor × horizonversperring voor de sim-zonrichting van
	//deze frame (rekenPar.zonRicht). Geen shadowmap: de render-zon staat los.
	const uint32_t groepen = (uint32_t)((_geo->aantalVakjes() + 63) / 64);
	_scherm->doeRekenVerwerker("zonSchijn", glm::uvec3(groepen, 1, 1), [this]()
	{
		_geo->bindVrwrkrOpslagen(*_scherm);
		_scherm->verbindRekenBuffer(3, _rekenParBuffer);
	});
}

void Simulatie::doeSchaduwPass()
{
	if(!_cfg.schaduwAan) return;

	weergaveInstellingen schaduwInstellingen;
	schaduwInstellingen.cullMode = WGPUCullMode_Front;
	_scherm->zetWeergaveInstellingen(schaduwInstellingen);

	_scherm->bindSchaduwKaart("");
	_scherm->zetDiepteDoel(_scherm->textuurId("zonSchaduwKaart"),
	                       glm::uvec2((uint32_t)_cfg.schaduwGrootte, (uint32_t)_cfg.schaduwGrootte));
	_scherm->bereidRenderVoor("planeetSchaduw");
	_geo->bindVrwrkrOpslagen(*_scherm);
	_geo->tekenJezelf();
	_scherm->pasRondRenderAf();
	_scherm->rondRenderAf();
	_scherm->zetDiepteDoel(nullptr);
	_scherm->zetWeergaveInstellingen(weergaveInstellingen());
	_scherm->ontkoppelRekenBuffers();
	_scherm->bindSchaduwKaart("zonSchaduwKaart");
}

void Simulatie::doeRenderPassen()
{
	_kijkPlek = glm::vec3(glm::inverse(_scherm->modelZicht())[3]);
	_scherm->zetExtraFloats(_extra, 16);

	//De eerste pass van de frame wist kleur + diepte; de rest tekent eroverheen
	//(Load). Atmosfeer en wolken staan er vóór (geflipt = achterkanten) én na (voorkanten),
	//zodat het doorzichtige dek correct t.o.v. de planeet wordt gesorteerd.
	bool eerstePass = true;

	// ── Atmosfeer: Rayleigh-gloed achter de planeet (schil-achterkanten) ──
	if(_overlayKeuze == 0)
	{
		weergaveInstellingen atmosfeerInstellingen;
		atmosfeerInstellingen.blenden = true;
		atmosfeerInstellingen.cullMode = WGPUCullMode_Front;
		atmosfeerInstellingen.diepteSchrijven = false;
		atmosfeerInstellingen.diepteVergelijk = WGPUCompareFunction_Less;
		_scherm->zetWeergaveInstellingen(atmosfeerInstellingen);

		_scherm->bereidRenderVoor("planeetgridAtmosfeer", eerstePass);
		_geo->bindVrwrkrOpslagen(*_scherm);
		_geo->tekenJezelf();
		_scherm->pasRondRenderAf();
		eerstePass = false;
	}

	// ── Wolken: achterkanten eerst (geflipt) ────────────────────────────
	if(_tekenWolken && _overlayKeuze == 0)
	{
		weergaveInstellingen wolkInstellingen;
		wolkInstellingen.blenden = true;
		wolkInstellingen.cullMode = WGPUCullMode_Front;
		wolkInstellingen.diepteSchrijven = false;
		wolkInstellingen.diepteVergelijk = WGPUCompareFunction_Less;
		_scherm->zetWeergaveInstellingen(wolkInstellingen);

		_scherm->bereidRenderVoor("planeetgridWolk", eerstePass);
		_geo->bindVrwrkrOpslagen(*_scherm);
		_geo->tekenJezelf();
		_scherm->pasRondRenderAf();
		eerstePass = false;
	}

	// Grond-pass
	weergaveInstellingen grondInstellingen;
	grondInstellingen.cullMode = WGPUCullMode_Back;
	_scherm->zetWeergaveInstellingen(grondInstellingen);

	_scherm->bereidRenderVoor("planeetgridLand", eerstePass);
	_geo->bindVrwrkrOpslagen(*_scherm);
	if(!_cfg.procedural)
		_scherm->bindTextuur("marsHoogteTex", 0);
	_geo->tekenJezelf();
	_scherm->pasRondRenderAf();
	eerstePass = false;

	// IJs-passen (drijvend op de waterspiegel): onder- én bovenkant vóór het water.
	// Beide schrijven diepte, zodat het transparante water (LessEqual, geen
	// diepte-schrijf) daarna alleen tekent waar het vóór het ijs ligt: een grote
	// waterbult die boven het ijs uitsteekt bedekt het ijs correct, en waar het
	// water ónder de ijs-top ligt faalt de dieptetest (ijs blijft schoon).
	if(_tekenIjs && _overlayKeuze == 0)
	{
		weergaveInstellingen ijsInstellingen;
		ijsInstellingen.cullMode = WGPUCullMode_Front;
		ijsInstellingen.diepteSchrijven = true;
		ijsInstellingen.diepteVergelijk = WGPUCompareFunction_LessEqual;
		_scherm->zetWeergaveInstellingen(ijsInstellingen);

		_scherm->bereidRenderVoor("planeetgridIjsOnder", false);
		_geo->bindVrwrkrOpslagen(*_scherm);
		_geo->tekenJezelf();
		_scherm->pasRondRenderAf();

		//Bovenkant (cull Back, echte ijsdikte) direct erbovenop.
		ijsInstellingen.cullMode = WGPUCullMode_Back;
		_scherm->zetWeergaveInstellingen(ijsInstellingen);

		_scherm->bereidRenderVoor("planeetgridIjs", false);
		_geo->bindVrwrkrOpslagen(*_scherm);
		_geo->tekenJezelf();
		_scherm->pasRondRenderAf();
	}

	// Water-pass: tekent na het ijs; op de waterlijn ligt de ijs-onderkant op
	// gelijke hoogte als het water, dus LessEqual laat het water daar over die
	// rand tekenen (de ijswand onder de waterlijn krijgt een watertint).
	if(_tekenWater && _overlayKeuze == 0)
	{
		weergaveInstellingen waterInstellingen;
		waterInstellingen.blenden = true;
		waterInstellingen.cullMode = WGPUCullMode_Back;
		//Water schrijft óók diepte: overlappende wateroppervlakken (bulten, golf-
		//silhouetten) worden dan per pixel op ware diepte gesorteerd i.p.v. op
		//driehoek-volgorde — anders zie je het achterliggende oppervlak door de
		//bult heen. Minder: een oppervlak ónder semi-transparant water doet niet
		//meer mee (je ziet daar de grond), maar dat is de minste van twee kwaden.
		waterInstellingen.diepteSchrijven = true;
		waterInstellingen.diepteVergelijk = WGPUCompareFunction_LessEqual;
		_scherm->zetWeergaveInstellingen(waterInstellingen);

		_scherm->bereidRenderVoor("planeetgridWater", false);
		_geo->bindVrwrkrOpslagen(*_scherm);
		_scherm->bindTextuur("waterBumpTex", 0);
		_geo->tekenJezelf();
		_scherm->pasRondRenderAf();
	}

	// ── Wolken: voorkanten als laatste (bovenop) ────────────────────────
	if(_tekenWolken && _overlayKeuze == 0)
	{
		weergaveInstellingen wolkInstellingen;
		wolkInstellingen.blenden = true;
		wolkInstellingen.cullMode = WGPUCullMode_Back;
		wolkInstellingen.diepteSchrijven = false;
		wolkInstellingen.diepteVergelijk = WGPUCompareFunction_Less;
		_scherm->zetWeergaveInstellingen(wolkInstellingen);

		_scherm->bereidRenderVoor("planeetgridWolk", false);
		_geo->bindVrwrkrOpslagen(*_scherm);
		_geo->tekenJezelf();
		_scherm->pasRondRenderAf();
	}

	// ── Highlight + cursor-pass (penseel-bereik; zie doeHoogtepuntPass) ──
	if(_penseelGereedschap && _penseelGereedschap->heeftCursor())
		doeHoogtepuntPass();

	// ── GUI-pass (Dear ImGui) bovenop de planeet ───────────────────────
	if(_gui)
	{
		_gui->tekenInPass(_scherm);
	}

	_scherm->rondRenderAf();
	_scherm->zetWeergaveInstellingen(weergaveInstellingen());
	_scherm->ontkoppelRekenBuffers();
}

void Simulatie::doeHoogtepuntPass()
{
	weergaveInstellingen inst;
	inst.blenden         = true;
	inst.cullMode        = WGPUCullMode_Back;
	inst.diepteSchrijven = false;
	inst.diepteVergelijk = WGPUCompareFunction_LessEqual;
	_scherm->zetWeergaveInstellingen(inst);

	_scherm->bereidRenderVoor("planeetgridHoogtepunt", false);
	_geo->bindVrwrkrOpslagen(*_scherm);
	_scherm->verbindRekenBuffer(4, _penseelBuffer);
	_geo->tekenJezelf();
	_scherm->pasRondRenderAf();
}

///Decodeert een afgeronde pick-map (buffer is mapped) en maakt de buffer weer
///vrij. Native: meteen na de synchrone wait in doePickPass. Web: bij de volgende
///doePickPass-aanroep — getMappedRange blijft geldig tot unmap en de map-callback
///vuurt (AllowSpontaneous) tussen frames op de JS-eventloop.
void Simulatie::_rondPickAf()
{
	if(!_pick.klaar || !_pick.buffer)
		return;

	//Let op: read-map dus GetConstMappedRange — emdawnwebgpu's GetMappedRange
	//levert bij een read-only map steevast nullptr (wgpu-native maakt dat
	//onderscheid niet, maar de const-variant werkt aan beide kanten).
	const unsigned char * p = (const unsigned char *)wgpuBufferGetConstMappedRange(_pick.buffer, 0, 256);
	_pick.id = p ? planeetAanwijzer::decode(p) : geenCelId;
	wgpuBufferUnmap(_pick.buffer);
	_pick.klaar = false;
}

uint32_t Simulatie::doePickPass(uint32_t px, uint32_t py)
{
	uint32_t w = _scherm->oppervlakBreedte(), h = _scherm->oppervlakHoogte();
	if(w == 0 || h == 0)
		return geenCelId;

	//Web: er hangt nog een mapAsync in de lucht (de JS-eventloop moet eerst
	//draaien). Geef het vorige resultaat terug (cursor loopt 1 frame achter) en
	//probeer het volgende frame opnieuw. Native: hier hangt nooit een verzoek,
	//want onderaan wordt synchroon afgewacht.
	if(_pick.inVoortgang)
		return _pick.id;

	//Web: resultaat van de vorige aanvraag (map is inmiddels afgerond) binnenhalen.
	_rondPickAf();

	if(!_pickTextuur || _pickBreedte != w || _pickHoogte != h)
	{
		if(_pickTextuur) wgpuTextureRelease(_pickTextuur);
		WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT;
		td.usage         = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
		td.dimension     = WGPUTextureDimension_2D;
		td.size          = { w, h, 1u };
		td.format        = WGPUTextureFormat_RGBA8Unorm;
		td.mipLevelCount = 1;
		td.sampleCount   = 1;
		_pickTextuur = wgpuDeviceCreateTexture(weergaveScherm::deelApparaat(), &td);
		_pickBreedte = w; _pickHoogte = h;
	}

	weergaveInstellingen inst;
	inst.cullMode = WGPUCullMode_Back;
	_scherm->zetWeergaveInstellingen(inst);
	_scherm->zetWeergaveDoel(_pickTextuur, glm::uvec2(w, h));
	_scherm->zetWeergaveKleur(1.0f, 1.0f, 1.0f, 1.0f);

	_scherm->bereidRenderVoor("planeetgridPick");
	_geo->bindVrwrkrOpslagen(*_scherm);
	_geo->tekenJezelf();
	_scherm->pasRondRenderAf();
	_scherm->rondRenderAf();

	_scherm->zetWeergaveDoel(nullptr);
	_scherm->zetWeergaveInstellingen(weergaveInstellingen());
	_scherm->zetWeergaveKleur(0.0f, 0.0f, 0.0f, 1.0f);
	_scherm->ontkoppelRekenBuffers();

	//Kopieer 1 texel onder de cursor en decodeer de cel-ID. Native wacht hier
	//synchroon (alleen bij een actief penseel-gereedschap, zie stap()); web laat
	//de map tussen frames afhandelen en leest hem de volgende frame uit.
	WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(weergaveScherm::deelApparaat(), nullptr);
	WGPUTexelCopyTextureInfo bron = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	bron.texture  = _pickTextuur;
	bron.mipLevel = 0;
	bron.aspect   = WGPUTextureAspect_All;
	bron.origin   = { (uint32_t)px, (uint32_t)py, 0u };

	WGPUTexelCopyBufferInfo bestemming = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
	bestemming.buffer                = _pickLees;
	bestemming.layout.offset         = 0;
	bestemming.layout.bytesPerRow    = 256;
	bestemming.layout.rowsPerImage   = 1;

	WGPUExtent3D omvang = { 1u, 1u, 1u };
	wgpuCommandEncoderCopyTextureToBuffer(enc, &bron, &bestemming, &omvang);
	WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
	wgpuQueueSubmit(weergaveScherm::deelRij(), 1, &cmd);
	wgpuCommandBufferRelease(cmd);
	wgpuCommandEncoderRelease(enc);

	_pick.klaar = false;
	_pick.inVoortgang = true;
	WGPUBufferMapCallbackInfo info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	info.mode      = WGPUCallbackMode_AllowSpontaneous;
	info.callback  = pickVerwerker;
	info.userdata1 = &_pick;
	wgpuBufferMapAsync(_pickLees, WGPUMapMode_Read, 0, 256, info);

#ifndef __EMSCRIPTEN__
	//Native: de callback komt via ProcessEvents binnen; decode + unmap meteen.
	while(!_pick.klaar)
		wgpuInstanceProcessEvents(_scherm->instantie());
	_rondPickAf();
#endif

	//Web: dit is (nog) het resultaat van de vorige aanvraep — 1 frame latentie.
	return _pick.id;
}

void Simulatie::slaScreenshot(const std::string & pad)
{
	if(!_offScreen || !_shotLees) return;

	doeRenderPassen();

	const int shotBreedte = 960, shotHoogte = 960;
	const uint64_t shotBytes = (uint64_t)shotBreedte * shotHoogte * 4;

	WGPUTexelCopyTextureInfo bron = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	bron.texture = _offScreen;
	bron.mipLevel = 0;
	bron.aspect = WGPUTextureAspect_All;

	WGPUTexelCopyBufferInfo bestemming = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
	bestemming.buffer = _shotLees;
	bestemming.layout.offset = 0;
	bestemming.layout.bytesPerRow = (uint32_t)(shotBreedte * 4);
	bestemming.layout.rowsPerImage = (uint32_t)shotHoogte;

	WGPUExtent3D omvang = {(uint32_t)shotBreedte, (uint32_t)shotHoogte, 1u};

	WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(weergaveScherm::deelApparaat(), nullptr);
	wgpuCommandEncoderCopyTextureToBuffer(enc, &bron, &bestemming, &omvang);
	WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
	wgpuQueueSubmit(weergaveScherm::deelRij(), 1, &cmd);
	wgpuCommandBufferRelease(cmd);
	wgpuCommandEncoderRelease(enc);

	_shot.klaar = false;
	_shot.gelukt = false;
	WGPUBufferMapCallbackInfo info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	info.mode = WGPUCallbackMode_AllowSpontaneous;
	info.callback = shotVerwerker;
	info.userdata1 = &_shot;
	wgpuBufferMapAsync(_shotLees, WGPUMapMode_Read, 0, shotBytes, info);
	while(!_shot.klaar)
		wgpuInstanceProcessEvents(_scherm->instantie());

	const unsigned char * pixels = (const unsigned char *)wgpuBufferGetMappedRange(_shotLees, 0, shotBytes);
	if(!_shot.gelukt || !pixels)
	{
		std::cerr << "schermafbeelding: map mislukt (" << (int)_shot.gelukt << ", pixels " << (pixels ? "aanwezig" : "null") << ")" << std::endl;
		if(pixels) wgpuBufferUnmap(_shotLees);
		return;
	}
	std::vector<unsigned char> kopie(pixels, pixels + shotBytes);
	wgpuBufferUnmap(_shotLees);

	std::cout << "schermafbeelding -> " << pad << std::endl;
	bewaarPNG(pad, shotBreedte, shotHoogte, kopie);
}

// ── Readback-helpers (--diagnose/--diagnoseCsv/--conservering/--veldKaart) ───

void Simulatie::drainVakken(WGPUBufferMapCallback callback, void * gebruiker, bool * klaar)
{
	WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(weergaveScherm::deelApparaat(), nullptr);
	wgpuCommandEncoderCopyBufferToBuffer(enc, _geo->huidigeOpslag(), 0, _diagLees, 0, _diagGrootte);
	WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
	wgpuQueueSubmit(weergaveScherm::deelRij(), 1, &cmd);
	wgpuCommandBufferRelease(cmd);
	wgpuCommandEncoderRelease(enc);

	*klaar = false;
	WGPUBufferMapCallbackInfo info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	info.mode       = WGPUCallbackMode_AllowSpontaneous;
	info.callback   = callback;
	info.userdata1  = gebruiker;
	info.userdata2  = nullptr;
	wgpuBufferMapAsync(_diagLees, WGPUMapMode_Read, 0, _diagGrootte, info);
	while(!*klaar)
		wgpuInstanceProcessEvents(_scherm->instantie());
}

void Simulatie::schrijfVeldKaarten()
{
	if(_cfg.veldKaarten.empty() || !_diagLees) return;

	veldKaartToestandje toestand;
	toestand.buffer  = _diagLees;
	toestand.grootte = _diagGrootte;
	toestand.geo     = _geo;
	toestand.kaarten = _cfg.veldKaarten;
	toestand.factor  = _cfg.kaartFactor;

	drainVakken(veldKaartVerwerker, &toestand, &toestand.klaar);
}

bool Simulatie::conservatieLek() const
{
	if(!_cfg.conservatieAan || _totaalWaterStart <= 0.0)
		return false;

	double afwijking = std::max(std::abs((_totaalWaterMax - _totaalWaterStart) / _totaalWaterStart),
	                            std::abs((_totaalWaterMin - _totaalWaterStart) / _totaalWaterStart));
	std::cerr << "[conservering] EVALUATIE: start=" << _totaalWaterStart
	          << " min=" << _totaalWaterMin << " max=" << _totaalWaterMax
	          << "  max-afwijking=" << (100.0 * afwijking) << "%  (tolerantie "
	          << (100.0 * _cfg.conservatieTol) << "%)  => "
	          << (afwijking <= _cfg.conservatieTol ? "GEVANGEN: constant" : "LEK: niet constant")
	          << std::endl;
	return afwijking > _cfg.conservatieTol;
}
