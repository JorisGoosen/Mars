#version 440

out vec4 kleur;

in NaarFrag
{
	in vec3 normal;
	in vec3 tex; //x = u! y = s0 en z = s1
	in vec4 kleur;
} fs_in;


uniform sampler2D marsHoogte;

void main()
{
	//Naadloos tex is op basis van in vertshader genoemd algoritme door Tarini
	vec2 naadloosTex = vec2(fwidth(fs_in.tex.y) <= fwidth(fs_in.tex.z) + 0.000001 ? fs_in.tex.y : fs_in.tex.z, fs_in.tex.x);

	const vec4 marsKleur = vec4(1, 0.5, 0.5, 1);
	
	float marsHoogte = texture(marsHoogte, naadloosTex).x;

	//if(marsHoogte > 0.3)	
	//kleur = fs_in.kleur;
	//else
	kleur = marsKleur * marsHoogte;
}
	