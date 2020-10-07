#version 440

layout(location = 0) in vec3	pos;
layout(location = 1) in uvec4 	burenA; // #, 0, 1, 2
layout(location = 2) in uvec4 	burenB; // 3, 4, 5, willekeurig   


uniform mat4 modelView;
uniform mat4 projectie;

out NaarFrag
{
	out vec3 normal;
	out vec2 tex;
	out vec4 kleur;
} tc_in;

vec2 longitudeLatitude(vec2 normalXZ)
{
	float 	latitude 	= (normalXZ.y + 1.0) * 0.5;
	
	vec2	angleVec	= normalize(normalXZ);
		
	return vec2(atan(angleVec.x, angleVec.y), latitude);

}

void main()
{
	tc_in.normal	= normalize(pos);
	tc_in.tex		= longitudeLatitude(tc_in.normal.xz);
	tc_in.kleur		= burenB.w == 0 ? vec4(1,0,0,1) : burenB.w == 1 ? vec4(0,1,0,1) : vec4(0,0,1,1);
	//tc_in.kleur		= burenA.x > 5 ? vec4(1,0,0,1) : vec4(0,0,1,1);
	gl_Position		= projectie * modelView * vec4(pos, 1);	
}