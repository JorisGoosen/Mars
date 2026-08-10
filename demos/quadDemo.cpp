//This file simply opens an OpenGL window where a quad will be rendered and some simple shaders to display this fact, press escape to exit.
#include "../weergaveSchermVierkant.h"

int main()
{
	weergaveSchermVierkant scherm("Quad Demo");
	scherm.maakShader("quadDemo", "shaders/quadDemo.vert", "shaders/quadDemo.frag");

	while(!scherm.stopGewenst())
	{
		scherm.bereidWeergevenVoor();
		scherm.geefVierkantWeer();
		scherm.rondWeergevenAf();
	}
}