#version 440

out vec4 kleur;

in NaarFrag
{
	in vec3 normal;
	in vec3 tex; //x = u! y = s0 en z = s1
	in vec4 kleur;
	in float waterHoogte;
	in float grondHoogte;
} fs_in;


uniform sampler2D marsHoogte;

void main()
{
	//Naadloos tex is op basis van in vertshader genoemd algoritme door Tarini
	vec2 naadloosTex = vec2(fwidth(fs_in.tex.y) <= fwidth(fs_in.tex.z) + 0.000001 ? fs_in.tex.y : fs_in.tex.z, fs_in.tex.x);

	const vec4 marsKleur = vec4(1, 0.5, 0.5, 1);
	
	float marsHoogte = fs_in.grondHoogte;// (texture(marsHoogte, naadloosTex).x + fs_in.grondHoogte) * 0.5;

	//if(marsHoogte > 0.3)	
	//kleur = fs_in.kleur;
	//else

	//Beter visuele check inbouwen voor lager dan nul water
	if(fs_in.waterHoogte < 0.0)		kleur = vec4(1, 0, 1, 1);
	else							kleur = vec4(marsHoogte, marsHoogte * 0.5, fs_in.waterHoogte > 0.5 ? 1.0 : 0.0, 1.0);
}
	