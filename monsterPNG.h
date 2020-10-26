#include <helpers.h>
#include <vector>

class monsterPNG
{
public:
	monsterPNG(unsigned char * pngGegevens, glm::uvec2 afmetingen);

	glm::vec4 operator()(glm::uvec2 plaats, glm::uvec2 straal = glm::uvec2(0)  );
	glm::vec4 operator()(glm::vec2  plaats, glm::vec2  straal = glm::vec2(0.0) );

private:
	std::vector<std::vector<glm::vec4> > 	_vakjes;
	glm::uvec2								_afmetingen;
};