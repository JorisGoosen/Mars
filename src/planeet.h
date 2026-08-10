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
	int			grondSoort	;
	float 		grondHoogte	,
				waterHoogte	,
				waterSchijn	,
				vocht		,
				ijs			,
				leven		,
				droesem		,
				pijpen[6]	;
	glm::vec2	snelheid	;
	glm::vec2	plek		; //opgetelde snelheden, om water mee te tekenen
};

struct vakMeta
{
	glm::vec4	normaal			;	// base alignment 4 bytes want std430
	glm::vec2	buurRicht[6]	;	// 3 * 4
	glm::uint32	buren[6]		,	//     6 
				burenAantal		,	// 	   1
				opvulling		;	//     1
};									//

class planeet : public geodesisch
{
public:
	typedef std::vector<std::set<glm::uint32>> buurt;
	
	planeet(size_t onderverdelingen, std::function<float(glm::vec2)> hoogteMonsteraar);
	planeet(size_t onderverdelingen, std::function<float(glm::vec3)> ruis);

	size_t 	aantalVakjes() const { return _vakken[0].size(); }
	void	volgendeRonde();
	void	bindVrwrkrOpslagen();
	
protected:
	void bouwPlaneet();
	void maakLijstBuren();
	void burenAlsEigenschapWijzers();
	void maakPingPongOpslagen();
	void browniaansLand();
	void gaHetKlokjeRondMetDeBuren(size_t ID);

private:
	std::vector<glm::uint32> 			_eigenschappen;
	buurt								_buren;
	std::vector<vak>					_vakken			[2];
	vrwrkrOpslagDing<vak>			*	_pingPongVakken	[2];
	std::vector<vakMeta>				_vakMetas;
	vrwrkrOpslagDing<vakMeta>		*	_vakMetaOpslag;
	size_t								_pingIsDit	= 0;
	std::function<float(glm::vec2)> 	_hoogteMonsteraar;
	std::function<float(glm::vec3)> 	_ruis;
	bool								_isRuis;
									
										
};