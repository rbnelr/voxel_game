#include "globjects.hpp"
#include <utility>

namespace gl {
	MOVE_ONLY_CLASS_DEF(Vbo)
	void swap (Vbo& l, Vbo& r) {
		std::swap(l.vbo, r.vbo);
	}
}
