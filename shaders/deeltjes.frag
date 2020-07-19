#version 440

out vec4 kleur;

in TS_FS_VERTEX
{
	in vec3 normal;
	in vec3 kleur;
	in vec2 tex;
} fs_in;


uniform sampler2D marsHoogte;

void main()
{
	
	vec4 mola = texture(marsHoogte, fs_in.tex);
	//col = vec4((vec3(1) + fs_in.normal) / vec3(2), 1);
	kleur = mola * vec4(fs_in.kleur, 1);

	if(mola.r < 0.2)
		kleur = mola * vec4(0, 0, 1, 1.0);
}