#include "planeet.h"
#include <random>
#include <iostream>


using namespace glm;

planeet::planeet(size_t onderverdelingen, std::function<float(glm::vec3)> ruis) : geodesisch(onderverdelingen), _ruis(ruis), _isRuis(true)
{
	bouwPlaneet();
}

planeet::planeet(size_t onderverdelingen, std::function<float(glm::vec2)> hoogteMonsteraar) : geodesisch(onderverdelingen), _hoogteMonsteraar(hoogteMonsteraar), _isRuis(false)
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
	for(size_t zoVaak = 5; zoVaak > 0; zoVaak--)
		browniaansLand();
	
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
	//_eigenschappen.reserve(_buren.size() * 8); //De grootte is eigenlijk _buren.size() * 6 - 12 maar ik denk dat het beter is voor de gpu om ze allebei een vec4 te geven, anders accepteert ie misschien wel, maar...
	
	std::random_device willekeur;
	std::mt19937 gen(willekeur()); 
	std::normal_distribution<> wlkGrond(1.0, 0.05);

	for(size_t i=0; i<_buren.size(); i++)
	{
		const auto & buurt = _buren[i];

		_vakMetas[i].burenAantal = buurt.size();

		assert(buurt.size() == 5 || buurt.size() == 6);

		size_t buur = 0;
		for(const uint32 & buurId : buurt)
			_vakMetas[i].buren[buur++] 	= buurId;

		gaHetKlokjeRondMetDeBuren(i);

		_vakken[0][i].iets 			= gen()%2048;
		_vakken[0][i].grondHoogte 	= _isRuis ? _ruis(_punten->ggvPunt3(i)) : _hoogteMonsteraar(_tex->ggvPunt2(i));
		
		glm::vec3 n = normalize(_punten->ggvPunt3(i));
		for(size_t ii=0; ii<3; ii++)
			_vakMetas[i].normaal[ii] = n[ii];

		if(gen()%20000 == 0)
			_vakken[0][i].waterHoogte = 100000;
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

	auto sorteerder = [](const pair<size_t, float> & l, const pair<size_t, float> & r)
	{
		return l.second < r.second;
	};

	// [ <buurNo, hoek>, ... ]
	vector<pair<size_t, float>> sorteerDit;

	//Bepaal de buur die dichtste bij het noorden ligt, gaan we vanaf daar buren bepalen
	vec3 	noordPool 	= vec3(0, 1, 0);
	float 	meestNoord	= length(normalize(midden) - noordPool);
	int		buurNoord	= -1; //oftwel midden

	for(size_t i=0; i<buren.size(); i++)
	{
		float mijnNoord = length(normalize(buren[i]) - noordPool);
		if(mijnNoord < meestNoord)
		{
			meestNoord 	= mijnNoord;
			buurNoord	= i;
		}
	}

	vec3 	noord = vec3(1.0f, 0.0f, 0.0f);

	if(buurNoord != -1)
		noord = normalize(buren[buurNoord] -  midden);

	for(size_t i=0; i<_vakMetas[ID].burenAantal; i++)
	{
		vec3 relatief = normalize(buren[i] - midden);
		sorteerDit.push_back(make_pair(_vakMetas[ID].buren[i], -acos(dot(noord, relatief))));
	}

	sort(sorteerDit.begin(), sorteerDit.end(), sorteerder);

	assert(sorteerDit.size() == buren.size()				);
	assert(sorteerDit.size() == _vakMetas[ID].burenAantal	);

	size_t buur = 0;
	for(auto & buurNoHoek : sorteerDit)
		_vakMetas[ID].buren[buur++] = buurNoHoek.first;
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

		vec2	lenBrdGr	= _tex->ggvPunt2(b);
		bool	poolIjs		= lenBrdGr.y < _poolA || lenBrdGr.y > _poolB;

		if		(poolIjs)				grondSoort = GS_IJS;
		else if (grondRand < 0.5)		grondSoort = GS_ZAND;
		//else if	(grondRand < 0.0)		grondSoort = GS_GROND;
		else if	(grondRand < 0.85)		grondSoort = GS_ROTS;
		else if	(grondRand < 0.9)		grondSoort = GS_KLEI;
		//else if	(grondRand < 0.2)		grondSoort = GS_IJS;
		else							grondSoort = GS_LOESS;

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

void planeet::bindVrwrkrOpslagen()
{
	_pingPongVakken[    _pingIsDit]	->zetKnooppunt(0);
	_pingPongVakken[1 - _pingIsDit]	->zetKnooppunt(1);
	_vakMetaOpslag					->zetKnooppunt(2);
}
