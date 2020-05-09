#version 440

layout (triangles) in;
layout (triangle_strip, max_vertices = 16) out;

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
	/*
    for(int i = 0; i < gl_in.length(); i++)
    {
      gl_Position 	= gl_in[i].gl_Position;
	  gs_out.normal = gs_in[i].normal;
	  EmitVertex();
	}
	EndPrimitive();
*/
	vec4 nieuwPunt = gl_in[0].gl_Position + gl_in[1].gl_Position + gl_in[2].gl_Position;
	
	vec3 nieuwNormaal = vec3(0);
	for(int i=0;i<3;i++)
		nieuwNormaal += gs_in[i].normal * 0.33333333f;

	//float gewensteLengte =  (length(gl_in[0].gl_Position) + length(gl_in[1].gl_Position) + length(gl_in[2].gl_Position)) / 3;
	//nieuwPunt = vec4(nieuwNormaal * gewensteLengte, 0) ;//normalize(nieuwPunt) * 9;

	nieuwPunt /= 3;
	nieuwPunt += vec4(nieuwNormaal, 0);


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
	EndPrimitive();
}