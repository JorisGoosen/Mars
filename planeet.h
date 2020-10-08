#include <geometrie/geodesisch.h>
#include <stdexcept>
#include <map>
#include <set>
#include <iostream>

class planeet : public geodesisch
{
public:
	typedef std::vector<std::set<glm::uint32>> buurt;
	
	planeet(size_t onderverdelingen = 6);

protected:
	void maakLijstBuren();
	void burenAlsEigenschapWijzers();
	void maakPingPongOpslagen();

private:
	std::vector<glm::uint32> 			_eigenschappen;
	buurt								_buren;
	GLuint								_pingPongOpslag[2];
										
};