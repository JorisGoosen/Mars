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

/*float berekenLicht(vec3 pos)
{
	const vec3	zonpos 	= vec4(3, 1, -3, 1);
	const vec4 	aardpos	= (vec4(0, 0,  0, 1) * modelView);

	const vec3 richtZon = zonpos - (aardpos.xyz / aardpos.w);
	return dot(fs_in.normal, richtZon);
}*/


void main()
{
	
	vec4 mola = texture(marsHoogte, fs_in.tex);
	//col = vec4((vec3(1) + fs_in.normal) / vec3(2), 1);
	kleur = mola * vec4(fs_in.kleur, 1);

	const float groen = 1.0, blauw = 0.2, geel = 0.7;
	if(mola.r < blauw) 		kleur = mola * vec4(0, 0, 1, 1.0);
	else if(mola.r < groen)	
	{
		float groenrood 		= pow( min((mola.r - blauw) / (groen - blauw), 1), 0.5);
	//	float geelrood			= min((mola.r - groen) / (geel  - blauw), 1);

		kleur = mix(mola * vec4(0, 1, 0, 1), vec4(1, 0, 0, 1), min(groenrood, 1) );
	}
}