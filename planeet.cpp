#include "planeet.h"
#include <random>



using namespace glm;

planeet::planeet(size_t onderverdelingen, std::function<float(glm::vec2)> hoogteMonsteraar) : geodesisch(onderverdelingen), _hoogteMonsteraar(hoogteMonsteraar)
{
	//geodesisch zorgt ervoor dat we een boel punten krijgen, gesorteerd op nabijheid en met bijbehorende breedte- en lentegraden.

	//Daarna maken we een lijst van al deze driehoeken
	//Via deze lijst kunnen we per punt de buren bepalen, waarbij er 12 5 buren (de originele) hebben en de rest 6,
	maakLijstBuren();

	//We gaan alle nodige info in _vakjes[0] wegschrijven, om het pingpongen te laten beginnen.
	_vakjes[0].resize(_buren.size()); 
	
	//Ook moet ieder punt een lijstje meekrijgen met de buren waar ie bij hoort (dit is ook erg handig voor het tekenen van genoemde polygoon)
	//Alsin, dat ze naast een coordinaat nog 5 of 6 indices als vertex attribuut heeft
	//Een vertex attrib pointer kan maar max 4 velden hebben (ivec4) maar das niet erg.
	//Configuratie van [{{#buren, buur0, buur1, buur3}, {buur4, buur5, ( ? | buur6), ?}}, ...]
	//Dus twee aparte attribpointers maar zelfde buffer met stride 4
	burenAlsEigenschapWijzers();
	
	//Nu nog twee buffers toevoegen waarin ik kan gaan rekenen en als punteigenschapwijzers kan gebruiken.
	maakPingPongOpslagen();

	//Dan een geometrische shader maken die een polygoon tekent per punt

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

		_vakjes[0][i].burenAantal = buurt.size();

		assert(buurt.size() == 5 || buurt.size() == 6);

		size_t buur = 0;
		for(const uint32 & buurId : buurt)
			_vakjes[0][i].buren[buur++] = buurId;

		_vakjes[0][i].iets 			= gen()%2048;
		_vakjes[0][i].grondHoogte 	= _hoogteMonsteraar(_tex->ggvPunt2(i));

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

		_vakjes[0][b].grondSoort = grondSoort;
	}

	_vakjes[1] = _vakjes[0];

	//We gebruiken twee keer dezelfde data, want het wordt gekopieerd en de een wordt straks toch overschreven door de ander maar zo is iig de goeie grootte.
	_pingPong[0] = new vrwrkrOpslagDing<vakje>(_vakjes[0], 0);
	_pingPong[1] = new vrwrkrOpslagDing<vakje>(_vakjes[1], 1);


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
	_pingPong[    _pingIsDit]->zetKnooppunt(0);
	_pingPong[1 - _pingIsDit]->zetKnooppunt(1);
}
