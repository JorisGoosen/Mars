#include "planeet.h"
#include <random>
#include <iostream>
#include <weergaveScherm.h>

//Klemwaarden voor de terreinhoogtes, gekoppeld aan dezelfde waarden in shaders/planeetStructen.wgsl
const float minGrondHoogte = 10.0f;
const float maxGrondHoogte = 200.0f;

//De vak-struct moet byte-gelijk zijn aan de WGSL-struct (152 bytes: 104 + 2x24
//voor de vochtpijpen). Laat het compileren falen als iemand straks een veld
//toevoegt zonder de layout te fixen.
static_assert(sizeof(vak) == 152, "vak-struct moet 152 bytes groot zijn (gelijk aan WGSL)");

//vakMeta: normaal(16) + oost(16) + noord(16) + gradWeights(12) + padding(4) + buurRicht(48) + buren(24) + burenAantal(4) + opvulling(4) = 144
static_assert(sizeof(vakMeta) == 144, "vakMeta-struct moet 144 bytes groot zijn (gelijk aan WGSL)");


using namespace glm;

planeet::planeet(size_t onderverdelingen, std::function<float(glm::vec3)> ruis, bool beginMetWater) : geodesisch(onderverdelingen), _ruis(ruis), _isRuis(true), _beginMetWater(beginMetWater)
{
	bouwPlaneet();
}

planeet::planeet(size_t onderverdelingen, std::function<float(glm::vec2)> hoogteMonsteraar, bool beginMetWater) : geodesisch(onderverdelingen), _hoogteMonsteraar(hoogteMonsteraar), _isRuis(false), _beginMetWater(beginMetWater)
{
	bouwPlaneet();
}

void planeet::bouwPlaneet()
{
	//geodesisch zorgt ervoor dat we een boel punten krijgen, gesorteerd op nabijheid en met bijbehorende breedte- en lentegraden.

	//Daarna maken we een lijst van al deze driehoeken
	//Via deze lijst kunnen we per punt de buren bepalen, waarbij er 12 5 buren (de originele) hebben en de rest 6,
	maakLijstBuren();

	//We gaan alle nodige info in _vakken[0] wegschrijven, om het pingpongen te laten beginnen.
	_vakken[0].resize(_buren.size());
	_vakMetas .resize(_buren.size());
	
	//Ook moet ieder punt een lijstje meekrijgen met de buren waar ie bij hoort (dit is ook erg handig voor het tekenen van genoemde polygoon)
	//Alsin, dat ze naast een coordinaat nog 5 of 6 indices als vertex attribuut heeft
	//Een vertex attrib pointer kan maar max 4 velden hebben (ivec4) maar das niet erg.
	//Configuratie van [{{#buren, buur0, buur1, buur3}, {buur4, buur5, ( ? | buur6), ?}}, ...]
	//Dus twee aparte attribpointers maar zelfde buffer met stride 4
	burenAlsEigenschapWijzers();

	//Strijk de boel glad
	//for(size_t zoVaak = 0; zoVaak < 1; zoVaak++)
	//	browniaansLand();
	
	//Nu nog twee buffers toevoegen waarin ik kan gaan rekenen en als punteigenschapwijzers kan gebruiken.
	maakPingPongOpslagen();

	//Dan een geometrische shader maken die een polygoon tekent per punt. Of niet natuurlijk

}

void planeet::maakLijstBuren()
{
	_buren.resize(_punten->grootte());

	for(size_t i=0; i<_drieHk.size(); i+=3)
	{
		size_t 	v0	= _drieHk[ i   ],
				v1	= _drieHk[ i+1 ],
				v2	= _drieHk[ i+2 ];

		_buren[ v0 ].insert( v1 );
		_buren[ v0 ].insert( v2 );

		_buren[ v1 ].insert( v0 );
		_buren[ v1 ].insert( v2 );

		_buren[ v2 ].insert( v0 );
		_buren[ v2 ].insert( v1 );				
	}	
}

void planeet::burenAlsEigenschapWijzers()
{
	std::random_device willekeur;
	std::mt19937 gen(willekeur()); 
	std::normal_distribution<> wlkGrond(1.0, 0.05);

	size_t dezeIsNat = gen()%_buren.size();

	for(size_t i=0; i<_buren.size(); i++)
	{
		const auto & buurt = _buren[i];

		_vakMetas[i].burenAantal = buurt.size();

		assert(buurt.size() == 5 || buurt.size() == 6);

		size_t buur = 0;
		for(const uint32 & buurId : buurt)
			_vakMetas[i].buren[buur++] 	= buurId;

		_vakken[0][i].grondHoogte 	= _isRuis ? _ruis(_punten->ggvPunt3(i)) : _hoogteMonsteraar(_tex->ggvPunt2(i));
		if(_vakken[0][i].grondHoogte > _hoogsteGrond)
			_hoogsteGrond = _vakken[0][i].grondHoogte;

		//De planeet begint met een zanddeksel: een deklaag zand boven op de rots.
		//Waar die laag ligt is het oppervlak zand (maakPingPongOpslagen zet dat als
		//grondSoort); diepere ondergrond is rots, die 100x langzamer erodeert.
		const float zandDeklaag = 2.0f;
		_vakken[0][i].rotsHoogte = glm::clamp(_vakken[0][i].grondHoogte - zandDeklaag, minGrondHoogte, maxGrondHoogte);
		
		vec3 n = normalize(_punten->ggvPunt3(i));
		_vakMetas[i].normaal = vec4(n, 0.0f);

		gaHetKlokjeRondMetDeBuren(i);

		if(false)
		{
			float wijstIeNaarBoven = dot(n, glm::vec3(0.0f, 1.0f, 0.0f));
			const float vanafHier = 0.9, vanafDaar = 0.1, hoogte = 200;
			float polig = abs(wijstIeNaarBoven);

			if(polig > vanafHier)	_vakken[0][i].waterHoogte = hoogte * (polig - vanafHier) / (1.0f - 	vanafHier) ;
			//if(wijstIeNaarBoven < vanafDaar)	_vakken[0][i].waterHoogte = hoogte * (wijstIeNaarBoven  			/ 			vanafDaar) ;
		}
		else
		{
			_vakken[0][i].waterHoogte 	=  0.0f;
			_vakken[0][i].bodemVocht	=  1.0f;
			_vakken[0][i].luchtVocht	=  0.0f;
			_vakken[0][i].ijs			=  1.0f;
			_vakken[0][i].leven			=  0.0002f;
			_vakken[0][i].droesem		=  0.0f;
			_vakken[0][i].plek			= glm::vec2(0.0f);

			//Luchttoestand: evenwichtstemperatuur naar breedte (palen koud, evenaar
			//warm) en hoogte (lapse-rate), neutrale druk, stilstaande wind, geen wolken.
			//Mars begint koud: evenaar rond -20 °C (253.15 K).
			glm::vec3 wijst = glm::normalize(_punten->ggvPunt3(i));
			float breedte  = glm::clamp(wijst.y, -1.0f, 1.0f); //noordpool=+1
			float hoogteF  = glm::clamp((_vakken[0][i].grondHoogte - 10.0f) / (200.0f - 10.0f), 0.0f, 1.0f);
			_vakken[0][i].temperatuur = 253.15f - 20.0f * glm::abs(breedte) - 10.0f * hoogteF;
			_vakken[0][i].luchtdruk   = 1.0f;
			_vakken[0][i].wind        = glm::vec2(0.0f);
			_vakken[0][i].wolken      = 0.0f;
			_vakken[0][i].zonZicht    = 1.0f; //volle zon tot de schaduwkaart het tegendeel zegt
		}
	}	
}

void planeet::gaHetKlokjeRondMetDeBuren(size_t ID)
{
	using namespace std;
	using namespace glm;
	
	//Het zou mooi zijn als we de buren vanuit ons gezien in een rondje met de klok mee
	//Om dat te doen ga ik eerst de posities verzamelen:

	vec3 			midden;
	vector<vec3> 	buren;

	midden = _punten->ggvPunt3(ID);

	for(size_t i=0; i<_vakMetas[ID].burenAantal; i++)
		buren.push_back(_punten->ggvPunt3(_vakMetas[ID].buren[i]));


	struct sorteerDit
	{
		size_t 	buurNo;
		float 	hoek;
		vec2	buurRicht;
	};

	auto sorteerder = [](const sorteerDit & l, const sorteerDit & r)
	{
		return l.hoek < r.hoek;
	};

	
	vector<sorteerDit> sorteerDeze;

	//Gladde, grid-onafhankelijke tangentbasis uit de WARE rotatie-as (geografische
	//noordpool) i.p.v. uit de noordelijkste BUUR. De oude "noord"-richting (naar de
	//meest-noordelijke buur) volgde de icosahedron-structuur en gaf een grid-afwijking
	//in alle windrichtingen/drukgradiënten (zie de 5/6-voudige vlekken in druk en
	//wolkafbuiging). east = zonale/rotatierichting, noordT = raaklijn naar de noordpool.
	vec3 	omhoog	= _vakMetas[ID].normaal.xyz();
	vec3 	noordAs	= vec3(0.0f, 1.0f, 0.0f);
	vec3 	east	= normalize(cross(noordAs, omhoog));
	if(length(east) < 1.0e-4f)                 //pool-degeneratie: kies een richting loodrecht
		east = normalize(cross(vec3(0.0f, 0.0f, 1.0f), omhoog));
	vec3 	noordT	= normalize(cross(omhoog, east));
	_vakMetas[ID].oost  = vec4(east, 0.0f);
	_vakMetas[ID].noord = vec4(noordT, 0.0f);

	for(size_t i=0; i<_vakMetas[ID].burenAantal; i++)
	{
		vec3 relatief = normalize(buren[i] - midden);
		sorteerDeze.push_back(
			sorteerDit(
				_vakMetas[ID].buren[i], 
				atan2(dot(east, relatief), dot(noordT, relatief)),
				normalize(vec2(dot(east, relatief), dot(noordT, relatief)))
			)
		);
	}

	sort(sorteerDeze.begin(), sorteerDeze.end(), sorteerder);

	assert(sorteerDeze.size() == buren.size()				);
	assert(sorteerDeze.size() == _vakMetas[ID].burenAantal	);

	size_t buur = 0;
	for(auto & buurNoHoek : sorteerDeze)
	{
		_vakMetas[ID].buurRicht	[buur	] = buurNoHoek.buurRicht;
		_vakMetas[ID].buren		[buur++	] = buurNoHoek.buurNo;
	}

	//Least-squares gradient weights: trace-genormaliseerde correctiematrix.
	//M = 2·C⁻¹ / (trace(C⁻¹)·n) — corrigeert de directionele bias van het
	//onregelmatige grid (5 vs 6 buren, ongelijke afstanden) terwijl de
	//magnitude vergelijkbaar blijft met de oude "/ n" normalisatie.
	//Voor een regelmatige zeshoek: M = I/n, dus grad_new = raw/n = grad_old.
	//LET OP: C gebruikt LINEAIRE afstanden (|r|·dir·dirᵀ), niet kwadratische,
	//omdat de shader de GENORMALISEERDE richting (buurRicht) gebruikt:
	//raw = Σ(ΔP·dir) = Σ(|r|·(g·dir)·dir) voor een lineair veld.
	using glm::mat2;
	mat2 C(0.0f);
	size_t nBuren = _vakMetas[ID].burenAantal;
	float somAfstand = 0.0f;
	for(size_t i = 0; i < nBuren; i++)
	{
		size_t nb = sorteerDeze[i].buurNo;
		vec3 delta = _punten->ggvPunt3(nb) - midden;
		vec2 r = vec2(dot(east, delta), dot(noordT, delta));
		float dist = length(r);
		somAfstand += dist;
		if(dist > 1.0e-8f) {
			C[0][0] += r.x * r.x / dist;
			C[0][1] += r.x * r.y / dist;
			C[1][1] += r.y * r.y / dist;
		}
	}
	//De LS-gradient/divergentie uit de shader is O(gemiddelde buurafstand); met
	//2/d̄ als factor wordt dat de ware (diepte-onafhankelijke) waarde.
	_vakMetas[ID].gradSchaal = (somAfstand > 1.0e-8f) ? 2.0f * float(nBuren) / somAfstand : 0.0f;
	float det = C[0][0] * C[1][1] - C[0][1] * C[0][1];
	if(det > 1.0e-10f) {
		float invDet = 1.0f / det;
		float a = C[1][1] * invDet;
		float b = -C[0][1] * invDet;
		float c = C[0][0] * invDet;
		float trace = a + c;
		float scale = 2.0f / (trace * float(nBuren));
		_vakMetas[ID].gradWeights = glm::vec3(a * scale, b * scale, c * scale);
	} else {
		_vakMetas[ID].gradWeights = glm::vec3(0.0f);
	}

}

void planeet::browniaansLand()
{
	for(size_t i=0; i<_vakMetas.size(); i++)
	{
		vak 	& deze 		= _vakken[0][i];
		vakMeta & dezeMeta 	= _vakMetas[i];
		float 	burenHoogte = 0.0f;

		for(size_t i=0; i<dezeMeta.burenAantal; i++)
			burenHoogte += _vakken[0][dezeMeta.buren[i]].grondHoogte;

		deze.grondHoogte += burenHoogte / dezeMeta.burenAantal;
		deze.grondHoogte *= 0.5f;
	}
}

void planeet::maakPingPongOpslagen()
{
	std::random_device rd;  //Wordt gebruikt om het zaadje te planten
    std::mt19937 gen(rd()); 
    
	std::uniform_real_distribution<> dis(0.0, 1.0);

	for(size_t b=0; b<_buren.size(); b++)
	{
		float 	grondRand 	= dis(gen);
		int 	grondSoort	= 0;

		//Het oppervlak is rots waar geen deklaag boven zit, anders zand
		grondSoort = (_vakken[0][b].grondHoogte - _vakken[0][b].rotsHoogte > 0.0f) ? GS_ZAND : GS_ROTS;

//		vec2	lenBrdGr	= _tex->ggvPunt2(b);
//		bool	poolIjs		= (lenBrdGr.y < _poolA && dis(gen) > (lenBrdGr.y / _poolA)) || (lenBrdGr.y > _poolB && dis(gen) < (lenBrdGr.y - _poolB) / (1.0f - _poolB));

		/*if		(_vakken[0][b].grondHoogte > 0.8f || poolIjs)	grondSoort = GS_IJS;
		else*/  //if (_vakken[0][b].grondHoogte < 0.1f)				grondSoort = GS_ZAND;
		//else if	(grondRand < 0.0)							grondSoort = GS_GROND;
//		else if (_vakken[0][b].grondHoogte < 0.6f)				grondSoort = grondRand < (_vakken[0][b].grondHoogte - 0.1f) * 2.0 ? GS_ROTS : GS_ZAND;
	//	else													grondSoort = GS_ROTS;
	//	else if	(grondRand < 0.9)								grondSoort = GS_KLEI;
		//else if	(grondRand < 0.2)							grondSoort = GS_IJS;
	//	else													grondSoort = GS_LOESS;

		_vakken[0][b].grondSoort = grondSoort;
	}

	_vakken[1] = _vakken[0];

	//We gebruiken twee keer dezelfde data, want het wordt gekopieerd en de een wordt straks toch overschreven door de ander maar zo is iig de goeie grootte.
	_pingPongVakken[0] 	= new vrwrkrOpslagDing<vak>(	_vakken[0], 0);
	_pingPongVakken[1] 	= new vrwrkrOpslagDing<vak>(	_vakken[1], 1);
	_vakMetaOpslag 		= new vrwrkrOpslagDing<vakMeta>(_vakMetas, 	2);


	//We kunnen _pingPongOpslag straks wel allebei gebruiken om een SSB aan te binden en een compute shader tegenaan te gooien.
	//Voor nu maar gewoon zo houden denk ik.
}

void planeet::volgendeRonde()
{
	_pingIsDit	= 1 - _pingIsDit;

	bindVrwrkrOpslagen();
}

float planeet::buurAsymmetrie(size_t id) const
{
	using namespace glm;
	vec3 midden = _punten->ggvPunt3(id);
	vec3 som(0.0f);
	size_t n = _vakMetas[id].burenAantal;
	for(size_t k = 0; k < n && k < 6; k++)
	{
		vec3 d = _punten->ggvPunt3(_vakMetas[id].buren[k]) - midden;
		float dl = length(d);
		if(dl > 1.0e-8f)
			som += d / dl;
	}
	return n ? length(som) / float(n) : 0.0f;
}

void planeet::bindVrwrkrOpslagen(weergaveScherm & scherm)
{
	//In WebGPU krijgen de opslag-buffers hun plek in het reken/weergave-schema via het scherm
	scherm.verbindRekenBuffer(0, _pingPongVakken[    _pingIsDit]->opslag());
	scherm.verbindRekenBuffer(1, _pingPongVakken[1 - _pingIsDit]->opslag());
	scherm.verbindRekenBuffer(2, _vakMetaOpslag->opslag());
}
