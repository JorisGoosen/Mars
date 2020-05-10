#version 440

layout (triangles) in;
layout (triangle_strip, max_vertices = 256) out;

uniform mat4 projectie;
uniform mat4 modelView;

in VS_GS_VERTEX
{
	in vec3 normal;
} gs_in[];

out GS_FS_VERTEX
{
	out vec3 normal;
	out vec3 kleur;
} gs_out;

void main() 
{    
	vec4 	p00, p01, p10, p11;

	vec3 	n0 = gs_in[0].normal,
			n1 = gs_in[1].normal,
			n2 = gs_in[2].normal;

	const float deler = 6,
				stap = 1/deler;

	for(float a=0; a<1; a += stap)
	{
		p00 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, a);
		p01 = mix(gl_in[2].gl_Position, gl_in[1].gl_Position, a);
		
		p10 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, a + stap);
		p11 = mix(gl_in[2].gl_Position, gl_in[1].gl_Position, a + stap);

		const float bstap = stap * 2;//a < 0.5 ? stap : a < 0.75 ? 1 / (4): 1 / (2) ; //distance(p00.xyz, p01.xyz) / stap;// ;//distance(p00.xyz / p00.w, p01.xyz / p01.w);
		
		for(float b=0; b<1; b += bstap)
		{
			

			gl_Position = projectie * modelView * mix(p00, p01, b);			gs_out.normal = mix(n0, n1, b);			gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
			gl_Position = projectie * modelView * mix(p00, p01, b + bstap);	gs_out.normal = mix(n0, n1, b + bstap);	gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
			gl_Position = projectie * modelView * mix(p10, p11, b);			gs_out.normal = mix(n0, n1, b);			gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
			
			EndPrimitive();

			gl_Position = projectie * modelView * mix(p00, p01, b + bstap);			gs_out.normal = mix(n0, n1, b+ bstap);		gs_out.kleur = vec3(0, 1, 0);	EmitVertex();
			gl_Position = projectie * modelView * mix(p10, p11, b);					gs_out.normal = mix(n0, n1, b );			gs_out.kleur = vec3(0, 1, 0);	EmitVertex();
			gl_Position = projectie * modelView * mix(p10, p11, b + bstap);			gs_out.normal = mix(n0, n1, b+ bstap);		gs_out.kleur = vec3(0, 1, 0);	EmitVertex();
			
			EndPrimitive();
		}
	}

	/*
    for(int i = 0; i < gl_in.length(); i++)
    {
      gl_Position 	= gl_in[i].gl_Position;
	  gs_out.normal = gs_in[i].normal;
	  EmitVertex();
	}
	EndPrimitive();
*/
/*	vec4 nieuwPunt = gl_in[0].gl_Position + gl_in[1].gl_Position + gl_in[2].gl_Position;
	
	vec3 nieuwNormaal = vec3(0);
	for(int i=0;i<3;i++)
		nieuwNormaal += gs_in[i].normal * 0.33333333f;

	//float gewensteLengte =  (length(gl_in[0].gl_Position) + length(gl_in[1].gl_Position) + length(gl_in[2].gl_Position)) / 3;
	//nieuwPunt = vec4(nieuwNormaal * gewensteLengte, 0) ;//normalize(nieuwPunt) * 9;

	//nieuwPunt /= 3;
	//nieuwPunt += vec4(nieuwNormaal, 0) * -0.5;

	nieuwPunt.xyz = nieuwNormaal * 3.5;


	gl_Position = projectie * modelView * gl_in[0].gl_Position;		gs_out.normal = gs_in[0].normal;	gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
	gl_Position = projectie * modelView * gl_in[1].gl_Position;		gs_out.normal = gs_in[1].normal;	gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
	gl_Position = projectie * modelView * nieuwPunt;				gs_out.normal = nieuwNormaal;		gs_out.kleur = vec3(0, 1, 0);	EmitVertex();
	EndPrimitive();

	gl_Position = projectie * modelView * gl_in[1].gl_Position;		gs_out.normal = gs_in[1].normal;	gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
	gl_Position = projectie * modelView * gl_in[2].gl_Position;		gs_out.normal = gs_in[2].normal;	gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
	gl_Position = projectie * modelView * nieuwPunt;				gs_out.normal = nieuwNormaal;		gs_out.kleur = vec3(0, 1, 0);	EmitVertex();
	EndPrimitive();

	gl_Position = projectie * modelView * gl_in[2].gl_Position;		gs_out.normal = gs_in[2].normal;	gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
	gl_Position = projectie * modelView * gl_in[0].gl_Position;		gs_out.normal = gs_in[0].normal;	gs_out.kleur = vec3(1, 0, 0);	EmitVertex();
	gl_Position = projectie * modelView * nieuwPunt;				gs_out.normal = nieuwNormaal;		gs_out.kleur = vec3(0, 1, 0);	EmitVertex();
	EndPrimitive();*/
}