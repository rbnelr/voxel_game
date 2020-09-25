
$include "noise.glsl"
$include "CSG.glsl"

// noise settings
uniform float nfreq[8];
uniform float namp[8];
uniform float nparam0[8];
uniform float nparam1[8];

float noise (int i, vec3 pos) {
	return snoise3(pos * nfreq[i]) * namp[i];
}

const float k = 40.0;

float SDF (vec3 pos) {
	//return snoise3(pos / 20.0) * 20.0;

	//float val;
	//val =          -sphere(pos, vec3(0), 1000.0);
	//val = smax(val, -sphere(pos, vec3(1900, 0, -700), 700.0), k);
	//val = smax(val, -capsule2(pos, vec3(0,0,-600), vec3(1900, 0, -800), 120.0), k);
	//val = smin(val, capsule2(pos, vec3(-1100,0,200), vec3(1100, 0, 500), 180.0), k);
	//val = smin(val, capsule2(pos, vec3(100,0,350), vec3(0, 1100, -200), 120.0), k);

	float val;
	vec3 grad;
	val =          -sphereGrad(pos, vec3(0), 1000.0, grad);
	grad = -grad;

	//val = min(val, max(-grad.z, 0.0));

	float t = map(abs(grad.z), 0.0, nparam1[2]);
	val = mix(val, val - nparam0[2], smoothstep(0.0, 1.0, t));
	//val = smin(val, -pow(max(grad.z, 0.0), nparam1[3]) * nparam0[3], k);

	val += noise(0, pos);
	
	float warp2 = noise(1, pos);
	val += noise(2, pos + vec3(warp2));

	return val;
}

// numerical gradients
const float grad_eps = 0.1;
vec3 grad (vec3 pos, float sdf0) {
	vec3 sdfs = vec3(	SDF(pos + vec3(grad_eps, 0.0, 0.0)),
						SDF(pos + vec3(0.0, grad_eps, 0.0)),
						SDF(pos + vec3(0.0, 0.0, grad_eps)) );
	return normalize(sdfs - vec3(sdf0));
}
