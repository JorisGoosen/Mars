//Dit zou een ray marching algo moeten laten zien ooit, we gaan het meemaken
#include "../weergaveSchermVierkant.h"
#include "../nepScherm.h"
#include <iostream>

int main(int argc, const char * argv[])
{
	bool toonHetLand = argc > 1 && argv[1] == std::string("toonHetLand");

	weergaveSchermVierkant scherm("Marcheer Demo", 1024, 1024, false);
	
	scherm.maakShader("bewerkHetLand", 	"shaders/bewerkHetLand.vert", 	"shaders/bewerkHetLand.frag");
	scherm.maakShader("marcheerDemo", 	"shaders/marcheerDemo.vert", 	"shaders/marcheerDemo.frag"	);
	scherm.maakShader("toonHetLand", 	"shaders/toonHetLand.vert", 	"shaders/toonHetLand.frag"	);
	

	glm::uvec2 textuurGrootte = scherm.laadTextuurUitPng("plaatjes/handLand.png", "handLand", false, false, false);
	//scherm.maakTextuur("handLandTwee", textuurGrootte.x, textuurGrootte.y, false, false, false);
	scherm.laadTextuurUitPng("plaatjes/handLand.png", "handLandTwee", false, false, false);

	nepScherm nepperd(&scherm, "handLandTwee");

	scherm.initVierkant();
	nepperd.bereidWeergevenVoor("bewerkHetLand");
	scherm.bindTextuur("handLand", 0);
	glUniform1i(glGetUniformLocation(scherm.huidigProgramma(), "landRuis"), 0);
	scherm.geefVierkantWeer();
	nepperd.rondWeergevenAf();
	
	float rot = 0.;
	while(!scherm.stopGewenst())
	{
		if(toonHetLand)		scherm.bereidWeergevenVoor("toonHetLand"); 
		else				scherm.bereidWeergevenVoor("marcheerDemo");

		glm::mat4 modelZicht = glm::rotate(glm::mat4(1.0f), sinf(rot) * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 inversie   = glm::transpose(glm::inverse(modelZicht));
		glUniformMatrix4fv(glGetUniformLocation(scherm.huidigProgramma(), "modelZicht"), 1, GL_FALSE, glm::value_ptr(modelZicht));
		glUniformMatrix4fv(glGetUniformLocation(scherm.huidigProgramma(), "inversie"), 1, GL_FALSE, glm::value_ptr(inversie));

		scherm.bindTextuur("handLandTwee", 0);
		glUniform1i(glGetUniformLocation(scherm.huidigProgramma(), "landTwee"), 0);
		scherm.geefVierkantWeer();
		scherm.rondWeergevenAf();

		rot += 0.1;
	}
}