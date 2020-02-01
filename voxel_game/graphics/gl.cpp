#include "gl.hpp"
#include <utility>

namespace gl {
	MOVE_ONLY_CLASS_DEF(Vbo)
	void swap (Vbo& l, Vbo& r) {
		std::swap(l.vbo, r.vbo);
	}

	MOVE_ONLY_CLASS_DEF(Vao)
		void swap (Vao& l, Vao& r) {
		std::swap(l.vao, r.vao);
	}
}
