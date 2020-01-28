#include "globjects.hpp"
#include <utility>

namespace gl {
	MOVE_ONLY_CLASS_DEF(ShaderProgram)
	void swap (ShaderProgram& l, ShaderProgram& r) {
		std::swap(l.prog, r.prog);
	}

	MOVE_ONLY_CLASS_DEF(Vbo)
	void swap (Vbo& l, Vbo& r) {
		std::swap(l.vbo, r.vbo);
	}

	MOVE_ONLY_CLASS_DEF(Vao)
	void swap (Vao& l, Vao& r) {
		std::swap(l.vao, r.vao);
	}
}
