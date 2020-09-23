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

	uniform sampler2D env;

	const vec3 sun_dir = normalize(vec3(3, -4, 9));
	const vec3 sun_light = srgb(vec3(255,225,200)) * 1.2;
	const vec3 amb_light = srgb(vec3(185,221,255)) * 0.02;
	const vec3 camlight_col = srgb(vec3(255,80,30)) * 1.0;

	const vec3 F0 = vec3(0.01);
	const float shininess = 28.0;

	vec3 lights (vec3 pos, vec3 dir, vec3 normal) {
		vec3 L = vec3(0.0);

		vec3 view = -dir;
		float VN = dot(view, normal);

		vec3 F = fresnel(F0, VN);

		{ // sun light
			vec3 refl = reflect(-sun_dir, normal);

			float diff = max(dot(normal, sun_dir), 0.0);
			float spec = diff > 0.0 ? max(dot(refl, view), 0.0) : 0.0;
			spec = pow(spec, shininess) * shininess;

			L +=	mix(vec3(diff), vec3(spec), F) * sun_light;
		}
		{ // camera light
			vec3 campos = (cam_to_world * vec4(+1.0, +1.0, +0.3, 1)).xyz;
			vec3 ldir = normalize(campos - pos);
			float atten = 1.0 / (length(campos - pos) * 0.2 + 1.0);

			vec3 refl = reflect(-ldir, normal);

			float diff = max(dot(normal, ldir), 0.0);
			float spec = diff > 0.0 ? max(dot(refl, view), 0.0) : 0.0;
			spec = pow(spec, shininess) * shininess;

			L +=	mix(vec3(diff), vec3(spec), F) * camlight_col * atten;
		}

		// ambient light
		L += amb_light;

		//{
		//	vec3 refl = reflect(view, normal);
		//	L += texture(env, equirectangular_from_dir(refl), +4.0).rgb * 0.1 * F;
		//}

		return L;
	}

	vec4 shading (vec3 pos, vec3 dir, vec3 normal) {
		const float linew = 0.03;
		vec3 f = fract(pos);
		
		float lines = 0.0;
		if (f.x < linew)
			lines += 1.2 - abs(normal.x);
		if (f.y < linew)
			lines += 1.2 - abs(normal.y);
		if (f.z < linew)
			lines += 1.2 - abs(normal.z);
		lines = clamp(lines, 0.0, 1.0);

		vec3 fw = fwidth(pos);
		float fwm = clamp(max(max(fw.x, fw.y), fw.z) * 2.0, 0.0, 1.0);

		vec3 col = hsl_to_rgb(snoise3(pos / 7000.0), 1.0, 0.7);

		vec3 albedo = mix(col, vec3(0.1), mix(lines, 0.0, fwm));

		return vec4(albedo * lights(pos, dir, normal), 1.0);
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

	float noise (int i, vec3 pos) {
		return snoise3(pos * nfreq[i]) * namp[i] * 2.0;
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
		float prev_t = 0.0, t = 0.0;
		float prev_dist, dist;
		vec3 pos;

		for (;;) {

			pos = pos0 + dir * t;
			dist = SDF(pos);

			if (dist <= min_step*20)
				break;
			if (iterations++ == max_iterations || t >= clip_dist)
				return vec4(0.0);

			prev_t = t;
			prev_dist = dist;

			t += min(dist * sdf_fac, max_step);
		}

		// binary search for surface if ray happened to land in surface
		if (dist < -min_step && prev_t != t) {
			float t0 = prev_t;
			float t1 = t;
			float d0 = prev_dist;
			float d1 = dist;
		
			int iter = 0;
			do {
				t = mix(t0, t1, d0 / (d0 - d1));
				//t = (t0 + t1) * 0.5;
				pos = pos0 + dir * t;
				dist = SDF(pos);
		
				if (dist > 0) {
					t0 = t;
					d0 = dist;
				} else {
					t1 = t;
					d1 = dist;
				}
		
				iterations++;
		
			} while (iter++ < 10 && abs(dist) > min_step);
		
			iterations += iter;
			//DEBUG(vec3(float(iter) / float(10), 0,0));
		}

		//DEBUG(-dist / 10.0);

		vec3 normal = grad(pos, dist);
		//DEBUG(normal);
		return shading(pos, dir, normal);
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

	vec4 tonemap (vec4 col) { // from http://filmicworlds.com/blog/filmic-tonemapping-operators/
		vec3 c = col.rgb * 1.0;
		c = c / (c + 1);
		//c = max(vec3(0.0), c -0.004);
		//c = (c * (6.2 * c +.5)) / (c * (6.2 * c +1.7) +0.06);
		return vec4(c, col.a);
	}
	void main () {
		vec3 ray_pos, ray_dir;
		get_ray(ray_pos, ray_dir);

		vec4 col = raymarch(ray_pos, ray_dir);

		if (visualize_iterations)
			col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
		
		FRAG_COL(tonemap(col));
	}
$endif
