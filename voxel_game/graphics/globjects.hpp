#pragma once
#include "../glad/glad.h"
#include "../move_only_class.hpp"

/////
// Just a bunch of wrapper classes to avoid having to manually manage opengl object lifetimes
////

namespace gl {
	enum data_type {
		T_FLT		=0,
		T_V2		,
		T_V3		,
		T_V4		,

		T_INT		,
		T_IV2		,
		T_IV3		,
		T_IV4		,

		T_U8V4		,

		T_M3		,
		T_M4		,

		T_BOOL		,
	};
	
	class ShaderProgram { // OpenGL "Program" ie. Combination of vertex shader, fragment shader, etc. (usually just called a shader)
		MOVE_ONLY_CLASS_DECL(ShaderProgram)
		GLuint prog = 0;
	public:

		ShaderProgram () {}
		static ShaderProgram alloc () {
			ShaderProgram shad;
			shad.prog = glCreateProgram();
			return shad;
		}
		~ShaderProgram () {
			if (prog)
				glDeleteProgram(prog);
		}

		operator GLuint () const { return prog; }
	};

	class Vbo {
		MOVE_ONLY_CLASS_DECL(Vbo)
		GLuint vbo = 0;
	public:

		Vbo () {}
		static Vbo alloc () {
			Vbo vbo;
			glGenBuffers(1, &vbo.vbo);
			return vbo;
		}
		~Vbo () {
			if (vbo)
				glDeleteBuffers(1, &vbo);
		}

		operator GLuint () const { return vbo; }
	};

	class Vao {
		MOVE_ONLY_CLASS_DECL(Vao)
		GLuint vao = 0;
	public:

		Vao () {}
		static Vao alloc () {
			Vao vao;
			glGenVertexArrays(1, &vao.vao);
			return vao;
		}
		~Vao () {
			if (vao)
				glDeleteVertexArrays(1, &vao);
		}

		operator GLuint () const { return vao; }
	};
}
