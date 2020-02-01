#pragma once
#include "../glad/glad.h"
#include "../move_only_class.hpp"

/////
// Just a bunch of wrapper classes to avoid having to manually manage opengl object lifetimes
////

namespace gl {
	enum type {
		FLOAT			,
		FLOAT2			,
		FLOAT3			,
		FLOAT4			,
		INT				,
		INT2			,
		INT3			,
		INT4			,
		U8V4			,
		MAT3			,
		MAT4			,
		BOOL			,
	};

	enum class Enum : GLenum {
		FLOAT			= GL_FLOAT,
		INT				= GL_INT,
		UNSIGNED_BYTE	= GL_UNSIGNED_BYTE,
		BOOL			= GL_BOOL,
	};

	class Vbo {
		MOVE_ONLY_CLASS_DECL(Vbo)
		GLuint vbo;
	public:

		Vbo () {
			glGenBuffers(1, &vbo);
		}
		~Vbo () {
			glDeleteBuffers(1, &vbo);
		}

		operator GLuint () const { return vbo; }
	};
}
