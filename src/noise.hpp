#pragma once

#include <cmath>

#if 0
float simplex_part (float x, float y, float z, int ix, int iy) {

	float fx = x - (float)ix;
	float fy = y - (float)iy;

	float f = 1.0f - x*x - y*y;
	float f2 = f*f;
	float f3 = f*f2;
	f3 = f3 >= 0.0f ? f3 : 0.0f;

	float h = hash(ix);

	return h * f3;
}

float snoise3 (float x, float y, float z) {
	
	float floorx = floorf(x);

	int ix = (int)floorx;

	float val;
	val  = simplex_part(x,y,z, ix);
	val += simplex_part(x,y,z, ix+1);

	return val * (1.0f / hash_max);
}
#endif
