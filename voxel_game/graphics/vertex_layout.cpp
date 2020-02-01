#include "vertex_layout.hpp"

void bind_attrib_arrays (Vertex_Layout layout, Shader& shad) {
	for (int i=0; i<layout.count; ++i) {
		auto& a = layout.attribs[i];

		GLint loc = glGetAttribLocation(shad.shader->shad, a.name);
		//if (loc <= -1) fprintf(stderr,"Attribute %s is not used in the shader!", a.name);

		if (loc != -1) {
			assert(loc > -1);

			glEnableVertexAttribArray(loc);
			glVertexAttribPointer(loc, a.components, (GLenum)a.type, a.normalized, (GLsizei)a.stride, (void*)a.offs);
		}
	}
}
