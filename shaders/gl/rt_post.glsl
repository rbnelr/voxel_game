#version 460 core
#include "fullscreen_triangle.glsl"

#ifdef _FRAGMENT
	uniform sampler2D light;
	uniform usampler2D gbuf_faceid;
	
	uniform float gauss_radius_px = 20;
	
	uniform float exposure = 1.0;
	
	uniform sampler2D gbuf_col;
	uniform sampler2D gbuf_norm;

	uniform bool show_light = false;
	uniform bool show_normals = false;

	struct Gbuf {
		//float depth;
		vec3 normal;
		vec4 col;
		vec3 emiss;
	};
	void read_gbuf (ivec2 pxpos, out Gbuf g) {
		
		//g.depth  = texelFetch(gbuf_pos   , pxpos, 0).r;
		vec4 col = texelFetch(gbuf_col   , pxpos, 0);
		g.normal = texelFetch(gbuf_norm  , pxpos, 0).rgb;
		
		g.col = vec4(col.rgb, 1.0);
		g.emiss = col.rgb * col.a;
	}

	float gauss_kernel (float x) {
		float sigma = 0.35 * gauss_radius_px;
		
		return exp((x*x) / (-2.0 * sigma * sigma));
	}
	
	out vec3 frag_col;
	void main () {
		vec2 px_sz = 1.0 / textureSize(light, 0).xy;
		
		vec3 col = vec3(0.0);
		float total = 0.0;
		
		uint faceid = texture(gbuf_faceid, vs_uv).r;
		
		int r = int(ceil(gauss_radius_px));
		
#if 0
		
	#if PASS == 0
		for (int i=-r; i<=r; ++i) {
			vec2 uv = vs_uv + vec2(0.0, px_sz.y * float(i));
			
			if (faceid == texture(gbuf_faceid, uv).r) {
				float weight = gauss_kernel(float(i));
				col   += weight * texture(light, uv).rgb;
				total += weight;
			}
		}
		col /= total;
	#else
		for (int i=-r; i<=r; ++i) {
			vec2 uv = vs_uv + vec2(px_sz.x * float(i), 0.0);
			
			if (faceid == texture(gbuf_faceid, uv).r) {
				float weight = gauss_kernel(float(i));
				col   += weight * texture(light, uv).rgb;
				total += weight;
			}
		}
		col /= total;
		col *= exposure;
	#endif
#else
	
	#if PASS == 0
		for (int i=0; i<=r; ++i) {
			vec2 uv = vs_uv - vec2(0.0, px_sz.y * float(i));
			if (faceid == texture(gbuf_faceid, uv).r) {
				float weight = gauss_kernel(float(i));
				col   += weight * texture(light, uv).rgb;
				total += weight;
			} else break;
		}
		for (int i=0; i<=r; ++i) {
			vec2 uv = vs_uv + vec2(0.0, px_sz.y * float(i));
			if (faceid == texture(gbuf_faceid, uv).r) {
				float weight = gauss_kernel(float(i));
				col   += weight * texture(light, uv).rgb;
				total += weight;
			} else break;
		}
		col /= total;
	#else
		for (int i=0; i<=r; ++i) {
			vec2 uv = vs_uv - vec2(px_sz.x * float(i), 0.0);
			if (faceid == texture(gbuf_faceid, uv).r) {
				float weight = gauss_kernel(float(i));
				col   += weight * texture(light, uv).rgb;
				total += weight;
			} else break;
		}
		for (int i=0; i<=r; ++i) {
			vec2 uv = vs_uv + vec2(px_sz.x * float(i), 0.0);
			if (faceid == texture(gbuf_faceid, uv).r) {
				float weight = gauss_kernel(float(i));
				col   += weight * texture(light, uv).rgb;
				total += weight;
			} else break;
		}
		col /= total;
		
		
		Gbuf gbuf;
		read_gbuf(ivec2(gl_FragCoord.xy), gbuf);
		
		vec3 light = col;
		col.rgb = gbuf.col.rgb * light + gbuf.emiss;
		
		if (show_light) col.rgb = light;
		if (show_normals) col.rgb = gbuf.normal * 0.5 + 0.5;
		
		col.rgb *= exposure;
	#endif

#endif
		
		frag_col = col;
	}
#endif
