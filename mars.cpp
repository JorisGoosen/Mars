//This file simply opens an OpenGL window where a quad will be rendered and some simple shaders to display this fact, press escape to exit.
#include <RenderSchermPerspectief.h>
#include <geometrie/icosahedron.h>
#include <iostream>

int main()
{
	RenderSchermPerspectief scherm("Perspectief Demo", 1280, 720, 4);


	scherm.maakVlakVerdelingsShader("deeltjes", "shaders/deeltjes.vert", "shaders/deeltjes.frag", "shaders/deeltjes.tess", "shaders/deeltjes.tctl");

	glClearColor(0,0,0,0);

	scherm.laadTextuurUitPng("MARS_Hoogte.png", "Mars");

	Icosahedron ico;

	float rot = 0.0f, rot2=1.5f;

	float zoom = 0.1, groente = 0.0f;

	while(!scherm.stopGewenst())
	{
		scherm.RecalculateProjection();
		scherm.setModelView(
			glm::rotate(
				glm::rotate(
					glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.45f + (0.375 * sin(zoom)))), 
					rot, 
					glm::vec3(0.0f, 0.0f, 1.0f)
				), 
				rot2, 
				glm::vec3(0.0f, 1.0f, 0.0f)
			)
		);
		scherm.bereidRenderVoor();

		
		glUniform1f(glGetUniformLocation(scherm.geefEnigeProgrammaHandvat(), "groente"), groente);

		ico.tekenJezelfPatchy();
		glErrorToConsole("Woppaloppa Mainloop ");
		//scherm.renderQuad();
		scherm.rondRenderAf();
		glErrorToConsole("rondRenderAf: ");
		
		rot += 0.00001f;
		rot2 += 0.0012f;
	//	zoom += 0.01f;
		groente += 0.001f;

		groente = glm::min(groente, 0.95f);
	}

}