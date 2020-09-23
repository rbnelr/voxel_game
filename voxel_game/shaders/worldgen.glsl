
$include "noise.glsl"

// noise settings
uniform float nfreq[8];
uniform float namp[8];
uniform float param0[8];
uniform float param1[8];

// Based on https://www.iquilezles.org/www/articles/smin/smin.htm:
// polynomial smooth min (k = 0.1);
float smin (float a, float b, float k)	{
	float h = max(k - abs(a - b), 0.0) / k;
	return min(a, b) - h*h*k * 0.25;
}
float smax (float a, float b, float k)	{
	float h = max(k - abs(a - b), 0.0) / k;
	return max(a, b) + h*h*k * 0.25;
}

float noise (int i, vec3 pos) {
	return snoise3(pos * nfreq[i]) * namp[i];
}

float SDF (vec3 pos) {

	vec3 p = pos;
	p.x += noise(0, pos);
	float x = noise(1, p) - param0[1];

	p = pos;
	p.z *= param0[3];
	p.z += noise(2, pos);
	x += max(noise(3, p), 0.0);

	return x;
}

vec3 grad (vec3 pos, float sdf0) {
	float eps = 0.1;
	vec3 sdfs = vec3(	SDF(pos + vec3(eps, 0.0, 0.0)),
		SDF(pos + vec3(0.0, eps, 0.0)),
		SDF(pos + vec3(0.0, 0.0, eps)) );
	return normalize(sdfs - vec3(sdf0));
}
