//Dit zou een landschap dmv driehoeken moeten laten zien
#include "../weergaveSchermPerspectief.h"
#include "../geometrie/vierkantRooster.h"
#include "../nepScherm.h"

int main(int argc, const char * argv[])
{
	int 	vermindering 	= argc == 1 ? 0 : std::stoi(argv[1]),
		 	deling 			= pow(2, vermindering);

	bool toonHetLand = argc > 2 && argv[2] == std::string("toonHetLand");

	weergaveSchermPerspectief scherm("Land Demo", 1024, 1024, false);
	
	scherm.maakShader("bewerkHetLand", 	"shaders/bewerkHetLand.vert", 	"shaders/bewerkHetLand.frag");
	scherm.maakShader("landDemo", 		"shaders/landDemo.vert", 		"shaders/landDemo.frag");
	scherm.maakShader("toonHetLand", 	"shaders/toonHetLand.vert", 	"shaders/toonHetLand.frag"	);
	
	glm::uvec2 landGrootte = scherm.laadTextuurUitPng("plaatjes/handLand.png", "handLandOri", false, false, false);

	scherm.laadTextuurUitPng("plaatjes/handLand.png", "handLand", false, false, false);

	nepScherm nepperd(&scherm, "handLand");

	scherm.initVierkant();
	nepperd.bereidWeergevenVoor("bewerkHetLand");
	scherm.bindTextuur("handLandOri", 0);
	glUniform1i(glGetUniformLocation(scherm.huidigProgramma(), "landRuis"), 0);
	scherm.geefVierkantWeer();
	nepperd.rondWeergevenAf();

	vierkantRooster landRooster(landGrootte.x/deling, landGrootte.y/deling);

	glEnable(GL_DEPTH_TEST);
	glClearColor(0., 0., 0.5, 0.);
	
	while(!scherm.stopGewenst())
	{
		if(toonHetLand)		
		{
			scherm.bereidWeergevenVoor("toonHetLand"); 
			scherm.bindTextuur("handLand", 0);
			glUniform1i(glGetUniformLocation(scherm.huidigProgramma(), "landTwee"), 0);
			scherm.geefVierkantWeer();
			scherm.rondWeergevenAf();
		}			
		else				
		{
			scherm.bereidWeergevenVoor("landDemo");
			scherm.bindTextuur("handLand", 0);
			glUniform1i(glGetUniformLocation(scherm.huidigProgramma(), "handLand"), 	0);
			glUniform1i(glGetUniformLocation(scherm.huidigProgramma(), "vermindering"), vermindering);
			landRooster.tekenJezelf();
			scherm.rondWeergevenAf();
		}
	}
}