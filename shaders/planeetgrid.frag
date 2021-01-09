#version 440

out vec4 kleur;

in NaarFrag
{
	in vec3 	normaal;
	in vec3		hoeks;
	in vec3 	tex; //x = u! y = s0 en z = s1
	in vec4 	kleur;
	in float 	waterHoogte;
	in float 	grondHoogte;
	in vec2 	snelheid;
	in vec4 	pos;
	in float 	leven;
	in vec2 	plek;
} fs_in;


uniform sampler2D 	waterTextuur;
uniform uint 		grondNietWater;
uniform vec3 		kijkPlek;
uniform vec3 		zonPos;

vec3 berekenVervormdeNormaal(vec3 n, vec3 hoeks, bool water, vec2 vervorming)
{	
	vec3	haaks = cross(n, hoeks);

	return normalize(n + vervorming.x * hoeks + vervorming.y * haaks);
}

void main()
{
	if(grondNietWater == 0 && fs_in.waterHoogte < 0.1) discard;

	//Naadloos tex is op basis van in vertshader genoemd algoritme door Tarini
	vec2 naadloosTex = vec2(fwidth(fs_in.tex.y) <= fwidth(fs_in.tex.z) + 0.000001 ? fs_in.tex.y : fs_in.tex.z, fs_in.tex.x);

	 const vec4 marsKleur = vec4(1, 0.5, 0.5, 1);
	// float marsHoogte = texture(marsHoogte, naadloosTex).x;// + fs_in.grondHoogte) * 0.5;


	naadloosTex += fs_in.plek * 0.001;

	const vec3	vervormdN		= grondNietWater == 1 ? fs_in.normaal.xyz : berekenVervormdeNormaal(fs_in.normaal.xyz, fs_in.hoeks, true, texture(waterTextuur, vec2(sin(naadloosTex.x * 3.142 * 2)  * 10.0, naadloosTex.y * 10.0	)).xy);
	const vec3 	mijnPlek		= fs_in.pos.xyz / fs_in.pos.w;
	const vec3 	lichtRicht 		= normalize(zonPos - mijnPlek);
	const vec3 	lichtSpiegel 	= reflect(lichtRicht, vervormdN);
	float 		diffuus 		= max(0.0, dot(lichtRicht,  vervormdN));

	if(grondNietWater == 1)
	{	
		kleur 	= mix(fs_in.kleur, vec4(0.0, 0.35, 0.0, 1.0), fs_in.leven) * max(0.2, diffuus);// marsKleur * clamp(diffuus, 0.5, 1.0);// fs_in.kleur * marsKleur / 4;//vec4(marsHoogte, marsHoogte * 0.5, 0.0, 1.0);// mix(, marsKleur, lichtheid);
	}
	else
	{
		float lichtheid 	= diffuus <= 0.0f || dot(lichtRicht, normalize(mijnPlek)) >= 0.0f ? 0.0f : pow(max(0.0f, dot(lichtSpiegel,  normalize(kijkPlek - mijnPlek))), 200) * 0.8f;

		float sn = length(fs_in.snelheid);
		vec2  nn = fs_in.snelheid / sn;
		float hk = atan(nn.y, nn.x);

		kleur = mix(vec4(fs_in.kleur.xyz * max(0.2, diffuus), fs_in.kleur.a), vec4(vec3(1.0), fs_in.kleur.a), lichtheid) * vec4(1, 1, 1, 0.85);
		

	//	kleur.r = bobbel.r;
	//	kleur.g = bobbel.g;
		//kleur = vec4(sn, abs(sn * sin(hk)), abs(sn * cos(hk)), 1.0);
		//kleur= vec4(sn, sn, hk / 3.142, 1.0);
		kleur.a = max(kleur.a, lichtheid);
	}

	
/*
	//Beter visuele check inbouwen voor lager dan nul water
	if(fs_in.waterHoogte < 0.0)			kleur = vec4(1, 1, 0, 1);
	if(fs_in.waterHoogte > 10000.0)		kleur = vec4(0, 1, 0, 1);
	else								kleur = vec4(marsHoogte, marsHoogte * 0.5, fs_in.waterHoogte, 1.0);*/
}
	