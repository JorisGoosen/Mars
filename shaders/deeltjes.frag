#version 440

out vec4 kleur;

in TS_FS_VERTEX
{
	in vec3  normal;
	in vec3  kleur;
	in vec2  tex;
	in vec3 hexIndex;
} fs_in;


uniform sampler2D marsHoogte;

void main()
{
	const int hexParts = 9;

	vec3 p = fs_in.hexIndex * hexParts;
	
	float dist = length(p - round(p));

	if(dist > 1) dist = 0;
	kleur = vec4(1-dist);

	

	

	/*da original
	vec4 mola = texture(marsHoogte, fs_in.tex);
	kleur = mola * vec4(fs_in.kleur, 1);

	if(mola.r < 0.2)
		kleur = mola * vec4(0, 0, 1, 1.0);*/	
}