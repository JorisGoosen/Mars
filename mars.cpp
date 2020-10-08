//This file simply opens an OpenGL window where a quad will be rendered and some simple shaders to display this fact, press escape to exit.
#include <weergaveSchermPerspectief.h>
#include "planeet.h"
#include <iostream>

int main()
{
	weergaveSchermPerspectief scherm("Planeet");

	scherm.maakShader("planeetgrid", "shaders/planeetgrid.vert", "shaders/planeetgrid.frag");

	glClearColor(0,0,0,0);

	scherm.laadTextuurUitPng("MARS_Hoogte.png", "Mars");

	planeet geo(6);

	float rot = 0.0f;
	while(!scherm.stopGewenst())
	{
		scherm.RecalculateProjection();
		scherm.setModelView(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.5f)), rot, glm::vec3(0.0f, 1.0f, 0.0f)));
		scherm.bereidRenderVoor();

		geo.tekenJezelf();
		glErrorToConsole("geo.tekenJezelf(): ");

		scherm.rondRenderAf();
		glErrorToConsole("rondRenderAf: ");
		rot += 0.007f;
	}
}