//This file simply opens an OpenGL window where an icosahedron will be rendered with a nice geometry shader to make it rounder, press escape to exit.
#include "../weergaveSchermPerspectief.h"
#include "../geometrie/icosahedron.h"

int main()
{
	weergaveSchermPerspectief scherm("Geometry Shader Demo");
	scherm.maakGeometrieShader("geometryDemo", "shaders/geometryDemo.vert", "shaders/geometryDemo.frag", "shaders/geometryDemo.geom");

	glClearColor(0,0,0,0);

	icosahedron ico;

	float rot = 0.0f;
	while(!scherm.stopGewenst())
	{
		scherm.zetModelZicht(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -4.0f)), rot, glm::vec3(0.0f, 1.0f, 0.0f)));
		scherm.bereidWeergevenVoor();
		ico.tekenJezelf();
		//scherm.geefVierkantWeer();
		scherm.rondWeergevenAf();


		rot += 0.01f;
	}
}