
//// https://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
// Gold Noise ©2015 dcerisano@standard3d.com
// - based on the Golden Ratio
// - uniform normalized distribution
// - fastest static noise generator function (also runs at low precision)

float PHI = 1.61803398874989484820459;  // Φ = Golden Ratio   

float gold_noise(in vec2 xy, in float seed){
	return fract(tan(distance(xy*PHI, xy)*seed)*xy.x);
}
///

uniform float time = 0.0;

float seed = time + 1.0;

float rand () {
	return gold_noise(gl_GlobalInvocationID.xy, seed++);
}
vec2 rand2 () {
	return vec2(rand(), rand());
}
vec3 rand3 () {
	return vec3(rand(), rand(), rand());
}

#define PI	3.1415926535897932384626433832795
#define INF (1. / 0.)

vec3 hemisphere_sample () {
	// cosine weighted sampling (100% diffuse)
	// http://www.rorydriscoll.com/2009/01/07/better-sampling/

	vec2 uv = rand2();

	float r = sqrt(uv.y);
	float theta = 2*PI * uv.x;

	float x = r * cos(theta);
	float y = r * sin(theta);

	vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - uv.y)));
	return dir;
}
vec3 get_bounce_dir (vec3 normal) {
	mat3 tangent_to_world;
	{
		vec3 tangent = abs(normal.x) >= 0.9 ? vec3(0,1,0) : vec3(1,0,0);
		vec3 bitangent = cross(normal, tangent);
		tangent = cross(bitangent, normal);

		tangent_to_world = mat3(tangent, bitangent, normal);
	}

	return tangent_to_world * hemisphere_sample();
}
