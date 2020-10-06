#version 440

layout(location = 0) in vec3 vPos;


uniform mat4 modelView;
uniform mat4 projectie;

out NaarFrag
{
	out vec3 normal;
	out vec2 tex;
} tc_in;

vec2 longitudeLatitude(vec2 normalXZ)
{
	float 	latitude 	= (normalXZ.y + 1.0) * 0.5;
	
	vec2	angleVec	= normalize(normalXZ);
		
	return vec2(atan(angleVec.x, angleVec.y), latitude);

}

void main()
{
	tc_in.normal	= normalize(vPos);
	tc_in.tex		= longitudeLatitude(tc_in.normal.xz);
	gl_Position		= projectie * modelView * vec4(vPos, 1);	
}