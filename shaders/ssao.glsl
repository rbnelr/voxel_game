#version 450
#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec2 uv;
} vs;

#ifdef _VERTEX
	void main () {
		vs.uv =		        vec2(gl_VertexIndex & 2, (gl_VertexIndex << 1) & 2);
		gl_Position =		vec4(vs.uv * 2.0 - 1.0, 0.0, 1.0);
}
#endif

#ifdef _FRAGMENT
	#include "noise.glsl"

	layout(set = 1, binding = 0) uniform sampler2D main_depth;
	layout(set = 1, binding = 1) uniform sampler2D main_norm; // z implicit

	float get_depth (vec2 uv) {
		// use reverse depth buffer, see camera.cpp
		// return depth in meters (positive is forward from camera)
		return clip_near / texture(main_depth, uv).r;
	}

	layout(set = 1, binding = 2) uniform sampler2D random_vecs;

	//layout(std140, set = 1, binding = 3) uniform Uniforms {
	//	int sample_count; // 64;
	//	float min_radius; // 0.05;
	//	float max_radius; // 0.5;
	//	float _pad0;
	//	vec2 random_vecs_uv_mult;
	//	vec2 _pad1;
	//
	//	vec3 hemisphere_samples[64];
	//}

	const int sample_count = 32;
	const float min_radius = 0.05;
	const float max_radius = 0.5;
	const float base_radius = 0.5;
	const float range_cutoff = 2.0;
	const float bias = 0.01;

	vec3 hemisphere_samples[64] = {
		vec3(-0.430422, +0.368863, 0.607364),
		vec3(+0.088444, +0.132788, 0.496941),
		vec3(-0.023161, +0.043048, 0.010576),
		vec3(+0.053416, +0.089218, 0.033733),
		vec3(+0.017001, +0.001172, 0.012874),
		vec3(-0.819290, -0.078621, 0.392924),
		vec3(-0.458690, +0.815555, 0.140332),
		vec3(-0.021991, +0.030624, 0.004924),
		vec3(+0.441536, -0.282644, 0.061332),
		vec3(+0.001002, -0.003352, 0.124240),
		vec3(+0.053209, -0.024458, 0.034718),
		vec3(+0.275220, -0.293808, 0.536154),
		vec3(+0.012400, -0.127345, 0.007494),
		vec3(-0.304069, -0.145961, 0.841885),
		vec3(+0.064120, +0.066850, 0.133300),
		vec3(+0.154595, -0.092152, 0.066688),
		vec3(-0.028076, +0.020294, 0.076019),
		vec3(-0.075449, -0.063172, 0.276429),
		vec3(+0.035716, -0.089566, 0.046972),
		vec3(+0.100973, -0.058196, 0.156152),
		vec3(+0.403691, +0.192358, 0.219519),
		vec3(-0.058547, -0.347617, 0.195033),
		vec3(-0.151617, -0.104794, 0.154209),
		vec3(-0.001803, +0.017766, 0.003794),
		vec3(-0.051747, +0.121123, 0.443803),
		vec3(+0.007136, -0.289151, 0.208724),
		vec3(+0.278057, -0.709862, 0.596424),
		vec3(-0.124788, -0.003531, 0.184750),
		vec3(-0.493776, -0.433608, 0.723683),
		vec3(-0.042710, -0.031451, 0.029479),
		vec3(+0.561764, -0.142526, 0.351496),
		vec3(-0.052964, -0.341950, 0.343181),
		vec3(-0.210679, -0.796770, 0.135686),
		vec3(-0.635611, +0.751041, 0.112983),
		vec3(+0.205801, -0.203417, 0.689499),
		vec3(+0.195181, -0.054099, 0.071844),
		vec3(+0.019430, +0.060957, 0.090460),
		vec3(-0.012595, +0.051902, 0.010886),
		vec3(+0.060252, +0.462069, 0.801500),
		vec3(+0.454035, -0.264773, 0.133630),
		vec3(-0.171365, +0.207175, 0.148438),
		vec3(-0.311732, +0.499447, 0.359908),
		vec3(-0.009537, -0.015639, 0.006365),
		vec3(+0.133258, +0.027734, 0.015398),
		vec3(+0.180300, -0.391109, 0.351184),
		vec3(+0.130595, +0.036338, 0.047700),
		vec3(+0.200367, +0.358757, 0.312652),
		vec3(-0.151123, +0.688569, 0.695522),
		vec3(-0.006221, -0.005610, 0.045859),
		vec3(-0.157726, -0.209957, 0.651920),
		vec3(+0.031371, +0.052884, 0.022149),
		vec3(+0.023228, +0.080529, 0.038650),
		vec3(-0.170887, -0.144953, 0.534862),
		vec3(-0.499118, +0.509098, 0.177104),
		vec3(+0.182073, +0.048685, 0.005189),
		vec3(+0.258872, -0.491169, 0.532990),
		vec3(+0.016374, -0.015792, 0.010823),
		vec3(+0.460762, +0.648567, 0.327153),
		vec3(-0.010579, -0.197259, 0.592282),
		vec3(-0.088282, -0.093143, 0.178106),
		vec3(-0.009755, -0.007764, 0.006714),
		vec3(+0.076658, +0.105933, 0.341299),
		vec3(+0.225074, +0.264199, 0.167185),
		vec3(-0.269575, +0.034333, 0.002097),
	};

	layout(location = 0) out float frag_col;
	void main () {
		vec3 normal = vec3(0,0,+1);
		normal.xyz = texture(main_norm, vs.uv).xyz;
		normal.y = -normal.y; // y-down in vulkan clip space, so we pretend cam space is y-down yoo to sample correctly
		
		// depth of geometry at pixel
		float geom_depth = get_depth(vs.uv);

		// radius of sample hemisphere based on world-size converted to and clamped in screen space
		float sampl_radius = base_radius / geom_depth;
		sampl_radius = clamp(sampl_radius, min_radius, max_radius);
		
		float cutoff = range_cutoff * sampl_radius / base_radius * geom_depth;

		//vec3 randomVec = texture(random_vecs, vs.uv * random_vecs_uv_mult).rgb;
		vec3 randomVec = normalize(rand3() * 2.0 - 1.0);
		
		// tangent space to camera space matrix to get hemisphere samples pointing away from geometry surface
		float d = dot(randomVec, normal);
		vec3 tangent   = d > 0.0001 ? normalize(randomVec - normal * d) : vec3(1,0,0);
		vec3 bitangent = cross(normal, tangent);
		mat3 TBN       = mat3(tangent, bitangent, normal);  
		
		float occluded = 0;
		for (int i=0; i<sample_count; ++i) {
			// sample vector in screen space
			vec3 sampl_offs = (TBN * hemisphere_samples[i]) * sampl_radius;
			//vec3 s = hemisphere_samples[i] * radius;

			// geometry depth at sample xy screen coord
			float sampl_depth = get_depth(vs.uv + sampl_offs.xy);
			
			float cutoff_fac = cutoff / abs(geom_depth - sampl_depth); // assume sampled difference depth closer than sampl_radius never actually occludes (stops shadow halos around forground objects)
			cutoff_fac = smoothstep(0.0, 1.0, cutoff_fac);
			float occl = sampl_depth + bias < geom_depth - sampl_offs.z ? 1.0 : 0.0; // see if sample point is occluded by geometry (-sampl_offs.z because normal.z points toward camera but depth goes away from camera)
			
			occluded += occl * cutoff_fac;
		}
		float fac = 1.0 - occluded / float(sample_count);
		
		//frag_col = normal.z;
		frag_col = pow(fac, 1.0);
	}
#endif
