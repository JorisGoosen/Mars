#include <helpers.h>
#include <vector>

class monsterPNG
{
public:
	monsterPNG(unsigned char * pngGegevens, glm::uvec2 afmetingen);

	glm::vec4 operator()(glm::uvec2 plaats, float straal = 0.0); //straal == 0 is puntmonstering
	glm::vec4 operator()(glm::vec2 plaats, 	float straal = 0.0); //straal == 0 is puntmonstering

private:
	std::vector<std::vector<glm::vec4> > 	_vakken;
	glm::uvec2								_afmetingen;
};