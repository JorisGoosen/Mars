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

struct vakje
{
	int		grondSoort		= GS_ZAND;
	float 	grondHoogte		= 1.0;
	float	waterHoogte		= 0.0;
	float	leven			= 0.0;
};

class planeet : public geodesisch
{
public:
	typedef std::vector<std::set<glm::uint32>> buurt;
	
	planeet(size_t onderverdelingen = 6);

	size_t 	aantalVakjes() const { return _vakjes[0].size(); }
	void	volgendeRonde();
	void	bindVrwrkrOpslagen();

protected:
	void maakLijstBuren();
	void burenAlsEigenschapWijzers();
	void maakPingPongOpslagen();

private:
	std::vector<glm::uint32> 			_eigenschappen;
	buurt								_buren;
	std::vector<vakje>					_vakjes		[2];
	vrwrkrOpslagDing<vakje>			*	_pingPong	[2];
	size_t								_pingIsDit	= 0;
									
										
};