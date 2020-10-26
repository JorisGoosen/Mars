#include <weergaveSchermPerspectief.h>
#include "planeet.h"
#include <iostream>
#include "monsterPNG.h"

int main()
{
	weergaveSchermPerspectief scherm("Planeet");

	scherm.maakShader(		"planeetWeergave", 		"shaders/planeetgrid.vert", "shaders/planeetgrid.frag");
	scherm.maakRekenShader(	"planeetBerekening", 	"shaders/planeetgrid.comp");

	glClearColor(0,0,0,0);

	unsigned char * MarsHoogte 	= nullptr;
	glm::uvec2 MarsHoogteBH		= scherm.laadTextuurUitPng("MARS_Hoogte.png", "Mars", & MarsHoogte);
	monsterPNG MOLA(MarsHoogte, MarsHoogteBH);

	planeet geo(5, [&](glm::vec2 plek)
	{ 
		//plek.x += 1.0;
		//plek.x *= 0.25f;
		//plek.x += 0.5;

		//if(plek.x < 0.0f || plek.x > 1.0f)
		//	std::cout << "geo wil: " << plek.x << ",\t" << plek.y << std::endl; 
		
		return MOLA(plek).x; 
	});

	bool roteerMaar = false;

	glm::vec3 	verplaatsing	(0.0f, 0.0f, -1.5f)	;
	glm::vec2 	verdraaiing		(0.0f, 0.0f)		,
				draaisnelheid	(0.01, 0.0)			;

	weergaveScherm::keyHandlerFunc toetsenbord = [&](int key, int scancode, int action, int mods)
	{
		const float tol = 0.2, tik = 0.05;

		if(action == GLFW_PRESS && key == GLFW_KEY_SPACE) roteerMaar = !roteerMaar;

		switch(key)
		{
		case GLFW_KEY_A:			if(roteerMaar)	glm::max(draaisnelheid.x * (1.0f - tol), 0.0f);	else verdraaiing.x -= tik;		break;
		case GLFW_KEY_D:			if(roteerMaar)	glm::max(draaisnelheid.x * (1.0f + tol), 1.0f);	else verdraaiing.x += tik;		break;
		case GLFW_KEY_Q:			if(roteerMaar)	glm::max(draaisnelheid.y * (1.0f - tol), 0.0f);	else verdraaiing.y -= tik;		break;
		case GLFW_KEY_E:			if(roteerMaar)	glm::max(draaisnelheid.y * (1.0f + tol), 1.0f);	else verdraaiing.y += tik;		break;
		case GLFW_KEY_S:			verplaatsing.z -= 0.1f;		break;
		case GLFW_KEY_W:			verplaatsing.z += 0.1f;		break;
		}	
	};

	scherm.setCustomKeyhandler(toetsenbord);

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
		scherm.bereidRenderVoor("planeetWeergave");

		geo.volgendeRonde();

		geo.tekenJezelf();
		glErrorToConsole("geo.tekenJezelf(): ");

		scherm.rondRenderAf();
		glErrorToConsole("rondRenderAf: ");
		
		if(roteerMaar)
			verdraaiing += draaisnelheid;

		
		scherm.doeRekenVerwerker("planeetBerekening", glm::uvec3(geo.aantalVakjes(), 1, 1), [&](){ geo.bindVrwrkrOpslagen(); });
	}
}