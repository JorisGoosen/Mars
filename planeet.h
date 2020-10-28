#include <geometrie/geodesisch.h>
#include <stdexcept>
#include <map>
#include <set>
#include <iostream>
#include <vrwrkrOpslagDing.h>

#define GS_ZAND		0
#define GS_GROND	1	//Ook wel humus of rijke grond
#define GS_ROTS		2
#define GS_KLEI		3
#define GS_IJS		4
#define GS_LOESS	5

struct vak
{
	int			grondSoort		= GS_ZAND;
	float 		grondHoogte		= 1.0;
	float		waterHoogte		= 0.0;
	float		leven			= 0.0;
	int			iets			= 0;
	float		pijpen[6]		= { 0, 0, 0, 0, 0, 0 }; //Naar andere vakken

	//Zouden eventueel in losse buffer kunnen want worden nooit aangepast:
	glm::uint32	burenAantal		= 0;
	glm::uint32	buren[6]		= { 0, 0, 0, 0, 0, 0 };
};

class planeet : public geodesisch
{
public:
	typedef std::vector<std::set<glm::uint32>> buurt;
	
	planeet(size_t onderverdelingen, std::function<float(glm::vec2)> hoogteMonsteraar);

	size_t 	aantalVakjes() const { return _vakken[0].size(); }
	void	volgendeRonde();
	void	bindVrwrkrOpslagen();
	
protected:
	void maakLijstBuren();
	void burenAlsEigenschapWijzers();
	void maakPingPongOpslagen();

private:
	std::vector<glm::uint32> 			_eigenschappen;
	buurt								_buren;
	std::vector<vak>					_vakken			[2];
	vrwrkrOpslagDing<vak>			*	_pingPongVakken	[2];
	size_t								_pingIsDit	= 0;
	std::function<float(glm::vec2)> 	_hoogteMonsteraar;
									
										
};