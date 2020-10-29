#version 440

#define GS_ZAND		0
#define GS_GROND	1	//Ook wel humus of rijke grond
#define GS_ROTS		2
#define GS_KLEI		3
#define GS_IJS		4
#define GS_LOESS	5

#define KLEUR_ZAND	vec4(0.99, 	0.52, 	0.37, 	1.0)
#define KLEUR_GROND	vec4(0.16, 	0.13, 	0.1, 	1.0)
#define KLEUR_ROTS	vec4(0.2, 	0.2, 	0.2, 	1.0)
#define KLEUR_KLEI	vec4(0.416,	0.38, 	0.344, 	1.0)
#define KLEUR_IJS	vec4(0.8, 	0.8, 	0.8, 	1.0)
#define KLEUR_LOESS	vec4(0.78, 	0.75, 	0.74, 	1.0)


struct vak
{
	int		grondSoort	;
	float 	grondHoogte	;
	float	waterHoogte	;
	float	leven		;
	int		iets		;
	float	pijpen[6]	;
	uint	burenAantal	;
	uint	buren[6]	;
};

layout(location = 0) 		in 		vec3	pos;
layout(location = 1) 		in 		vec2	tex;
layout(std430, binding = 0) buffer 			ping 	{ vak vakken0[]; };
layout(std430, binding = 1) buffer 			pong 	{ vak vakken1[]; };


uniform mat4 modelView;
uniform mat4 projectie;
uniform uint grondNietWater;

// Om het probleem met het texture aan de achterkant van de planeet (een lelijke naad) op te lossen gebruik ik het idee wat beschreven wordt in:
// Cylindrical and Toroidal Parameterizations Without Vertex Seams door Marco Tarini (2012)
// Het basale idee ervan is de (fractionele) x/s texcoord tweemaal varying door te geven aan frag, zodat je de een kan pakken als de ander overduidelijk raar aan het doen is
// En overduidelijk is in dit geval als de afgeleide van de varying texcoord (via fwidth) kleiner is voor de een dan voor de ander. wat natuurlijk een prachtige oplossing is, bedankt Marco!

out NaarFrag
{
	out vec3 normal;
	out vec3 tex;
	out vec4 kleur;
	out float waterHoogte;
	out float grondHoogte;
} tc_in;

uniform sampler2D marsHoogte;

uniform float grondSchaal;
uniform float grondMult;

#define WATERMULT (1.0 / grondMult)

void main()
{
	
	vec4 grondKleur;

	switch(vakken0[gl_VertexID].grondSoort)
	{
	case GS_ZAND:		grondKleur	= KLEUR_ZAND;	break;
	case GS_GROND:		grondKleur	= KLEUR_GROND;	break;
	case GS_ROTS:		grondKleur	= KLEUR_ROTS;	break;
	case GS_KLEI:		grondKleur	= KLEUR_KLEI;	break;
	case GS_IJS:		grondKleur	= KLEUR_IJS;	break;
	case GS_LOESS:		grondKleur	= KLEUR_LOESS;	break;
	};

	const float waterSchaler = 10.0f;

	tc_in.tex			= vec3(tex.y, fract(tex.x), fract(tex.x + 0.5) - 0.5);
	float lokaalWater	= vakken0[gl_VertexID].waterHoogte > waterSchaler ? 1.0 : vakken0[gl_VertexID].waterHoogte / waterSchaler;
	tc_in.grondHoogte	= vakken0[gl_VertexID].grondHoogte;

	if(grondNietWater == 0)	tc_in.kleur			= vec4(0.0, 0.0, 1.0, 0.1 + (lokaalWater * 0.5));
	else					tc_in.kleur			= vec4(vakken0[gl_VertexID].grondHoogte);


	tc_in.normal		= (projectie * modelView * vec4(normalize(pos), 0.0)).xyz;

	float vertexHoogte = vakken0[gl_VertexID].grondHoogte;
	
	if(grondNietWater == 0)		vertexHoogte += (vakken0[gl_VertexID].waterHoogte * WATERMULT);
	//else						vertexHoogte += WATERMULT;

	tc_in.waterHoogte	= vakken0[gl_VertexID].waterHoogte + (vakken0[gl_VertexID].grondHoogte * grondMult);	
	gl_Position			= projectie * modelView * vec4(pos * (1.0 + (vertexHoogte * grondSchaal)), 1);	
}