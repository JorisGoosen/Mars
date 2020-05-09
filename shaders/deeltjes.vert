#version 440

layout(location = 0) in vec3 vPos;

uniform mat4 projectie;
uniform mat4 modelView;

out VS_GS_VERTEX
{
	out vec3 normal;
} vs_out;

void main()
{
	vs_out.normal = normalize(vPos);

	gl_Position =// projectie * 
	//mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
	projectie * 
	//mat4(1) * 
	modelView * 
	vec4(vPos, 1.0);// * modelView;// * ;
}