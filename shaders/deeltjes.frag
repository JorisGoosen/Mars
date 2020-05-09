#version 440

out vec4 col;

in GS_FS_VERTEX
{
	in vec3 normal;
} fs_in;

void main()
{
	col = vec4((vec3(1) + fs_in.normal) / vec3(2), 1);
}