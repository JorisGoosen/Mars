#version 440

layout(location = 0) in vec3 vPos;


out VS_GS_VERTEX
{
	out vec3 normal;
} vs_out;

void main()
{
	vs_out.normal = vPos;//normalize(vec4(vPos, 0) * modelView).xyz;

	//gl_Position = projectie * modelView * vec4(vPos, 1.0);
	gl_Position = vec4(vPos, 1.0);
}