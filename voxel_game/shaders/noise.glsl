$include "webgl-noise/src/noise3D.glsl"

float snoise3 (vec3 pos) {
	return snoise(pos);
}
