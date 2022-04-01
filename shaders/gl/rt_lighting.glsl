#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t
//#extension GL_ARB_shader_group_vote : enable

#if VISUALIZE_COST && VISUALIZE_TIME
	#extension GL_ARB_shader_clock : enable
#endif

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;

#include "rt_util.glsl"

layout(rgba32f, binding = 0) writeonly restrict uniform image2D img_col;

uniform int rand_seed_time = 0;
uniform ivec2 framebuf_size;

uniform bool show_light = false;
uniform bool show_normals = false;

uniform float bounce_max_dist = 90.0;
uniform int bounce_max_count = 3;
uniform int bounce_samples = 1;

uniform float roughness = 0.8;

uniform sampler2D gbuf_pos;
uniform sampler2D gbuf_col;
uniform sampler2D gbuf_norm;

bool read_gbuf (ivec2 pxpos, out Hit hit, out float depth) {
	hit.pos     = texelFetch(gbuf_pos , pxpos, 0).rgb;
	vec4 col    = texelFetch(gbuf_col , pxpos, 0);
	hit.normal  = texelFetch(gbuf_norm, pxpos, 0).rgb;
	
	//hit.dist = depth;
	//hit.coord = ivec3(floor(hit.pos));
	
	hit.col = vec4(col.rgb, 1.0);
	hit.emiss = col.rgb * col.w;
	
	return hit.pos.x > -99.0;
}

void main () {
	INIT_VISUALIZE_COST();
	
	ivec2 threadid = ivec2(gl_LocalInvocationID.xy);
	ivec2 wgroupid = ivec2(work_group_tiling(20u));
	
	ivec2 pxpos = wgroupid * ivec2(WG_PIXELS_X,WG_PIXELS_Y) + threadid;
	
	srand(pxpos.x, pxpos.y, rand_seed_time);
	
	#if DEBUGDRAW
	_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
	#endif
	
	Hit hit;
	float hit_depth;
	bool did_hit = read_gbuf(pxpos, hit, hit_depth);
	
	#if 0 && DEBUGDRAW
	if (_dbgdraw) {
		vec3 p = hit.pos - WORLD_SIZEf/2.0;
		mat3 TBN = generate_TBN(hit.normal);
		dbgdraw_vector(p, TBN[0] * 0.3, vec4(1,0,0,1));
		dbgdraw_vector(p, TBN[1] * 0.3, vec4(0,1,0,1));
		dbgdraw_vector(p, TBN[2] * 0.3, vec4(0,0,1,1));
	}
	#endif
	
	if (show_light) hit.col.rgb = vec3(1.0);
	
	vec4 col = vec4(0.0);
	
	#if BOUNCE_ENABLE
	
	vec3 light = vec3(0.0);
	
	if (did_hit) {
		for (int j=0; j<bounce_samples; ++j) {
			Hit hit2 = hit;
			
			vec3 A = vec3(1.0);
			
			float dist_remain = bounce_max_dist;
			
			for (int i=0; i<bounce_max_count; ++i) {
				vec3 ray_pos = hit2.pos + hit2.normal * epsilon;
				vec3 ray_dir;
				
				ray_dir = generate_TBN(hit2.normal) * hemisphere_sample();
				
				//if (dot(ray_dir, hit2.gTBN[2]) <= 0.0)
				//	break; // normal mapping made generated ray that went into the surface TODO: what do?
				
				if (max(max(A.x,A.y),A.z) < 0.02) break;
				
				#if DEBUGDRAW
				if (_dbgdraw) dbgdraw_vector(ray_pos - WORLD_SIZEf/2.0, ray_dir * 0.3, vec4(1,1,0,1));
				#endif
				
				if (!trace_ray(ray_pos, ray_dir, dist_remain, hit2, vec3(0,0,1),false))
					break;
				
				light += A * hit2.emiss;
				
				A *= hit2.col.rgb;
				
				dist_remain -= hit2.dist;
				
				if (dist_remain <= 0.0)
					break;
			}
		}
		light *= 1.0 / float(bounce_samples);
	}
	
	light = APPLY_TAA(light, hit.pos, hit.normal, pxpos);
	
	col.rgb = hit.col.rgb * light + hit.emiss;
	col.a = hit.col.a;
	#else
	if (did_hit) {
		float sun = max(dot(hit.normal, normalize(vec3(1, 1.8, 4.0))) * 0.5 + 0.5, 0.0);
		const vec3 amb = vec3(0.1,0.1,0.3) * 0.4;
		
		hit.col.rgb *= sun*sun * (1.0 - amb) + amb;
		
		col = hit.col;
	}
	#endif
	
	if (show_normals) col.rgb = hit.normal * 0.5 + 0.5;
	
	GET_VISUALIZE_COST(col.rgb);
	
	imageStore(img_col, pxpos, col);
}