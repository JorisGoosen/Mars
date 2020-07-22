#version 440

out vec4 kleur;

in TS_FS_VERTEX
{
	in vec3 normal;
	in vec3 kleur;
	in vec2 tex;
} fs_in;


uniform sampler2D marsHoogte;
uniform mat4 modelView;
uniform float groente;

float berekenLicht()
{
	//const vec3	zonpos 	= (modelView * vec4(3, 1, 3, 0)).xyz;
	const vec3 zonpos = vec3(3, 0, 3);
//	const vec4 	aardpos	= (vec4(0, 0,  0, 1) * modelView);

//	const vec3 richtZon = zonpos - (aardpos.xyz / aardpos.w);
	return dot(fs_in.normal, normalize(zonpos));
}


void main()
{
	
	vec4 mola = texture(marsHoogte, fs_in.tex);
	//col = vec4((vec3(1) + fs_in.normal) / vec3(2), 1);
	kleur = vec4(1, 0, 0, 1);// vec4(fs_in.kleur, 1);

	const float blauw = min(groente, 0.16), groen = max(groente, blauw), wit = 1.0;

	const float licht =  berekenLicht();

	if(mola.r < blauw) 		kleur = mola * vec4(0, 0, 2, 1.0)  * licht;
	else if(mola.r < groen)	
	{
		float groenrood 		= pow( 	min((mola.r - blauw) / (groen - blauw), 1), 0.5);

		kleur = mix(vec4(0, 1, 0, 1), vec4(1, 0, 0, 1), min(groenrood, 1) ) * licht;
	}
	else if(mola.r < wit)
	{
		float witrood			= min((mola.r - groen) / (wit  - groen),  1);

		kleur = mix(vec4(1, 0, 0, 1), vec4(1, 0.7, 0.7, 1), min(witrood, 1) ) * licht;
	}
}