#version 440

layout(location = 0) in vec3	pos;
layout(location = 1) in vec2	tex;
layout(location = 2) in uvec4 	burenA; // #, 0, 1, 2
layout(location = 3) in uvec4 	burenB; // 3, 4, 5, willekeurig   
layout(location = 4) in vec4 	ping;
layout(location = 5) in vec4 	pong;


uniform mat4 modelView;
uniform mat4 projectie;

out NaarFrag
{
	out vec3 normal;
	out vec2 tex;
	out vec4 kleur;
} tc_in;

void main()
{
	const vec3 mults = vec3(33, 61, 12);

	tc_in.normal	= normalize(pos);
	tc_in.tex		= tex;
	tc_in.kleur		= vec4(sin(tex.x * 3.142 * mults.x), sin(tex.y * 3.142 * mults.y), sin((tex.x + tex.y) * 3.142 * mults.z), 1) ;
	gl_Position		= projectie * modelView * vec4(pos, 1);	
}