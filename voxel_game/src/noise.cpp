#include "stdafx.hpp"
#include "noise.hpp"

namespace noise {
	float __vectorcall perlin (float x) {
		return sinf(x * PI);
	}
}
