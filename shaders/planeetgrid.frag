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
uniform uint grondNietWater;

void main()
{
	//if(grondNietWater == 0 && fs_in.waterHoogte < 0.0001) discard;

	//Naadloos tex is op basis van in vertshader genoemd algoritme door Tarini
	vec2 naadloosTex = vec2(fwidth(fs_in.tex.y) <= fwidth(fs_in.tex.z) + 0.000001 ? fs_in.tex.y : fs_in.tex.z, fs_in.tex.x);

	const vec4 marsKleur = vec4(1, 0.5, 0.5, 1);
	
	float marsHoogte = texture(marsHoogte, naadloosTex).x;// + fs_in.grondHoogte) * 0.5;

	//if(marsHoogte > 0.3)	
	//kleur = fs_in.kleur;
	//else

	const vec2 	lichtGradient = normalize(vec2(1, 1));
	vec2 		gradient;
	float 		lichtheid = 0.3;
	

	if(grondNietWater == 1)
	{
	//	gradient 	 = vec2(dFdx(fs_in.grondHoogte), dFdy(fs_in.grondHoogte));
	//	lichtheid 	*= dot(lichtGradient, normalize(gradient));

		kleur = vec4(marsHoogte, marsHoogte * 0.5, 0.0, 1.0);// mix(, marsKleur, lichtheid);
	}
	else
	{
		gradient 	 = vec2(dFdx(fs_in.waterHoogte), dFdy(fs_in.waterHoogte));
		lichtheid 	*= dot(lichtGradient, normalize(gradient));

		kleur = mix(fs_in.kleur, vec4(0.75, 0.75, 1.0, 1.0), lichtheid) * vec4(1, 1, 1, 0.75);
	}
/*
	//Beter visuele check inbouwen voor lager dan nul water
	if(fs_in.waterHoogte < 0.0)			kleur = vec4(1, 1, 0, 1);
	if(fs_in.waterHoogte > 10000.0)		kleur = vec4(0, 1, 0, 1);
	else								kleur = vec4(marsHoogte, marsHoogte * 0.5, fs_in.waterHoogte, 1.0);*/
}
	