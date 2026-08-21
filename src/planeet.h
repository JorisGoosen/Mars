#include <geometrie/geodesisch.h>
#include <stdexcept>
#include <map>
#include <set>
#include <iostream>
#include <vrwrkrOpslagDing.h>

class weergaveScherm;

#define GS_ZAND		0
#define GS_GROND	1	//Ook wel humus of rijke grond
#define GS_ROTS		2
#define GS_KLEI		3
#define GS_IJS		4
#define GS_LOESS	5

struct vak
{
	int			grondSoort	;
	float 		zandHoogte	, //losse zand/deklaag boven op de rots (altijd >= 0); terreinhoogte = rotsHoogte + zandHoogte
				rotsHoogte	,
				waterHoogte	,
				waterSchijn	,
				bodemVocht	,
				ijs			,
				leven		,
				droesem		,
				luchtVocht	,
				temperatuur	,
				luchtdruk	,
				wolken		,
				zonZicht	, //fractie zonlicht die het terrein bereikt (schaduwkaart; 1 = volle zon)
				pijpen[6],
				vochtPijpenA[6], //flux van damp per buur (behoudend)
				vochtPijpenB[6]; //flux van wolken per buur (behoudend)
	glm::vec2	snelheid	;
	glm::vec2	wind		; //atmosferische wind in het lokale raakvlak (west, noord)
	glm::vec2	plek		; //opgetelde snelheden, om water mee te tekenen
};

struct vakMeta
{
	glm::vec4	normaal			;	// base alignment 16 (WGSL vec4f)
	glm::vec4	oost			;	// lokale raakvlak-basis (oost) in wereldcoördinaten
	glm::vec4	noord			;	// lokale raakvlak-basis (noord) in wereldcoördinaten
	glm::vec3	gradWeights		;	// (a,b,c) van M = avgDist · C⁻¹, 2×2 symmetrische correctie-matrix
	float		_padGrad		;	// opvulling: WGSL vec3f heeft align 16, C++ glm::vec3 heeft align 4
	glm::vec2	buurRicht[6]	;	// 3 * 4
	glm::uint32	buren[6]		,	//     6 
				burenAantal		;	// 	   1
	float		gradSchaal		;	// 2 / gemiddelde buurafstand: schaalt de LS-gradient/divergentie naar de ware waarde (diepte-onafhankelijk)
};									//

//Beginwaarden per cel bij het maken van de planeet (defaults = lege Mars).
struct planeetInit
{
	float	water			= 0.0f;	//waterHoogte per cel
	float	bodemVocht		= 0.0f;
	float	wolken			= 0.0f;
	float	leven			= 0.0f;
	float	ijs				= 0.0f;
	float	damp			= 0.0f;	//luchtVocht
	float	zandDeksel		= 0.0f;	//dikte van de begin-zandlaag bovenop de rots
	float	temperatuur		= 273.0f;	//vlakke begintemperatuur (Kelvin)
};

class planeet : public geodesisch
{
public:
	typedef std::vector<std::set<glm::uint32>> buurt;
	
	planeet(size_t onderverdelingen, std::function<float(glm::vec2)> hoogteMonsteraar, const planeetInit & init = planeetInit());
	planeet(size_t onderverdelingen, std::function<float(glm::vec3)> ruis, const planeetInit & init = planeetInit());

	size_t 	aantalVakjes() const { return _vakken[0].size(); }
	float	hoogsteGrond() const { return _hoogsteGrond; }
	void	volgendeRonde();
	void	bindVrwrkrOpslagen() { }
	void	bindVrwrkrOpslagen(weergaveScherm & scherm);

	//Voor het diagnose-script (--diag in mars.cpp): de storage-buffer die nu als
	//"vakken0" (de laatste uitgerekende stand) gebonden wordt + de plek van een cel.
	WGPUBuffer	huidigeOpslag() const { return _pingPongVakken[_pingIsDit]->opslag(); }
	glm::vec3	punt3(size_t id) const { return _punten->ggvPunt3(id); }

	//Richtings-asymmetrie van de buurlus: |Σ_j normalize(pos_buur − pos)/(n)|.
	//0 voor een perfect symmetrische buurlus, groter waar het grid onregelmatig is.
	float		buurAsymmetrie(size_t id) const;
	size_t		burenAantal(size_t id) const { return _vakMetas[id].burenAantal; }
	glm::uint32	buurVan(size_t id, size_t k) const { return _vakMetas[id].buren[k]; }
	
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
	planeetInit							_init;
	float								_hoogsteGrond	= 0.0f;
									
										
};