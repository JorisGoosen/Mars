#version 440

layout(location = 0) in vec3 vPos;

out VS_TC_VERTEX
{
	out vec3 normal;
	out vec3 pos;
} tc_in;

void main()
{
	tc_in.normal	= normalize(vPos);
	tc_in.pos 		= vPos;
}