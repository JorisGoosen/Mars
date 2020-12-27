#include <weergaveSchermPerspectief.h>
#include "planeet.h"
#include <iostream>
#include "monsterPNG.h"
#include <random>
#include <perlinRuis.h>

int main()
{
	weergaveSchermPerspectief scherm("Planeet", 1280, 720, 8);

	glEnable(GL_CULL_FACE);

	scherm.maakShader(		"planeetWeergave", 	"shaders/planeetgrid.vert", 		"shaders/planeetgrid.frag"		);
	

	scherm.maakRekenShader(	"waterDruk", 		"shaders/waterDruk.comp"											);
	scherm.maakRekenShader(	"waterStroming", 	"shaders/waterStroming.comp"										);
	scherm.maakRekenShader(	"waterGemiddelde", 	"shaders/waterGemiddelde.comp"										);

	glClearColor(0,0,0,1);

	//unsigned char * MarsHoogte 	= nullptr;
	//glm::uvec2 MarsHoogteBH		= scherm.laadTextuurUitPng("MARS_Hoogte.png", "Mars", & MarsHoogte);
	//monsterPNG MOLA(MarsHoogte, MarsHoogteBH);

	//planeet geo(7, [&](glm::vec2 plek){ return MOLA(plek).x; });

//	std::random_device 	willekeur;  //Wordt gebruikt om het zaadje te planten
//	std::mt19937 		bemonsteraar(willekeur()); 
//	std::uniform_real_distribution<> dis(0.0, 1.0);

	perlinRuis ruisje0, ruisje1;
	planeet geo(7, [&](glm::vec3 plek)
	{
		//plek *= 0.5f;

		 float	val  = ruisje0.geefIniqoQuilesRuis(plek * glm::vec3(07.0f)) * 0.5; 
		 		val += ruisje1.geefIniqoQuilesRuis(plek * glm::vec3(13.0f)) * 0.35f;
		 		val += ruisje0.geefIniqoQuilesRuis(plek * glm::vec3(17.0f)) * 0.225f;
				val += ruisje1.geefIniqoQuilesRuis(plek * glm::vec3(23.0f)) * 0.1625f; 
				val += ruisje0.geefIniqoQuilesRuis(plek * glm::vec3(03.0f)) * 0.6f;
				val += ruisje1.geefIniqoQuilesRuis(plek * glm::vec3(02.0f)) * 0.8f;
				val	+= 1.0f;
				val *= 0.5f;
		
		 return (val > 0.0f ? pow( val, 0.666f) * 0.1833f : 0.0f) - 0.5f; 
	});

	bool 		roteerMaar 		= false,
				waterStroomt	= true,
				waterStap		= false,
				zonDraait		= false;

	glm::vec3 	verplaatsing	(0.0f, 0.0f, -2.0f)	,
				kijkPlek		(0.0f)				,
				regenPlek		(0.0f)				,
				zonPos			(0.0f)				;
	glm::vec2 	verdraaiing		(0.0f, 0.0f)		,
				draaisnelheid	(0.01, 0.0)			;

	float		grondMult		= 100.0,
				grondSchaal		= 0.3,
				verdamping		= 0.002;

	weergaveScherm::keyHandlerFunc toetsenbord = [&](int key, int scancode, int action, int mods)
	{
		const float tol = 0.2, tik = 0.05;

		if(action == GLFW_PRESS)
			switch(key)
			{
			case GLFW_KEY_SPACE:	waterStroomt 	= !waterStroomt;	break;
			case GLFW_KEY_R:		roteerMaar 		= !roteerMaar;		break;
			case GLFW_KEY_Z:		zonDraait 		= !zonDraait;		break;
			}

		switch(key)
		{
		case GLFW_KEY_A: case GLFW_KEY_LEFT: 	if(roteerMaar)	glm::max(draaisnelheid.x * (1.0f - tol), 0.0f);	else verdraaiing.x -= tik;		break;
		case GLFW_KEY_D: case GLFW_KEY_RIGHT:	if(roteerMaar)	glm::max(draaisnelheid.x * (1.0f + tol), 1.0f);	else verdraaiing.x += tik;		break;
		case GLFW_KEY_Q: case GLFW_KEY_UP: 		if(roteerMaar)	glm::max(draaisnelheid.y * (1.0f - tol), 0.0f);	else verdraaiing.y -= tik;		break;
		case GLFW_KEY_E: case GLFW_KEY_DOWN: 	if(roteerMaar)	glm::max(draaisnelheid.y * (1.0f + tol), 1.0f);	else verdraaiing.y += tik;		break;
		case GLFW_KEY_S:						verplaatsing.z -= 0.1f;																			break;
		case GLFW_KEY_W:						verplaatsing.z += 0.1f;																			break;
		case GLFW_KEY_ENTER:					waterStap 		= true;																			break;
		case GLFW_KEY_SEMICOLON:				grondMult 		= glm::max(1.0f, grondMult * 0.9f);												break;
		case GLFW_KEY_APOSTROPHE:				grondMult 		= glm::max(1.0f, grondMult * 1.1f);												break;
		case GLFW_KEY_K:						verdamping 		= glm::max(0.0f, verdamping - 0.1f);											break;
		case GLFW_KEY_L:						verdamping 		= glm::max(0.0f, verdamping + 0.1f);											break;
		}	
	};

	scherm.setCustomKeyhandler(toetsenbord);

	auto grondShaderInfo = [&]()
	{
		glUniform1f(	glGetUniformLocation(scherm.huidigProgramma(), "grondMult"		),		grondMult				);
		glUniform1f(	glGetUniformLocation(scherm.huidigProgramma(), "grondSchaal"	),		grondSchaal				);
		glUniform1f(	glGetUniformLocation(scherm.huidigProgramma(), "verdamping"		),		verdamping				);
		glUniform3fv(	glGetUniformLocation(scherm.huidigProgramma(), "kijkPlek"		),  1, 	glm::value_ptr(kijkPlek));
		glUniform3fv(	glGetUniformLocation(scherm.huidigProgramma(), "zonPos"			),  1, 	glm::value_ptr(zonPos)	);
		glUniform3fv(	glGetUniformLocation(scherm.huidigProgramma(), "regenPlek"		),  1, 	glm::value_ptr(regenPlek));
		ruisje0.zetKnooppunten(3, 4);
		ruisje0.zetKnooppunten(5, 6);
	};

	auto berekenShaderBinden = [&]()
	{
		geo.bindVrwrkrOpslagen(); 
		grondShaderInfo();
	};

	glm::vec2 zonRot = glm::vec2(0.0f);

	glErrorToConsole("Voordat we beginnen: ");
	std::cout << "Laten we beginnen..." << std::endl;
	while(!scherm.stopGewenst())
	{
		
		scherm.setModelView(
			glm::rotate(
				glm::rotate(
					glm::translate(glm::mat4(1.0f), verplaatsing),
					verdraaiing.y,
					glm::vec3(1.0f, 0.0f, 0.0f)
				), 
				verdraaiing.x, 
				glm::vec3(0.0f, 1.0f, 0.0f)
			)
		);

		kijkPlek = verplaatsing;//glm::vec3(0.0f, 0.0f, -1.0f);;//glm::mat3(scherm.modelView()) * -verplaatsing;

		if(zonDraait)
			zonRot.x += 0.013333;

		glm::mat4 zonRoteerder = 
			glm::rotate(
				glm::rotate(
					//scherm.modelView(),
					glm::mat4(1.0f),
					zonRot.x,
					glm::vec3(0.0f, 1.0f, 0.0f)
				),
				zonRot.y, // += 0.02,
				glm::vec3(1.0f, 0.0f, 0.0f)
			);

		glm::vec4 zonPosTdlk = scherm.modelView() * zonRoteerder * glm::normalize(glm::vec4(0.0, sin(zonPos.x / 365.0f), 7.0, 1.0));

		zonPos = zonPosTdlk.xyz() / zonPosTdlk.w;

		static size_t regenRot = 0;

		if(regenRot ++ % 20 == 0)	
			regenPlek = glm::normalize(willekeurigeVec3());

		

		glDisable(GL_BLEND);
		scherm.bereidRenderVoor("planeetWeergave");
		glUniform1ui(	glGetUniformLocation(scherm.huidigProgramma(), "grondNietWater"), 	1);
		grondShaderInfo();
		geo.tekenJezelf();
		glErrorToConsole("geo.tekenJezelf() grond: ");

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		scherm.bereidRenderVoor("planeetWeergave", false);
		glUniform1ui(	glGetUniformLocation(scherm.huidigProgramma(), "grondNietWater"), 	0);
		grondShaderInfo();
		geo.tekenJezelf();
		glErrorToConsole("geo.tekenJezelf() water: ");

		scherm.rondRenderAf();
		glErrorToConsole("rondRenderAf: ");
		
		if(roteerMaar)
			verdraaiing += draaisnelheid;

		if(waterStroomt || waterStap)
		{
			geo.volgendeRonde();
			scherm.doeRekenVerwerker("waterStroming", 	glm::uvec3(geo.aantalVakjes(), 1, 1), berekenShaderBinden);	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			scherm.doeRekenVerwerker("waterDruk", 		glm::uvec3(geo.aantalVakjes(), 1, 1), berekenShaderBinden);	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			scherm.doeRekenVerwerker("waterGemiddelde", glm::uvec3(geo.aantalVakjes(), 1, 1), berekenShaderBinden);	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			waterStap = false;
		}
	}
}