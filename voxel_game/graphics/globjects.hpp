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
