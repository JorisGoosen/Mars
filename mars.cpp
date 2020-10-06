//This file simply opens an OpenGL window where a quad will be rendered and some simple shaders to display this fact, press escape to exit.
#include <RenderSchermPerspectief.h>
#include <geometrie/icosahedron.h>
#include <iostream>

int main()
{
	RenderSchermPerspectief scherm("Perspectief Demo");


//	scherm.maakVlakVerdelingsShader("deeltjes", "shaders/deeltjes.vert", "shaders/deeltjes.frag", "shaders/deeltjes.tess", "shaders/deeltjes.tctl");
	scherm.maakShader("deeltjes", "shaders/planeetgrid.vert", "shaders/planeetgrid.frag");


	glClearColor(0,0,0,0);

	//scherm.laadTextuurUitPng("MOLA_cylin_grijs.png", "Mars");
	scherm.laadTextuurUitPng("MARS_Hoogte.png", "Mars");

	std::cout << "Hmmm" << std::endl;
	Icosahedron ico;

	float rot = 0.0f;
	while(!scherm.stopGewenst())
	{
		scherm.RecalculateProjection();
		scherm.setModelView(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.5f)), rot, glm::vec3(0.0f, 1.0f, 0.0f)));
		scherm.bereidRenderVoor();

		ico.tekenJezelf();
		glErrorToConsole("Woppaloppa Mainloop ");
		//scherm.renderQuad();
		scherm.rondRenderAf();
		glErrorToConsole("rondRenderAf: ");
		rot += 0.007f;
	}
}