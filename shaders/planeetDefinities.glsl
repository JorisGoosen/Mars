#define GS_ZAND		0
#define GS_GROND	1	//Ook wel humus of rijke grond
#define GS_ROTS		2
#define GS_KLEI		3
#define GS_IJS		4
#define GS_LOESS	5

struct vak
{
	int		grondSoort	;
	float 	grondHoogte	,
			waterHoogte	,
			waterSchijn	,
			vocht		,
			leven		,
			snelheid	,
			pijpen[6]	;
};

struct vakMetasS
{
	vec4	normaal			;
	vec2	buurAfzet	[6]	;
	uint	buren		[6]	;
	uint	burenAantal		;
	uint 	opvulling		;
};

layout(std430, binding = 0) buffer	pingVak { vak 		vakken0	[]; };
layout(std430, binding = 1) buffer	pongVak { vak 		vakken1	[]; };
layout(std430, binding = 2) buffer	vakInfo	{ vakMetasS	vakMetas[];	};
