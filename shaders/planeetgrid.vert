#version 440

#define KLEUR_ZAND	vec4(0.7, 	0.52, 	0.3, 	1.0)
#define KLEUR_GROND	vec4(0.16, 	0.13, 	0.1, 	1.0)
#define KLEUR_ROTS	vec4(0.5, 	0.4, 	0.2, 	1.0)
#define KLEUR_KLEI	vec4(0.416,	0.38, 	0.344, 	1.0)
#define KLEUR_IJS	vec4(0.8, 	0.8, 	0.8, 	1.0)
#define KLEUR_LOESS	vec4(0.78, 	0.75, 	0.74, 	1.0)

#include "planeetDefinities.glsl"

layout(location = 0) in	vec3 posV;
layout(location = 1) in	vec2 tex;



uniform mat4 modelView;
uniform mat4 transInvMV;
uniform mat4 projectie;
uniform uint grondNietWater;

// Om het probleem met het texture aan de achterkant van de planeet (een lelijke naad) op te lossen gebruik ik het idee wat beschreven wordt in:
// Cylindrical and Toroidal Parameterizations Without Vertex Seams door Marco Tarini (2012)
// Het basale idee ervan is de (fractionele) x/s texcoord tweemaal varying door te geven aan frag, zodat je de een kan pakken als de ander overduidelijk raar aan het doen is
// En overduidelijk is in dit geval als de afgeleide van de varying texcoord (via fwidth) kleiner is voor de een dan voor de ander. wat natuurlijk een prachtige oplossing is, bedankt Marco!

out NaarFrag
{
	out vec3 normaal;
	out vec3 tex;
	out vec4 kleur;
	out float waterHoogte;
	out float grondHoogte;
	out float snelheid;
	out vec4 pos;
	out float leven;
} tc_in;

uniform sampler2D marsHoogte;

uniform float grondSchaal;
uniform float grondMult;


#define WATERMULT (1.0 / grondMult)

uint buurID(uint buur)
{
	return vakMetas[gl_VertexID].buren[buur];
}

float hoogteBuur(uint buurID)
{
	return vakken0[buurID].grondHoogte + (grondNietWater == 0 ? vakken0[buurID].waterSchijn * WATERMULT : 0.0f);
}

vec3 vakPlek(uint buurID, float hoogte)
{
	vec3 n;
	for(uint i=0; i<3; i++)
		n[i] = vakMetas[buurID].normaal[i];

	return (n * (1.0 + (hoogte * grondSchaal)));
}

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

	const float waterSchaler = 0.1 * grondMult;

	tc_in.tex			= vec3(tex.y, fract(tex.x), fract(tex.x + 0.5) - 0.5);
	float lokaalWater	= vakken0[gl_VertexID].waterHoogte > waterSchaler ? 1.0 : vakken0[gl_VertexID].waterHoogte / waterSchaler;
	tc_in.grondHoogte	= vakken0[gl_VertexID].grondHoogte;

	if(grondNietWater == 0)	tc_in.kleur	= vec4(0.0, 0.0, 0.4, 0.3 + (lokaalWater * 0.7));
	else					tc_in.kleur = grondKleur; //tc_in.kleur	= vec4(vec3( vakken0[gl_VertexID].grondHoogte), 1.0f); //vec4(vakken0[gl_VertexID].grondHoogte); // grondKleur;
	
	vec3 n;

	for(uint i=0; i<3; i++)
		n[i] = vakMetas[gl_VertexID].normaal[i];

	uint 	burenAantal 			= vakMetas[gl_VertexID].burenAantal;
	vec3	hier					= vakPlek(gl_VertexID, hoogteBuur(gl_VertexID));

	hier *= sign(dot(posV, hier));

	const bool doeNormaal = false;
	if(doeNormaal)
	{	
		tc_in.normaal 		= n;
	}
	else
	{
		uint	buurIdTmp;
		vec3 	buurPos[6],
				kruis 			= vec3(0.0); 
	
		
		for(uint p=0; p<burenAantal; p++)
		{
			buurIdTmp  				 = buurID(p);
			buurPos[p] 				 = vakPlek(buurIdTmp, hoogteBuur(buurIdTmp));
		}
	
		for(uint i=0; i<burenAantal; i++)
		{
			vec3 tussen = cross(buurPos[i] - hier, buurPos[(i+1)%burenAantal] - hier);
			kruis += tussen * sign(dot(tussen, hier));
		}

		tc_in.normaal = kruis;
	}

	tc_in.normaal		= normalize((transInvMV * vec4(tc_in.normaal, 0.0f)).xyz);
	tc_in.leven			= vakken0[gl_VertexID].leven;
	tc_in.snelheid		= vakken0[gl_VertexID].snelheid;
	tc_in.waterHoogte	= vakken0[gl_VertexID].waterSchijn; //hoogteBuur(gl_VertexID);
	tc_in.pos			= modelView * vec4(hier, 1.0f);
	gl_Position			= projectie * tc_in.pos;
}