#version 440

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;


in VS_GS_VERTEX
{
	in vec3 normal;
} gs_in[];

out GS_FS_VERTEX
{
	out vec3 normal;
} gs_out;

void main() 
{    
    for(int i = 0; i < gl_in.length(); i++)
    {
      gl_Position 	= gl_in[i].gl_Position;
	  gs_out.normal = gs_in[i].normal;
	  EmitVertex();
	}
	EndPrimitive();
/*
	vec4 nieuwPunt = gl_in[0].gl_Position + gl_in[1].gl_Position + gl_in[2].gl_Position;
	float gewensteLengte = (length(gl_in[0].gl_Position) + length(gl_in[1].gl_Position) + length(gl_in[2].gl_Position)) / 3;
	nieuwPunt = normalize(nieuwPunt) * gewensteLengte;

	gl_Position = gl_in[0].gl_Position;		EmitVertex();
	gl_Position = gl_in[1].gl_Position;		EmitVertex();
	gl_Position = nieuwPunt;				EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[1].gl_Position;		EmitVertex();
	gl_Position = gl_in[2].gl_Position;		EmitVertex();
	gl_Position = nieuwPunt;				EmitVertex();
	EndPrimitive();

	gl_Position = gl_in[2].gl_Position;		EmitVertex();
	gl_Position = gl_in[0].gl_Position;		EmitVertex();
	gl_Position = nieuwPunt;				EmitVertex();
	EndPrimitive();*/
}