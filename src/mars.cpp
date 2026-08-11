#include "weergaveSchermPerspectief.h"
#include "planeet.h"
#include "monsterPNG.h"
#include <iostream>
#include <random>

//De parameters voor de reken-shaders (bind-groep 0, binding 3). Layout moet matchen
//met het rekenParameters-struct in shaders/planeetStructen.wgsl
struct rekenParameters
{
	float	grondSchaal,
			verdamping,
			_padA,
			_padB;
	float	regenPlek[4];
};

int main()
{
	weergaveSchermPerspectief scherm("Planeet", 1280, 720, 8);

	scherm.maakShader(		"planeetgridLand", 	"shaders/planeetgridVertLand.wgsl", 	"shaders/planeetgridFragLand.wgsl"	);
	scherm.maakShader(		"planeetgridWater", "shaders/planeetgridVertWater.wgsl",	"shaders/planeetgridFragWater.wgsl"	);

	scherm.maakRekenShader(	"waterDruk", 		"shaders/waterDruk.comp"											);
	scherm.maakRekenShader(	"waterStroming", 	"shaders/waterStroming.comp"										);
	scherm.maakRekenShader(	"waterGemiddelde", 	"shaders/waterGemiddelde.comp"										);
	scherm.maakRekenShader(	"grondGelijkmaker", "shaders/grondGelijkmaker.comp"										);

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

	geo = new planeet(8, hoogteMonsteraar);

	bool 		roteerMaar		= false,
				waterStroomt	= true,
				waterStap		= false,
				zonDraait		= false,
				tekenWater		= true;

	glm::vec3	kijkPlek		(0.0f)				,
				regenPlek		(0.0f)				,
				zonPos			(0.0f)				;
	float		grondSchaal		= 1.0,
				verdamping		= 0.0004;

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
			case GLFW_KEY_K:			verdamping 		= glm::max(0.0f, verdamping - 0.0002f);	break;
			case GLFW_KEY_L:			verdamping 		= glm::max(0.0f, verdamping + 0.0002f);	break;
			}
	};

	scherm.setCustomKeyhandler(toetsenbord);

	glm::vec2 zonRot = glm::vec2(-0.6f, 0.3);

	//Opslag-buffer met de parameters voor de reken-shaders (bind-groep 0, binding 3)
	WGPUDevice apparaat = weergaveScherm::deelApparaat();
	WGPUQueue  rij 		= weergaveScherm::deelRij();

	WGPUBufferDescriptor beschrijving = WGPU_BUFFER_DESCRIPTOR_INIT;
	beschrijving.usage 	= WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
	beschrijving.size 	= sizeof(rekenParameters);
	WGPUBuffer rekenParBuffer = wgpuDeviceCreateBuffer(apparaat, &beschrijving);

	auto berekenShaderBinden = [&]()
	{
		geo->bindVrwrkrOpslagen(scherm);
		scherm.verbindRekenBuffer(3, rekenParBuffer);
	};

	//de weergave-parameters (bind-groep 0, binding 2): layout volgens extraParameters in de WGSL
	float extra[16] = { 0.0f };

	rekenParameters rekenPar = {};

	const uint32_t rekenGroepen = (uint32_t)((geo->aantalVakjes() + 63) / 64);

	while(!scherm.stopGewenst())
	{
		kijkPlek = glm::vec3(glm::inverse(scherm.modelZicht())[3]);

		if(zonDraait)
			zonRot.x += 0.01;

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

		regenPlek = glm::normalize(glm::vec3(1.0, 1.0, 1.0) * willekeurigeVec3());

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
		rekenPar.regenPlek[0] 	= regenPlek.x;
		rekenPar.regenPlek[1] 	= regenPlek.y;
		rekenPar.regenPlek[2] 	= regenPlek.z;
		rekenPar.regenPlek[3] 	= 0.0f;

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
			scherm.doeRekenVerwerker("grondGelijkmaker", 	glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			geo->volgendeRonde();
			scherm.doeRekenVerwerker("waterStroming", 		glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			scherm.doeRekenVerwerker("waterDruk", 			glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			scherm.doeRekenVerwerker("waterGemiddelde", 	glm::uvec3(rekenGroepen, 1, 1), berekenShaderBinden);
			waterStap = false;

			geo->volgendeRonde();
		}

		wgpFoutControle("Frame: ");
	}

	wgpuBufferRelease(rekenParBuffer);

	delete geo;
}