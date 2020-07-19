//This file simply opens an OpenGL window where a quad will be rendered and some simple shaders to display this fact, press escape to exit.
#include <RenderSchermPerspectief.h>
#include <geometrie/icosahedron.h>

int main()
{
	RenderSchermPerspectief scherm("Perspectief Demo");


	scherm.maakVlakVerdelingsShader("deeltjes", "shaders/deeltjes.vert", "shaders/deeltjes.frag", "shaders/deeltjes.tess");
//	scherm.maakShader("deeltjes", "shaders/deeltjes.vert", "shaders/deeltjes.frag");

	const GLfloat tessVal = 4.0;
	const GLfloat defaultOuterTess[] = {tessVal, tessVal, tessVal, tessVal};
	const GLfloat defaultInnerTess[] = {tessVal, tessVal};
	
	glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, defaultOuterTess);
	glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, defaultInnerTess);
	glClearColor(0,0,0,0);

	Icosahedron ico;

	float rot = 0.0f;
	while(!scherm.stopGewenst())
	{
		scherm.RecalculateProjection();
		scherm.setModelView(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f)), rot, glm::vec3(0.0f, 1.0f, 0.0f)));
		scherm.bereidRenderVoor();

		ico.tekenJezelfPatchy();
		glErrorToConsole("Woppaloppa Mainloop ");
		//scherm.renderQuad();
		scherm.rondRenderAf();
		glErrorToConsole("rondRenderAf: ");
		rot += 0.01f;
	}
}