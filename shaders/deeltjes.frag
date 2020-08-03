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
	//vec3 modded = mod(round(fs_in.hexIndex), 2);

	//kleur = vec4(modded.x, modded.y - modded.x, modded.z - modded.y, 1);

	//kleur = vec4(mod(round(fs_in.hexIndex), 3) * 0.5, 1);

	//float closest = min(fs_in.hexIndex.x, min(fs_in.hexIndex.y, fs_in.hexIndex.z));
	vec3 coords = floor(fs_in.hexIndex * 6.0);

	if(coords.x == coords.y || coords.z == coords.x || coords.y == coords.z)
		kleur = vec4(1);
	else
		kleur = vec4(coords / 6, 1);

	/*
	vec4 mola = texture(marsHoogte, fs_in.tex);
	kleur = mola * vec4(fs_in.kleur, 1);

	if(mola.r < 0.2)
		kleur = mola * vec4(0, 0, 1, 1.0);*/
}