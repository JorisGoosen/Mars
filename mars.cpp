#include <weergaveSchermPerspectief.h>
#include "planeet.h"
#include <iostream>
#include "monsterPNG.h"

int main()
{
	weergaveSchermPerspectief scherm("Planeet");

	scherm.maakShader(		"planeetWeergave", 		"shaders/planeetgrid.vert", "shaders/planeetgrid.frag");
	//scherm.maakRekenShader(	"planeetBerekening", 	"shaders/planeetgrid.comp");

	scherm.maakRekenShader(	"waterDruk", 		"shaders/waterDruk.comp");
	scherm.maakRekenShader(	"waterStroming", 	"shaders/waterStroming.comp");

	glClearColor(0,0,0,1);

	unsigned char * MarsHoogte 	= nullptr;
	glm::uvec2 MarsHoogteBH		= scherm.laadTextuurUitPng("MARS_Hoogte.png", "Mars", & MarsHoogte);
	monsterPNG MOLA(MarsHoogte, MarsHoogteBH);

	planeet geo(7, [&](glm::vec2 plek)
	{ 
		//plek.x += 1.0;
		//plek.x *= 0.25f;
		//plek.x += 0.5;

		//if(plek.x < 0.0f || plek.x > 1.0f)
		//	std::cout << "geo wil: " << plek.x << ",\t" << plek.y << std::endl; 
		
		return MOLA(plek).x; 
	});

	bool 		roteerMaar 		= false,
				waterStroomt	= false,
				waterStap		= false;

	glm::vec3 	verplaatsing	(0.0f, 0.0f, -1.5f)	;
	glm::vec2 	verdraaiing		(0.0f, 0.0f)		,
				draaisnelheid	(0.01, 0.0)			;

	float		grondMult		= 6000.0,
				grondSchaal		= 0.05;

	weergaveScherm::keyHandlerFunc toetsenbord = [&](int key, int scancode, int action, int mods)
	{
		const float tol = 0.2, tik = 0.05;

		if(action == GLFW_PRESS)
			switch(key)
			{
			case GLFW_KEY_SPACE:	waterStroomt 	= !waterStroomt;	break;
			case GLFW_KEY_R:		roteerMaar 		= !roteerMaar;		break;
			}

		switch(key)
		{
		case GLFW_KEY_A: case GLFW_KEY_LEFT: 	if(roteerMaar)	glm::max(draaisnelheid.x * (1.0f - tol), 0.0f);	else verdraaiing.x -= tik;		break;
		case GLFW_KEY_D: case GLFW_KEY_RIGHT:	if(roteerMaar)	glm::max(draaisnelheid.x * (1.0f + tol), 1.0f);	else verdraaiing.x += tik;		break;
		case GLFW_KEY_Q: case GLFW_KEY_UP: 		if(roteerMaar)	glm::max(draaisnelheid.y * (1.0f - tol), 0.0f);	else verdraaiing.y -= tik;		break;
		case GLFW_KEY_E: case GLFW_KEY_DOWN: 	if(roteerMaar)	glm::max(draaisnelheid.y * (1.0f + tol), 1.0f);	else verdraaiing.y += tik;		break;
		case GLFW_KEY_S:						verplaatsing.z -= 0.1f;		break;
		case GLFW_KEY_W:						verplaatsing.z += 0.1f;		break;
		case GLFW_KEY_ENTER:					waterStap = true;			break;
		}	
	};

	scherm.setCustomKeyhandler(toetsenbord);

	auto grondShaderInfo = [&]()
	{
		glUniform1f(	glGetUniformLocation(scherm.huidigProgramma(), "grondMult"), 		grondMult);
		glUniform1f(	glGetUniformLocation(scherm.huidigProgramma(), "grondSchaal"), 		grondSchaal);
	};

	auto berekenShaderBinden = [&]()
	{
		geo.bindVrwrkrOpslagen(); 
		grondShaderInfo();
	};

	glErrorToConsole("Voordat we beginnen: ");
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
			waterStap = false;
		}
	}
}