#version 440

out vec4 kleur;

in NaarFrag
{
	in vec3 	normaal;
	in vec3 	tex; //x = u! y = s0 en z = s1
	in vec4 	kleur;
	in float 	waterHoogte;
	in float 	grondHoogte;
	in float 	snelheid;
	in vec4 	pos;
} fs_in;


uniform sampler2D 	marsHoogte;
uniform uint 		grondNietWater;
uniform vec3 		kijkPlek;
uniform vec3 		zonPos;

void main()
{
	if(grondNietWater == 0 && fs_in.waterHoogte < 0.15) discard;

	//Naadloos tex is op basis van in vertshader genoemd algoritme door Tarini
	vec2 naadloosTex = vec2(fwidth(fs_in.tex.y) <= fwidth(fs_in.tex.z) + 0.000001 ? fs_in.tex.y : fs_in.tex.z, fs_in.tex.x);

	 const vec4 marsKleur = vec4(1, 0.5, 0.5, 1);
	// float marsHoogte = texture(marsHoogte, naadloosTex).x;// + fs_in.grondHoogte) * 0.5;
	
	const vec3 	mijnPlek		= fs_in.pos.xyz / fs_in.pos.w;
	const vec3 	lichtRicht 		= normalize(zonPos - mijnPlek);
	const vec3 	lichtSpiegel 	= reflect(lichtRicht, fs_in.normaal);
	float 		diffuus 		= max(0.0, dot(lichtRicht,  fs_in.normaal));

	if(grondNietWater == 1)
	{	
		kleur = fs_in.kleur * max(0.2, diffuus);// marsKleur * clamp(diffuus, 0.5, 1.0);// fs_in.kleur * marsKleur / 4;//vec4(marsHoogte, marsHoogte * 0.5, 0.0, 1.0);// mix(, marsKleur, lichtheid);
	}
	else
	{
		
		
		float lichtheid 	= diffuus <= 0.0f || dot(lichtRicht, normalize(mijnPlek)) >= 0.0f ? 0.0f : pow(max(0.0f, dot(lichtSpiegel,  normalize(kijkPlek - mijnPlek))), 40) * 0.8f;

		kleur = mix(vec4(fs_in.kleur.xyz * max(0.2, diffuus), fs_in.kleur.a), vec4(vec3(1.0), fs_in.kleur.a), lichtheid) * vec4(1, 1, 1, 0.85);
		kleur.a = max(kleur.a, lichtheid);
	//	kleur.g = fs_in.snelheid;
	}

	
/*
	//Beter visuele check inbouwen voor lager dan nul water
	if(fs_in.waterHoogte < 0.0)			kleur = vec4(1, 1, 0, 1);
	if(fs_in.waterHoogte > 10000.0)		kleur = vec4(0, 1, 0, 1);
	else								kleur = vec4(marsHoogte, marsHoogte * 0.5, fs_in.waterHoogte, 1.0);*/
}
	