#pragma once
#include "globjects.hpp"
#include "glshader.hpp"
#include "camera.hpp"
#include <vector>

struct Attribute {
	const char*		name;
	gl::data_type	type;
	uint64_t		stride;
	uint64_t		offs;
};

struct Vertex_Layout {
	Attribute const* attribs = nullptr;
	int count = 0;

	Vertex_Layout () {}

	template<int N> Vertex_Layout (std::array<Attribute, N> const& arr): attribs{&arr[0]}, count{N} {}

	uint32_t bind_attrib_arrays (Shader const& shad) const {
		uint32_t vertex_size = 0;

		for (int i=0; i<count; ++i) {
			auto& a = attribs[i];

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

			GLint loc = glGetAttribLocation(shad.shad, a.name);
			//if (loc <= -1) fprintf(stderr,"Attribute %s is not used in the shader!", a.name);

			if (loc != -1) {
				assert(loc > -1);

				glEnableVertexAttribArray(loc);
				glVertexAttribPointer(loc, comps, type, int_format ? GL_TRUE : GL_FALSE, (GLsizei)a.stride, (void*)a.offs);
			}
		}

		return vertex_size;
	}
};

template <typename Vertex>
struct MeshStreamDrawer {
	std::shared_ptr<Shader> shader;

	gl::Vao vao;
	gl::Vbo vbo;

	std::vector<Vertex> mesh;

	void init (std::shared_ptr<Shader> shader, Vertex_Layout const& layout) {
		vao = gl::Vao::alloc();
		vbo = gl::Vbo::alloc();

		this->shader = std::move(shader);

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		layout.bind_attrib_arrays(*this->shader);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void draw () {
		if (shader && mesh.size()) {
			glBindVertexArray(vao);

			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(mesh[0]), NULL, GL_STREAM_DRAW);
			glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(mesh[0]), mesh.data(), GL_STREAM_DRAW);

			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mesh.size());
		}

		mesh.clear();
	}
};
