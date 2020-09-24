
$include "noise.glsl"
$include "CSG.glsl"

// noise settings
uniform float nfreq[8];
uniform float namp[8];
uniform float param0[8];
uniform float param1[8];

float noise (int i, vec3 pos) {
	return snoise3(pos * nfreq[i]) * namp[i];
}

float SDF (vec3 pos) {
	//return snoise3(pos / 20.0) * 20.0;

	vec3 p = pos;
	p.x += noise(0, pos);
	float x = noise(1, p) - param0[1];
	
	p = pos;
	p.z *= param0[3];
	p.z += noise(2, pos);
	x += max(noise(3, p), 0.0);
	
	x = min(x, ellipse(mod(pos, vec3(800)), vec3(400), vec3(50, 10, 20)));

	return x;
}

// numerical gradients
const float grad_eps = 0.1;
vec3 grad (vec3 pos, float sdf0) {
	vec3 sdfs = vec3(	SDF(pos + vec3(grad_eps, 0.0, 0.0)),
						SDF(pos + vec3(0.0, grad_eps, 0.0)),
						SDF(pos + vec3(0.0, 0.0, grad_eps)) );
	return normalize(sdfs - vec3(sdf0));
}
