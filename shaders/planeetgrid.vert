#version 440

layout(location = 0) in vec3	pos;
layout(location = 1) in vec2	tex;
layout(location = 2) in uvec4 	burenA; // #, 0, 1, 2
layout(location = 3) in uvec4 	burenB; // 3, 4, 5, willekeurig   
layout(location = 4) in vec4 	ping;
layout(location = 5) in vec4 	pong;


uniform mat4 modelView;
uniform mat4 projectie;

// Om het probleem met het texture aan de achterkant van de planeet (een lelijke naad) op te lossen gebruik ik het idee wat beschreven wordt in:
// Cylindrical and Toroidal Parameterizations Without Vertex Seams door Marco Tarini (2012)
// Het basale idee ervan is de (fractionele) x/s texcoord tweemaal varying door te geven aan frag, zodat je de een kan pakken als de ander overduidelijk raar aan het doen is
// En overduidelijk is in dit geval als de afgeleide van de varying texcoord (via fwidth) kleiner is voor de een dan voor de ander. wat natuurlijk een prachtige oplossing is, bedankt Marco!

out NaarFrag
{
	out vec3 normal;
	out vec3 tex;
	out vec4 kleur;
} tc_in;

void main()
{
	const vec3 mults = vec3(33, 61, 12);

	tc_in.normal	= normalize(pos);
	tc_in.tex		= vec3(tex.y, fract(tex.x), fract(tex.x + 0.5) - 0.5);
	tc_in.kleur		= ping;
	gl_Position		= projectie * modelView * vec4(pos, 1);	
}