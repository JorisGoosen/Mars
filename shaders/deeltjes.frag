#version 440

out vec4 kleur;

in TS_FS_VERTEX
{
	in vec3 normal;
	in vec3 kleur;
} fs_in;


uniform sampler2D marsHoogte;

void main()
{
	//col = vec4((vec3(1) + fs_in.normal) / vec3(2), 1);
	kleur = texture(marsHoogte, fs_in.normal.xy);//vec4(fs_in.kleur, 1);
}