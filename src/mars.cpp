#include "weergaveSchermPerspectief.h"
#include "planeet.h"
#include "monsterPNG.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>

//De parameters voor de reken-shaders (bind-groep 0, binding 3). Layout moet matchen
//met het rekenParameters-struct in shaders/planeetStructen.wgsl
struct rekenParameters
{
	float	grondSchaal,
			verdamping,
			erosie,
			levenAan;
	float	windAs[4];      //rotatie-As van de circulatiecellen (xyz) + windsterkte (w)
	float	zonRicht[4];    //zonrichting voor dag/nacht-verdamping
	float	condenseer[4];  //basisVerzadiging, hoogteKoel, neerslagFactor, orografieFactor
};

//Diagnose (--diag): leest elke zoveel frames de laatste reken-stand terug van de
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
			maxDroesem = 0, maxPijp = 0, maxSnelheid = 0, maxGrond = 0;
	size_t	piekWater = 0, piekDroesem = 0, piekPijp = 0, piekSnelheid = 0;
	bool	nietEindig = false;

	for(size_t i = 0; i < aantal; i++)
	{
		const vak & cel = cellen[i];
		const float snelhe = sqrtf(cel.snelheid.x * cel.snelheid.x + cel.snelheid.y * cel.snelheid.y);

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

		const bool eindig =
				cel.grondHoogte >= -1.0e30f && cel.grondHoogte <=  1.0e30f &&
				cel.rotsHoogte  >= -1.0e30f && cel.rotsHoogte  <=  1.0e30f &&
				cel.waterHoogte >= -1.0e30f && cel.waterHoogte <=  1.0e30f &&
				cel.waterSchijn >= -1.0e30f && cel.waterSchijn <=  1.0e30f &&
				cel.bodemVocht  >= -1.0e30f && cel.bodemVocht  <=  1.0e30f &&
				cel.luchtVocht  >= -1.0e30f && cel.luchtVocht  <=  1.0e30f &&
				cel.droesem     >= -1.0e30f && cel.droesem     <=  1.0e30f;

		if(!nietEindig && !eindig)
		{
			glm::vec3 richting = t->geo->punt3(i);
			std::cerr << "[diag " << t->teller << " !!] cel " << i
					  << " richting (" << richting.x << ", " << richting.y << ", " << richting.z << ")"
					  << " NIET-eindig: grond=" << cel.grondHoogte << " rots=" << cel.rotsHoogte
					  << " water=" << cel.waterHoogte << " schijn=" << cel.waterSchijn
					  << " bodem=" << cel.bodemVocht << " lucht=" << cel.luchtVocht
					  << " droesem=" << cel.droesem << std::endl;
			nietEindig = true;
		}
	}

	std::cerr << "[diag " << t->teller << "]  water=" << maxWaterHoogte
			  << " (cel " << piekWater << ")  schijn=" << maxSchijn
			  << "  bodemvocht=" << maxBodemVocht << "  luchtvocht=" << maxLuchtVocht
			  << "  droesem=" << maxDroesem
			  << " (cel " << piekDroesem << ")  pijp=" << maxPijp
			  << " (cel " << piekPijp << ")  snelheid=" << maxSnelheid
			  << " (cel " << piekSnelheid << ")  grond=" << maxGrond << std::endl;

	wgpuBufferUnmap(t->buffer);
}

int main(int argc, char ** argv)
{
	//Testvlaggen: --no-water begint zonder water (waterHoogte = 0),
	//--no-erosion houdt het terrein stil (geen erosie/depositie),
	//--no-life schakelt plantengroei uit,
	//--subdiv <n> zet het icosahedron-onderverdelingsniveau (standaard 8),
	//--diag print elke zoveel frames de extremen van de reken-stand terug.
	bool beginMetWater = true;
	bool erosieAan = true;
	bool levenAan = true;
	bool diagAan = false;
	int  subdiv = 8;

	for(int a = 1; a < argc; a++)
	{
		std::string vlag = argv[a];
		if(vlag == "--no-water")       beginMetWater = false;
		else if(vlag == "--no-erosion") erosieAan = false;
		else if(vlag == "--no-life")    levenAan = false;
		else if(vlag == "--diag")       diagAan = true;
		else if(vlag == "--subdiv")
		{
			if(a + 1 < argc)
			{
				subdiv = std::max(1, std::atoi(argv[++a]));
			}
			else
				std::cerr << "--subdiv verwacht een getal (bijv. --subdiv 6)" << std::endl;
		}
		else std::cerr << "Onbekende vlag: " << vlag << std::endl;
	}

	weergaveSchermPerspectief scherm("Planeet", 1280, 720, 8);

	scherm.maakShader(		"planeetgridLand", 	"shaders/planeetgridVertLand.wgsl", 	"shaders/planeetgridFragLand.wgsl"	);
	scherm.maakShader(		"planeetgridWater", "shaders/planeetgridVertWater.wgsl",	"shaders/planeetgridFragWater.wgsl"	);

	scherm.maakRekenShader(	"waterDruk", 		"shaders/waterDruk.comp"											);
	scherm.maakRekenShader(	"waterStroming", 	"shaders/waterStroming.comp"										);
	scherm.maakRekenShader(	"waterGemiddelde", 	"shaders/waterGemiddelde.comp"										);
	scherm.maakRekenShader(	"waterLucht", 		"shaders/waterLucht.comp"											);

	scherm.zetWeergaveKleur(0, 0, 0, 1);

	//Hoogtekaart van Mars laden; de ruwe pixels hebben we ook nodig voor monsterPNG
	size_t 	w, h, kanalen;
	png_byte * MarsHoogte = laadPNG("MARS_Hoogte.png", w, h, kanalen);

	if(!MarsHoogte)
		throw std::runtime_error("Kon MARS_Hoogte.png niet laden!");

	glm::uvec2 MarsHoogteBH(w, h);

	monsterPNG MOLA(MarsHoogte, MarsHoogteBH);

	//voor de grond-pass: de hoogtekaart (naadloos tiling boeit niet, we fract-en de coordinaat toch)
	scherm.maakTextuur("marsHoogteTex", w, h, true, false, false, GL_RGBA8, MarsHoogte, GL_RGBA, GL_UNSIGNED_BYTE);

	//voor de water-pass: een bump-kaart voor de golfjes
	size_t 		wb, hb, kanalenB;
	png_byte * 	bumpData = laadPNG("plaatjes/zeewater_bump.png", wb, hb, kanalenB);

	if(!bumpData)
		throw std::runtime_error("Kon zeewater_bump.png niet laden!");

	scherm.maakTextuur("waterBumpTex", wb, hb, true, true, false, GL_RGBA8, bumpData, GL_RGBA, GL_UNSIGNED_BYTE);

	delete[] bumpData;
	delete[] MarsHoogte;

	float		grondMult	= 100.0;
	planeet	*	geo			= nullptr;

	std::function<float(glm::vec2)> hoogteMonsteraar = [&](glm::vec2 plek) -> float { return grondMult + 10 * MOLA(plek).x; };

	geo = new planeet(subdiv, hoogteMonsteraar, beginMetWater);

	bool 		roteerMaar		= false,
				waterStroomt	= true,
				waterStap		= false,
				zonDraait		= false,
				tekenWater		= true;

	glm::vec3	kijkPlek		(0.0f)				,
				zonPos			(0.0f)				;
	float		grondSchaal		= 1.0,
				verdamping		= 0.01f;
	float		windSterkte		= 0.4f;
	float		basisVerzadiging= 0.25f,
				hoogteKoel		= 0.4f,
				neerslagFactor	= 0.3f,
				orografieFactor	= 0.4f;

	weergaveScherm::toetsVerwerkerFunc toetsenbord = [&](int key, int scancode, int action, int mods)
	{
		if(action == GLFW_PRESS)
			switch(key)
			{
			case GLFW_KEY_SPACE:		waterStroomt 	= !waterStroomt;	break;
			case GLFW_KEY_R:			roteerMaar 		= !roteerMaar;		break;
			case GLFW_KEY_Z:			zonDraait 		= !zonDraait;		break;
			case GLFW_KEY_X:			tekenWater 		= !tekenWater;		break;
			case GLFW_KEY_ENTER:		waterStap 		= true;				break;
			case GLFW_KEY_SEMICOLON:	grondMult 		= glm::max(1.0f, grondMult * 0.9f);	break;
			case GLFW_KEY_APOSTROPHE:	grondMult 		= glm::max(1.0f, grondMult * 1.1f);	break;
			case GLFW_KEY_K:			verdamping 		= glm::max(0.0f, verdamping - 0.001f);	break;
			case GLFW_KEY_L:			verdamping 		= glm::max(0.0f, verdamping + 0.001f);	break;
			case GLFW_KEY_LEFT_BRACKET:	windSterkte 	= glm::max(0.0f, windSterkte - 0.05f);	break;
			case GLFW_KEY_RIGHT_BRACKET:windSterkte 	= glm::min(1.0f, windSterkte + 0.05f);	break;
			case GLFW_KEY_PERIOD:		neerslagFactor 	= glm::max(0.0f, neerslagFactor - 0.1f);	break;
			case GLFW_KEY_SLASH:		neerslagFactor 	= glm::max(0.0f, neerslagFactor + 0.1f);	break;
			}
	};

	scherm.setCustomKeyhandler(toetsenbord);

	glm::vec2 zonRot = glm::vec2(-0.6f, 0.3);
	float	windHoek = 0.0f;
	glm::vec3 windAsV = glm::vec3(0.0f, 1.0f, 0.0f);

	//Opslag-buffer met de parameters voor de reken-shaders (bind-groep 0, binding 3)
	WGPUDevice apparaat = weergaveScherm::deelApparaat();
	WGPUQueue  rij 		= weergaveScherm::deelRij();

	WGPUBufferDescriptor beschrijving = WGPU_BUFFER_DESCRIPTOR_INIT;
	beschrijving.usage 	= WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
	beschrijving.size 	= sizeof(rekenParameters);
	WGPUBuffer rekenParBuffer = wgpuDeviceCreateBuffer(apparaat, &beschrijving);

	//Diagnose-buffer (--diag): een aparte leesbare kopie om de stand terug te lezen
	WGPUBuffer diagLees = nullptr;
	const size_t diagGrootte = geo->aantalVakjes() * sizeof(vak);
	const size_t diagElkeFrames = 25;

	if(diagAan)
	{
		WGPUBufferDescriptor leesBeschrijving = WGPU_BUFFER_DESCRIPTOR_INIT;
		leesBeschrijving.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
		leesBeschrijving.size  = diagGrootte;
		diagLees = wgpuDeviceCreateBuffer(apparaat, &leesBeschrijving);
	}

	auto berekenShaderBinden = [&]()
	{
		geo->bindVrwrkrOpslagen(scherm);
		scherm.verbindRekenBuffer(3, rekenParBuffer);
	};

	//de weergave-parameters (bind-groep 0, binding 2): layout volgens extraParameters in de WGSL
	float extra[16] = { 0.0f };

	rekenParameters rekenPar = {};

	const uint32_t rekenGroepen = (uint32_t)((geo->aantalVakjes() + 63) / 64);

	size_t frameNummer = 0;

	while(!scherm.stopGewenst())
	{
		//Is de planeet buiten beeld (venster geOccludeerd)? Stop dan de loop:
		//geen render, geen simulatie, geen spin. glfwWaitEvents wekt hem weer op
		//zodra het venster weer zichtbaar wordt (of bij andere venster-gebeurtenissen).
		if(!scherm.oppervlakZichtbaar())
		{
			scherm.wachtOpGebeurtenissen();
			continue;
		}

		kijkPlek = glm::vec3(glm::inverse(scherm.modelZicht())[3]);

		//dag/nacht loopt langzaam over de planeet (Z zet sneller draaien aan)
		zonRot.x += zonDraait ? 0.02f : 0.003f;

		glm::mat4 zonRoteerder =
			glm::rotate(
				glm::rotate(
					glm::mat4(1.0f),
					zonRot.x,
					glm::vec3(0.0f, 1.0f, 0.0f)
				),
				zonRot.y,
				glm::vec3(1.0f, 0.0f, 0.0f)
			);

		glm::vec4 zonPosTdlk = (zonDraait ? scherm.modelZicht() : glm::mat4(1.0f)) * zonRoteerder * glm::normalize(glm::vec4(0.0, sin(zonPos.x / 365.0f), 7.0, 1.0));
		zonPos = zonPosTdlk.xyz() / zonPosTdlk.w;

		//De rotatie-as van de circulatiecellen drijft langzaam over het oppervlak:
		//twee rondjes om verschillende assen => geen permanente windwaartse bergen.
		windHoek += 0.004f;
		{
			glm::vec4 p1 = glm::rotate(glm::mat4(1.0f), windHoek, glm::vec3(0.35f, 0.7f, 0.6f)) * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			glm::vec4 p2 = glm::rotate(glm::mat4(1.0f), windHoek * 0.53f, glm::vec3(0.85f, 0.1f, 0.4f)) * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			windAsV = glm::normalize(glm::vec3(p1) + 0.6f * glm::vec3(p2));
		}

		if(roteerMaar)
			scherm.zetModelZicht(glm::rotate(scherm.modelZicht(), 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)));

		//weergave-parameters in de extra-buffer schrijven
		extra[0] 	= grondMult;
		extra[1] 	= grondSchaal;
		extra[4] 	= kijkPlek.x;	extra[5] = kijkPlek.y;	extra[6] = kijkPlek.z;
		extra[8] 	= zonPos.x;		extra[9] = zonPos.y;	extra[10] = zonPos.z;

		scherm.zetExtraFloats(extra, 16);

		//parameters voor de reken-shaders
		rekenPar.grondSchaal 	= grondSchaal;
		rekenPar.verdamping 	= verdamping;
		rekenPar.erosie 		= erosieAan ? 1.0f : 0.0f;
		rekenPar.levenAan 		= levenAan ? 1.0f : 0.0f;
		rekenPar.windAs[0] 		= windAsV.x;	rekenPar.windAs[1] = windAsV.y;	rekenPar.windAs[2] = windAsV.z;	rekenPar.windAs[3] = windSterkte;
		rekenPar.zonRicht[0] 	= zonPos.x;		rekenPar.zonRicht[1] = zonPos.y;	rekenPar.zonRicht[2] = zonPos.z;	rekenPar.zonRicht[3] = 0.0f;
		rekenPar.condenseer[0] 	= basisVerzadiging;
		rekenPar.condenseer[1] 	= hoogteKoel;
		rekenPar.condenseer[2] 	= neerslagFactor;
		rekenPar.condenseer[3] 	= orografieFactor;

		wgpuQueueWriteBuffer(rij, rekenParBuffer, 0, &rekenPar, sizeof(rekenParameters));

		//grond-pass (achterkant-verwijdering aan)
		weergaveInstellingen grondInstellingen;
		grondInstellingen.cullMode = WGPUCullMode_Back;
		scherm.zetWeergaveInstellingen(grondInstellingen);

		scherm.bereidRenderVoor("planeetgridLand");
		geo->bindVrwrkrOpslagen(scherm);
		scherm.bindTextuur("marsHoogteTex", 0);
		geo->tekenJezelf();
		scherm.pasRondRenderAf();

		//water-pass: een blendende tweede laag over de grond heen, op hetzelfde oppervlak.
		//Het water doet wél een diepte-test (alleen waar het vóór de grond ligt) maar schrijft
		//géén diepte (blenden en diepte-schrijven samen geven anders doorlopende donkere vlakken).
		if(tekenWater)
		{
			weergaveInstellingen waterInstellingen;
			waterInstellingen.blenden 			= true;
			waterInstellingen.cullMode 			= WGPUCullMode_Back;
			waterInstellingen.diepteSchrijven 	= false;
			waterInstellingen.diepteVergelijk 	= WGPUCompareFunction_LessEqual; //ook water exact op de grondhoogte
			scherm.zetWeergaveInstellingen(waterInstellingen);

			scherm.bereidRenderVoor("planeetgridWater", false);
			geo->bindVrwrkrOpslagen(scherm);
			scherm.bindTextuur("waterBumpTex", 0);
			geo->tekenJezelf();

			scherm.rondRenderAf();
			scherm.zetWeergaveInstellingen(weergaveInstellingen());
		}
		else
			scherm.rondRenderAf();

		scherm.ontkoppelRekenBuffers();

		if(waterStroomt || waterStap)
		{
			scherm.doeRekenVerwerker("waterStroming", 		glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			scherm.doeRekenVerwerker("waterDruk", 			glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			scherm.doeRekenVerwerker("waterGemiddelde", 	glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			scherm.doeRekenVerwerker("waterLucht", 			glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			waterStap = false;

			geo->volgendeRonde();
		}

		if(diagAan && diagLees && frameNummer % diagElkeFrames == 0)
		{
			//De stand (exact zoals de volgende render laat zien) teruglezen naar CPU
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

		frameNummer++;

		wgpFoutControle("Frame: ");
	}

	if(diagLees)
		wgpuBufferRelease(diagLees);
	wgpuBufferRelease(rekenParBuffer);

	delete geo;
}