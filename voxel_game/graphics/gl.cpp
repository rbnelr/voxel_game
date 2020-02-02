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

	MOVE_ONLY_CLASS_DEF(Ubo)
	void swap (Ubo& l, Ubo& r) {
		std::swap(l.ubo, r.ubo);
	}

	MOVE_ONLY_CLASS_DEF(Texture)
	void swap (Texture& l, Texture& r) {
		std::swap(l.tex, r.tex);
	}

	MOVE_ONLY_CLASS_DEF(Sampler)
		void swap (Sampler& l, Sampler& r) {
		std::swap(l.sampler, r.sampler);
	}
}
