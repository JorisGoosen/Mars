#version 440

layout(location = 0) in vec3 vPos;


out vec3 normal;


out VS_TS_VERTEX
{
	out vec3 normal;
	out vec3 pos;
} ts_in;

void main()
{
	ts_in.normal	= normalize(vPos);
	ts_in.pos 		= vPos;
}