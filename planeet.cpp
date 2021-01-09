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
	for(size_t zoVaak = 0; zoVaak < 1; zoVaak++)
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
			_vakken[0][i].vocht			= 70.0f;
			_vakken[0][i].droesem		=  0.0f;
			_vakken[0][i].plek			= glm::vec2(0.0f);
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

	//Bepaal de buur die het meeste naar het noorden ligt, gaan we vanaf daar buren bepalen
	vec3 	noordPool 	= vec3(0, 1, 0);
	float 	meestNoord	= length(noordPool - midden);
	int		buurNoord	= -1; //oftwel midden

	for(size_t i=0; i<buren.size(); i++)
	{
		float mijnNoord = length(noordPool - buren[i]);

		if(mijnNoord > meestNoord)
		{
			meestNoord 	= mijnNoord;
			buurNoord	= i;
		}
	}

	vec3 	omhoog	= _vakMetas[ID].normaal.xyz(),
			noord 	= buurNoord != -1 ? normalize(buren[buurNoord] -  midden) : vec3(0.0f, 1.0f, 0.0f),
			west	= cross(noord, omhoog);

//	std::cout << "noord: " << noord << "\twest: " << west << "\tomhoog: " << omhoog << std::endl;
	
	for(size_t i=0; i<_vakMetas[ID].burenAantal; i++)
	{
		vec3 relatief = normalize(buren[i] - midden);
		sorteerDeze.push_back(
			sorteerDit(
				_vakMetas[ID].buren[i], 
				acos(dot(noord, relatief)),
				normalize(vec2(dot(west, relatief), dot(noord, relatief)))
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

		grondSoort = GS_ROTS;

//		vec2	lenBrdGr	= _tex->ggvPunt2(b);
//		bool	poolIjs		= (lenBrdGr.y < _poolA && dis(gen) > (lenBrdGr.y / _poolA)) || (lenBrdGr.y > _poolB && dis(gen) < (lenBrdGr.y - _poolB) / (1.0f - _poolB));

		/*if		(_vakken[0][b].grondHoogte > 0.8f || poolIjs)	grondSoort = GS_IJS;
		else*/ if (_vakken[0][b].grondHoogte < 0.1f)				grondSoort = GS_ZAND;
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

void planeet::bindVrwrkrOpslagen()
{
	_pingPongVakken[    _pingIsDit]	->zetKnooppunt(0);
	_pingPongVakken[1 - _pingIsDit]	->zetKnooppunt(1);
	_vakMetaOpslag					->zetKnooppunt(2);
}
