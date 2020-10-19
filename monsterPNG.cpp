#include "monsterPNG.h"


monsterPNG::monsterPNG(unsigned char * pngGegevens, glm::uvec2 afmetingen) : _afmetingen(afmetingen)
{
	//van linksonder naar rechts en dan telkens rij erboven

	_vakjes.resize(afmetingen.y);
	for(auto & kolom : _vakjes)
		kolom.resize(afmetingen.x);

	auto png = [&](auto x, auto y, auto z)
	{
		return pngGegevens[(x + (y * afmetingen.x)) * 4 + z];
	};

	for(size_t y=0; y<afmetingen.y; y++)
		for(size_t x=0; x<afmetingen.x; x++)
			_vakjes[y][x] = glm::vec4(glm::uvec4(png(x, y, 0), png(x, y, 1), png(x, y, 2), png(x, y, 3))) / glm::vec4(255.0f);
}


glm::vec4 monsterPNG::operator()(glm::uvec2 plaats, float straal)
{
	//negeer straal voor nu
	return _vakjes[ plaats.y % _afmetingen.y ][ plaats.x % _afmetingen.x ];
}

glm::vec4 monsterPNG::operator()(glm::vec2 plaats, float straal)
{
	glm::uvec2 vergroot = glm::uvec2(glm::floor(glm::vec2(_afmetingen) * plaats));

	return operator()(vergroot, straal);
}