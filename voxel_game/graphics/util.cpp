#pragma once
#include "util.hpp"

uint32_t bind_attrib_arrays (_Vertex_Layout layout, Shader& shad) {
	uint32_t vertex_size = 0;

	for (int i=0; i<layout.count; ++i) {
		auto& a = layout.attribs[i];

		GLint comps = 1;
		GLenum type = GL_FLOAT;
		uint32_t size = sizeof(float);

		bool int_format = false;

		switch (a.type) {
			case gl::T_FLT:	comps = 1;	type = GL_FLOAT;	size = sizeof(float);	break;
			case gl::T_V2:	comps = 2;	type = GL_FLOAT;	size = sizeof(float);	break;
			case gl::T_V3:	comps = 3;	type = GL_FLOAT;	size = sizeof(float);	break;
			case gl::T_V4:	comps = 4;	type = GL_FLOAT;	size = sizeof(float);	break;

			case gl::T_INT:	comps = 1;	type = GL_INT;		size = sizeof(int);	break;
			case gl::T_IV2:	comps = 2;	type = GL_INT;		size = sizeof(int);	break;
			case gl::T_IV3:	comps = 3;	type = GL_INT;		size = sizeof(int);	break;
			case gl::T_IV4:	comps = 4;	type = GL_INT;		size = sizeof(int);	break;

			case gl::T_U8V4:	int_format = true;	comps = 4;	type = GL_UNSIGNED_BYTE;		size = sizeof(uint8_t);	break;

			default: assert(false);
		}

		vertex_size += size * comps;

		GLint loc = glGetAttribLocation(shad.shader->shad, a.name);
		//if (loc <= -1) fprintf(stderr,"Attribute %s is not used in the shader!", a.name);

		if (loc != -1) {
			assert(loc > -1);

			glEnableVertexAttribArray(loc);
			glVertexAttribPointer(loc, comps, type, int_format ? GL_TRUE : GL_FALSE, (GLsizei)a.stride, (void*)a.offs);
		}
	}

	return vertex_size;
}
