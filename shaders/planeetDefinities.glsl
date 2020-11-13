#define GS_ZAND		0
#define GS_GROND	1	//Ook wel humus of rijke grond
#define GS_ROTS		2
#define GS_KLEI		3
#define GS_IJS		4
#define GS_LOESS	5

#define TIJD_VERSCHIL 	0.1
#define ZWAARTEKRACHT 	0.8
#define PIJP_DOORSNEE 	0.75
#define PIJP_LENGTE		1.0
#define OPLOSHEID		0.01
#define BEZINKHEID		0.05
#define DROESEMHEID		0.1

struct vak
{
	int		grondSoort	;
	float 	grondHoogte	,
			waterHoogte	,
			waterSchijn	,
			vocht		,
			ijs			,
			leven		,
			droesem		,
			pijpen[6]	;
	vec2	snelheid	;
};

struct vakMetasS
{
	vec4	normaal			;
	vec2	buurRicht	[6]	;
	uint	buren		[6]	;
	uint	burenAantal		;
	uint 	opvulling		;
};

layout(std430, binding = 0) buffer	pingVak { vak 		vakken0	[]; };
layout(std430, binding = 1) buffer	pongVak { vak 		vakken1	[]; };
layout(std430, binding = 2) buffer	vakInfo	{ vakMetasS	vakMetas[];	};

uniform float 	grondSchaal;
uniform float 	grondMult;
uniform float	verdamping;
uniform vec3	regenPlek;


#define WATERMULT (1.0 / grondMult)

uint buurID(uint buur)
{
	return vakMetas[ID].buren[buur];
}

float hoogteBuur(uint buurID, bool water)
{
	return vakken0[buurID].grondHoogte + (water ? vakken0[buurID].waterSchijn * WATERMULT + vakken0[buurID].droesem : 0.0f);
}

vec3 vakHoogte(uint id, bool water)
{
	return (vakMetas[id].normaal.xyz * (1.0 + (hoogteBuur(id, water) * grondSchaal)));
}

vec3 berekenNormaal(uint id, bool water, vec3 hint)
{
	vec3 	n 			 = vakMetas[ID].normaal.xyz,
			hier		 = vakHoogte(ID, water);
	//		hier 		*= sign(dot(hint, hier));
	uint 	burenAantal  = vakMetas[id].burenAantal,
			buurIdTmp;
	vec3 	buurPos[6],
			kruis 			= vec3(0.0); 

	
	for(uint p=0; p<burenAantal; p++)
	{
		buurIdTmp  				 = vakMetas[id].buren[p];
		buurPos[p] 				 = vakHoogte(buurIdTmp, water);
	}

	for(uint i=0; i<burenAantal; i++)
	{
		vec3 tussen = cross(buurPos[i] - hier, buurPos[(i+1)%burenAantal] - hier);
		kruis += tussen * sign(dot(tussen, hier));
	}

	return normalize(kruis);
}
