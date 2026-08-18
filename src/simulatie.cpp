#include "simulatie.h"
#include "helpers.h"
#include <cmath>
#include <chrono>
#include <iostream>
#include <png.h>

// ── Callbacks (namespace scope) ─────────────────────────────────────────────

static void diagVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void *)
{
	diagToestandje * t = (diagToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success) { std::cerr << "[diag] lezen mislukt\n"; return; }

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

	if(status != WGPUMapAsyncStatus_Success) { std::cerr << "[csv] lezen mislukt\n"; return; }

	const vak * cellen = (const vak *)wgpuBufferGetMappedRange(t->buffer, 0, t->grootte);
	const size_t aantal = t->grootte / sizeof(vak);
	std::ofstream & uit = *t->uit;
	if(t->teller == 0)
		uit << "id,x,y,z,grond,rots,zand,water,bodemVocht,leven,droesem,luchtVocht,"
		       "temperatuur,luchtdruk,windX,windY,wolken,ijs,zonZicht,asym\n";

	for(size_t i = 0; i < aantal; i++)
	{
		const vak & cel = cellen[i];
		glm::vec3 p = t->geo->punt3(i);
		uit << i << "," << p.x << "," << p.y << "," << p.z << ","
			<< (cel.rotsHoogte + cel.zandHoogte) << "," << cel.rotsHoogte << "," << cel.zandHoogte << "," << cel.waterHoogte << ","
			<< cel.bodemVocht << "," << cel.leven << "," << cel.droesem << ","
			<< cel.luchtVocht << "," << cel.temperatuur << "," << cel.luchtdruk << ","
			<< cel.wind.x << "," << cel.wind.y << "," << cel.wolken << ","
			<< cel.ijs << "," << cel.zonZicht << "," << t->geo->buurAsymmetrie(i) << "\n";
	}

	wgpuBufferUnmap(t->buffer);
}

static void conservatieVerwerker(WGPUMapAsyncStatus status, WGPUStringView, void * gebruiker1, void *)
{
	conservatieToestandje * t = (conservatieToestandje *)gebruiker1;
	t->klaar = true;

	if(status != WGPUMapAsyncStatus_Success) { std::cerr << "[conservering] lezen mislukt\n"; return; }

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
	(void)status; (void)gebruiker2;
	((shotToestandje *)gebruiker1)->klaar = true;
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

Simulatie::Simulatie(SimulatieConfig cfg) : _cfg(std::move(cfg)) {}

Simulatie::~Simulatie()
{
	if(_csvUit.is_open()) _csvUit.close();
	if(_diagLees) wgpuBufferRelease(_diagLees);
	if(_shotLees) wgpuBufferRelease(_shotLees);
	if(_offScreen) wgpuTextureRelease(_offScreen);
	if(_rekenParBuffer) wgpuBufferRelease(_rekenParBuffer);
	delete _geo;
	delete _scherm;
}

bool Simulatie::init()
{
	const bool heeftRender = !_cfg.hoofdloos || !_cfg.schermafbeeldingBestand.empty();

	// ── Scherm ──────────────────────────────────────────────────────────
	_scherm = new weergaveSchermPerspectief("Planeet", 1280, 720, 8, _cfg.hoofdloos);

	// ── Shaders ─────────────────────────────────────────────────────────
	if(heeftRender)
	{
		_scherm->maakShader("planeetgridLand",  "shaders/planeetgridVertLand.wgsl",   "shaders/planeetgridFragLand.wgsl");
		_scherm->maakShader("planeetgridWater", "shaders/planeetgridVertWater.wgsl",  "shaders/planeetgridFragWater.wgsl");
		_scherm->maakShader("planeetgridWolk",  "shaders/planeetgridVertWolk.wgsl",   "shaders/planeetgridFragWolk.wgsl");
	}

	_scherm->maakRekenShader("waterStroming",  "shaders/waterStroming.comp");
	_scherm->maakRekenShader("waterDruk",      "shaders/waterDruk.comp");
	_scherm->maakRekenShader("waterGemiddelde", "shaders/waterGemiddelde.comp");
	_scherm->maakRekenShader("luchtStroming",  "shaders/luchtStroming.comp");
	_scherm->maakRekenShader("vochtStroming",  "shaders/vochtStroming.comp");
	_scherm->maakRekenShader("waterLucht",     "shaders/waterLucht.comp");

	_scherm->maakDiepteShader("planeetSchaduw", "shaders/planeetgridVertSchaduw.wgsl");
	_scherm->maakTextuur("zonSchaduwKaart", (size_t)_cfg.schaduwGrootte, (size_t)_cfg.schaduwGrootte, false, false, false, GL_DEPTH_COMPONENT32F, nullptr);
	_scherm->bindSchaduwKaart("zonSchaduwKaart");

	_scherm->zetWeergaveKleur(0, 0, 0, 1);

	// ── Texturen ────────────────────────────────────────────────────────
	if(!_cfg.procedural)
	{
		size_t w, h, kanalen;
		png_byte * MarsHoogte = laadPNG("MARS_Hoogte.png", w, h, kanalen);
		if(!MarsHoogte)
		{
			std::cerr << "Kon MARS_Hoogte.png niet laden! Gebruik --procedureel." << std::endl;
			return false;
		}

		// Spiegel links-rechts (RGBA, 4 kanalen per pixel)
		for(size_t y = 0; y < h; y++)
			for(size_t x = 0; x < w / 2; x++)
			{
				size_t links    = (x + y * w) * 4;
				size_t rechts   = ((w - 1 - x) + y * w) * 4;
				std::swap(MarsHoogte[links],     MarsHoogte[rechts]);
				std::swap(MarsHoogte[links + 1], MarsHoogte[rechts + 1]);
				std::swap(MarsHoogte[links + 2], MarsHoogte[rechts + 2]);
				std::swap(MarsHoogte[links + 3], MarsHoogte[rechts + 3]);
			}

		if(heeftRender)
			_scherm->maakTextuur("marsHoogteTex", w, h, true, false, false, GL_RGBA8, MarsHoogte, GL_RGBA, GL_UNSIGNED_BYTE);

		// Bewaar grijswaarden voor de altura-lambda (rode kanaal / 255.0)
		_molaBreedte = w;
		_molaHoogte  = h;
		_molaData.resize(w * h);
		for(size_t y = 0; y < h; y++)
			for(size_t x = 0; x < w; x++)
				_molaData[y * w + x] = (float)MarsHoogte[(y * w + x) * 4] / 255.0f;

		delete[] MarsHoogte;
	}

	// Bump-kaart voor water
	size_t bumpW, bumpH, bumpK;
	png_byte * bumpData = laadPNG("plaatjes/zeewater_bump.png", bumpW, bumpH, bumpK);
	if(!bumpData)
	{
		std::cerr << "Kon zeewater_bump.png niet laden!" << std::endl;
		return false;
	}
	if(heeftRender)
		_scherm->maakTextuur("waterBumpTex", bumpW, bumpH, true, true, false, GL_RGBA8, bumpData, GL_RGBA, GL_UNSIGNED_BYTE);
	delete[] bumpData;

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
	if(_cfg.procedural)
	{
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
		auto altura = [&, this](glm::vec3 pos) -> float
		{
			float n = fbm(pos * 2.2f) * 2.0f - 1.0f;
			float versterking = 2.0f + std::abs(n);
			float h = 60.0f + 10.0f * n * versterking;
			return glm::clamp(h, 40.0f, 140.0f);
		};
		_geo = new planeet(_cfg.subdiv, altura, _cfg.beginMetWater);
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
		_geo = new planeet(_cfg.subdiv, molaHoogte, _cfg.beginMetWater);
	}

	// ── Toetsenhandler ──────────────────────────────────────────────────
	if(!_cfg.hoofdloos)
	{
		weergaveScherm::toetsVerwerkerFunc toetsenbord = [this](int key, int scancode, int action, int mods)
		{
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
					std::cout << "Zon " << (_zonRoteert ? "voort" : "stil") << "." << std::endl;
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
				case GLFW_KEY_1: case GLFW_KEY_T:
					_overlayKeuze = 0;
					std::cout << "Normale weergave." << std::endl;
					break;
				case GLFW_KEY_2:
					_overlayKeuze = 1;
					std::cout << "Temperatuuroverlay." << std::endl;
					break;
				case GLFW_KEY_3: case GLFW_KEY_V:
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
					std::cout << "ZonZicht-overlay." << std::endl;
					break;
				case GLFW_KEY_9:
					_overlayKeuze = 8;
					std::cout << "Levens-overlay." << std::endl;
					break;
				case GLFW_KEY_0:
					_overlayKeuze = 9;
					std::cout << "Terreinhoogte-overlay." << std::endl;
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
				case GLFW_KEY_LEFT_BRACKET:
					if(mods & GLFW_MOD_SHIFT)
					{
						_winterZonneKracht = glm::max(0.0f, _winterZonneKracht - 5.0f);
						std::cout << "Winterzonkracht = " << _winterZonneKracht << "." << std::endl;
					}
					else
					{
						_rotatieOmega = glm::max(0.0f, _rotatieOmega - 0.002f);
						std::cout << "RotatieOmega = " << _rotatieOmega << "." << std::endl;
					}
					break;
				case GLFW_KEY_RIGHT_BRACKET:
					if(mods & GLFW_MOD_SHIFT)
					{
						_winterZonneKracht = glm::min(_zonKracht, _winterZonneKracht + 5.0f);
						std::cout << "Winterzonkracht = " << _winterZonneKracht << "." << std::endl;
					}
					else
					{
						_rotatieOmega = glm::min(0.2f, _rotatieOmega + 0.002f);
						std::cout << "RotatieOmega = " << _rotatieOmega << "." << std::endl;
					}
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
	}

	return true;
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

	// ── Zonrotatie ──────────────────────────────────────────────────────
	if(!_cfg.bevroren && _zonRoteert)
	{
		_dagHoek += _rotatieOmega;
		_seizoenTeller += _rotatieOmega;
	}
	_dagHoek = glm::mod(_dagHoek, 6.28318530718f);

	glm::mat4 zonRoteerder =
		glm::rotate(glm::rotate(glm::mat4(1.0f), _dagHoek, glm::vec3(0.0f, 1.0f, 0.0f)),
		             _obliquity, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::vec4 zonRicht4 = zonRoteerder * glm::normalize(glm::vec4(0.0f, 0.2f, 1.0f, 0.0f));
	_zonPos = glm::normalize(glm::vec3(zonRicht4));

	if(!_cfg.bevroren && _roteerMaar)
		_scherm->zetModelZicht(glm::rotate(_scherm->modelZicht(), 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)));

	// ── Uniformen ───────────────────────────────────────────────────────
	_kijkPlek = glm::vec3(glm::inverse(_scherm->modelZicht())[3]);
	_extra[0]  = _grondMult;
	_extra[1]  = _grondSchaal;
	_extra[2]  = (float)_cfg.schaduwGrootte;
	_extra[4]  = _kijkPlek.x; _extra[5] = _kijkPlek.y; _extra[6] = _kijkPlek.z;
	_extra[8]  = _zonPos.x;   _extra[9] = _zonPos.y;   _extra[10] = _zonPos.z;
	_extra[12] = _geo->hoogsteGrond();
	_extra[13] = (float)_overlayKeuze;
	_extra[14] = 0.0f;
	_extra[15] = _cfg.schaduwAan ? 1.0f : 0.0f;
	_scherm->zetExtraFloats(_extra, 16);

	rekenParameters rekenPar = {};
	rekenPar.grondSchaal  = _grondSchaal;
	rekenPar.verdamping   = _verdamping;
	rekenPar.erosie       = _cfg.erosieAan ? 1.0f : 0.0f;
	rekenPar.levenAan     = _cfg.levenAan ? 1.0f : 0.0f;

	float seizoenPos = glm::mod(_seizoenTeller, 25.1327412f) / 12.5663706f;
	float winterVerhouding = _zonKracht > 0.001f ? _winterZonneKracht / _zonKracht : 0.0f;
	float seizoenFactor = 1.0f;
	if(seizoenPos < 0.25f)
		seizoenFactor = 1.0f;
	else if(seizoenPos < 0.5f)
		seizoenFactor = winterVerhouding + (1.0f - winterVerhouding) * (1.0f - (seizoenPos - 0.25f) / 0.25f);
	else if(seizoenPos < 0.75f)
		seizoenFactor = winterVerhouding;
	else
		seizoenFactor = winterVerhouding + (1.0f - winterVerhouding) * (seizoenPos - 0.75f) / 0.25f;

	rekenPar.atmosfeer[0] = _cfg.atmosfeerAan ? _zonKracht * seizoenFactor : 0.0f;
	rekenPar.atmosfeer[1] = _coriolisOmega;
	rekenPar.atmosfeer[2] = _cfg.atmosfeerAan ? _wrijving : 0.0f;
	rekenPar.atmosfeer[3] = _cfg.atmosfeerAan ? _diffusie : 0.0f;
	rekenPar.zonRicht[0]  = _zonPos.x; rekenPar.zonRicht[1] = _zonPos.y;
	rekenPar.zonRicht[2]  = _zonPos.z; rekenPar.zonRicht[3] = 0.0f;
	rekenPar.condenseer[0] = _basisVerzadiging;
	rekenPar.condenseer[1] = _hoogteKoel;
	rekenPar.condenseer[2] = _neerslagFactor;
	rekenPar.condenseer[3] = _orografieFactor;
	rekenPar.fasen[0]      = _verwarmtijd;
	rekenPar.fasen[1]      = _geo->hoogsteGrond();
	rekenPar.fasen[2]      = _grondMult;
	rekenPar.fasen[3]      = std::pow(4.0f, (float)(_cfg.subdiv - 6));
	rekenPar.schaduw[0]    = _cfg.schaduwAan ? 1.0f : 0.0f;
	rekenPar.schaduw[1]    = (float)_cfg.schaduwGrootte;

	wgpuQueueWriteBuffer(weergaveScherm::deelRij(), _rekenParBuffer, 0, &rekenPar, sizeof(rekenParameters));

	// ── Schaduwpass ─────────────────────────────────────────────────────
	doeSchaduwPass();

	// ── Renderpasses ────────────────────────────────────────────────────
	if(!_cfg.hoofdloos)
		doeRenderPassen();

	// ── Rekenketen ──────────────────────────────────────────────────────
	if(!_cfg.bevroren && (_waterStroomt || _waterStap || _cfg.hoofdloos))
	{
		const uint32_t rekenGroepen = (uint32_t)((_geo->aantalVakjes() + 63) / 64);

		auto berekenShaderBinden = [this]()
		{
			_geo->bindVrwrkrOpslagen(*_scherm);
			_scherm->verbindRekenBuffer(3, _rekenParBuffer);
		};

		auto berekenShaderBindenMetSchaduw = [this, &berekenShaderBinden]()
		{
			berekenShaderBinden();
			_scherm->bindTextuur(_cfg.schaduwAan ? "zonSchaduwKaart" : "", 0);
		};

		for(int substap = 0; substap < _cfg.luchtStappen; substap++)
		{
			_scherm->doeRekenVerwerker("waterStroming",  glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("waterDruk",      glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("waterGemiddelde", glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("luchtStroming",  glm::uvec3(rekenGroepen, 1, 1), berekenShaderBindenMetSchaduw);
			_scherm->bindTextuur("", 0);
			_scherm->doeRekenVerwerker("vochtStroming",  glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_scherm->doeRekenVerwerker("waterLucht",     glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			_geo->volgendeRonde();
		}
		_waterStap = false;
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

	// Grond-pass
	weergaveInstellingen grondInstellingen;
	grondInstellingen.cullMode = WGPUCullMode_Back;
	_scherm->zetWeergaveInstellingen(grondInstellingen);

	_scherm->bereidRenderVoor("planeetgridLand");
	_geo->bindVrwrkrOpslagen(*_scherm);
	if(!_cfg.procedural)
		_scherm->bindTextuur("marsHoogteTex", 0);
	_geo->tekenJezelf();
	_scherm->pasRondRenderAf();

	// Water-pass
	if(_tekenWater && _overlayKeuze == 0)
	{
		weergaveInstellingen waterInstellingen;
		waterInstellingen.blenden = true;
		waterInstellingen.cullMode = WGPUCullMode_Back;
		waterInstellingen.diepteSchrijven = false;
		waterInstellingen.diepteVergelijk = WGPUCompareFunction_LessEqual;
		_scherm->zetWeergaveInstellingen(waterInstellingen);

		_scherm->bereidRenderVoor("planeetgridWater", false);
		_geo->bindVrwrkrOpslagen(*_scherm);
		_scherm->bindTextuur("waterBumpTex", 0);
		_geo->tekenJezelf();
		_scherm->pasRondRenderAf();
	}

	// Wolken-pass
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

	_scherm->rondRenderAf();
	_scherm->zetWeergaveInstellingen(weergaveInstellingen());
	_scherm->ontkoppelRekenBuffers();
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
	WGPUBufferMapCallbackInfo info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	info.mode = WGPUCallbackMode_AllowSpontaneous;
	info.callback = shotVerwerker;
	info.userdata1 = &_shot;
	wgpuBufferMapAsync(_shotLees, WGPUMapMode_Read, 0, shotBytes, info);
	while(!_shot.klaar)
		wgpuInstanceProcessEvents(_scherm->instantie());

	const unsigned char * pixels = (const unsigned char *)wgpuBufferGetMappedRange(_shotLees, 0, shotBytes);
	std::vector<unsigned char> kopie(pixels, pixels + shotBytes);
	wgpuBufferUnmap(_shotLees);

	std::cout << "schermafbeelding -> " << pad << std::endl;
	bewaarPNG(pad, shotBreedte, shotHoogte, kopie);
}
