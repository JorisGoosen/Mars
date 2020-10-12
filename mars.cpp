//This file simply opens an OpenGL window where a quad will be rendered and some simple shaders to display this fact, press escape to exit.
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

	planeet geo(7, [&](glm::vec2 plek){ return MOLA(plek).x; });

	float rot = 0.0f;

	bool roteerMaar = true;

	weergaveScherm::keyHandlerFunc toetsenbord = [&](int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
			roteerMaar = !roteerMaar;
	};

	scherm.setCustomKeyhandler(toetsenbord);

	while(!scherm.stopGewenst())
	{
		scherm.RecalculateProjection();
		scherm.setModelView(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.5f)), rot, glm::vec3(0.0f, 1.0f, 0.0f)));
		scherm.bereidRenderVoor("planeetWeergave");

		geo.volgendeRonde();

		geo.tekenJezelf();
		glErrorToConsole("geo.tekenJezelf(): ");

		scherm.rondRenderAf();
		glErrorToConsole("rondRenderAf: ");
		
		if(roteerMaar)
			rot += 0.007f;

		
		scherm.doeRekenVerwerker("planeetBerekening", glm::uvec3(geo.aantalVakjes(), 1, 1), [&](){ geo.bindVrwrkrOpslagen(); });
	}
}