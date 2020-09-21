#version 430 core // for findMSB, SSBO
#extension GL_NV_gpu_shader5 : enable
#extension GL_NV_shader_buffer_load : enable

#define BIT_DEBUGGER 1

$include "common.glsl"
$include "fog.glsl"

$if vertex
	// Fullscreen quad
	const vec4 pos_clip[6] = {
		vec4(+1,-1, 0, 1),
		vec4(+1,+1, 0, 1),
		vec4(-1,-1, 0, 1),
		vec4(-1,-1, 0, 1),
		vec4(+1,+1, 0, 1),
		vec4(-1,+1, 0, 1),
	};

	void main () {
		gl_Position = pos_clip[gl_VertexID];
	}
$endif

$if fragment
	$include "noise.glsl"

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
	float seed = 0.0;//time + 1.0;

	float rand () {
		return gold_noise(gl_FragCoord.xy, seed++);
	}
	vec2 rand2 () {
		return vec2(rand(), rand());
	}
	vec3 rand3 () {
		return vec3(rand(), rand(), rand());
	}

	vec3 srgb (vec3 c) { return pow(c / 255.0, vec3(2.2)); }

	uniform float slider = 1.0; // debugging
	uniform bool visualize_iterations = false;
	uniform int max_iterations = 100; // iteration limiter for debugging
	uniform sampler2D heat_gradient;

	uniform float clip_dist = 10000.0;
	uniform float max_step = 1000;
	uniform float min_step = 0.025;
	uniform float sdf_fac = 1.0;
	uniform float smin_k = 10.0;

	uniform float nfreq[8];
	uniform float namp[8];
	uniform float param0[8];
	uniform float param1[8];

	vec3 sun_light = srgb(vec3(253,255,230));
	vec3 sun_dir = normalize(vec3(3, -4, 9));
	vec3 amb_light = srgb(vec3(145,201,255) / 8.0);

	vec4 shading (vec3 pos, vec3 normal) {
		return vec4( max(dot(normal, sun_dir), 0.0) * sun_light + amb_light, 1.0 );
	}

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

	float sphere (vec3 p, vec3 center, float r) {
		return length(p - center) - r;
	}
	float cylinderx (vec3 p, vec3 center, float r) {
		return length(p.yz - center.yz) - r;
	}
	float cylindery (vec3 p, vec3 center, float r) {
		float rep = 30.0;
		return length(vec2(mod(p.x + rep, rep*2) -rep - center.x, p.z - center.z)) - r;
		//return length(p.xz - center.xz) - r;
	}
	float cylinderz (vec3 p, vec3 center, float r) {
		return length(p.xy - center.xy) - r;
	}
	float cube (vec3 p, vec3 center, float r) {
		vec3 v = abs(p - center);
		return max(max(v.x, v.y), v.z) - r;
	}

	float noise (int i, vec3 pos) {
		return snoise(pos * nfreq[i]) * namp[i] * 2.0;
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

	int iterations = 0;

	vec4 raymarch (vec3 pos0, vec3 dir) {
		float t = 0.0;
		float dist;
		vec3 pos;

		for (;;) {

			pos = pos0 + dir * t;
			dist = SDF(pos);

			if (dist <= min_step)
				break;
			if (iterations++ == max_iterations || t >= clip_dist)
				return vec4(0.0);

			t += min(dist * sdf_fac, max_step);
		}

		vec3 normal = grad(pos, dist);
		//DEBUG(normal);
		return shading(pos, normal);
	}

	// get pixel ray in world space based on pixel coord and matricies
	void get_ray (out vec3 ray_pos, out vec3 ray_dir) {

		//vec2 px_jitter = rand2() - 0.5;
		vec2 px_jitter = vec2(0.0);

		vec2 ndc = (gl_FragCoord.xy + px_jitter) / viewport_size * 2.0 - 1.0;

		if (ndc.x > (slider * 2 - 1))
			discard;

		vec4 clip = vec4(ndc, -1, 1) * clip_near; // ndc = clip / clip.w;

		vec3 pos_cam = (clip_to_cam * clip).xyz;
		vec3 dir_cam = pos_cam;

		ray_pos = ( cam_to_world * vec4(pos_cam, 1)).xyz;
		ray_dir = ( cam_to_world * vec4(dir_cam, 0)).xyz;
		ray_dir = normalize(ray_dir);
	}

	void main () {
		vec3 ray_pos, ray_dir;
		get_ray(ray_pos, ray_dir);

		vec4 col = raymarch(ray_pos, ray_dir);

		if (visualize_iterations)
			col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
		
		FRAG_COL(col);
	}
$endif
