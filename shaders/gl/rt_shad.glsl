#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t
//#extension GL_ARB_shader_group_vote : enable

#if VISUALIZE_COST && VISUALIZE_TIME
	#extension GL_ARB_shader_clock : enable
#endif

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;

#include "rt_util.glsl"

uniform int rand_seed_time = 0;
uniform ivec2 framebuf_size;


uniform bool show_light = false;
uniform bool show_normals = false;

uniform float bounce_max_dist = 90.0;
uniform int bounce_max_count = 3;

uniform float roughness = 0.8;


layout(rgba16f, binding = 0) writeonly restrict uniform image2D rt_col;

void main () {
	INIT_VISUALIZE_COST();
	
	ivec2 threadid = ivec2(gl_LocalInvocationID.xy);
	ivec2 wgroupid = ivec2(work_group_tiling(20u));
	
	ivec2 pxpos = wgroupid * ivec2(WG_PIXELS_X,WG_PIXELS_Y) + threadid;
	
	srand(pxpos.x, pxpos.y, rand_seed_time);
	
	#if DEBUGDRAW
	_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
	#endif
	
	vec3 ray_pos, ray_dir;
	get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	vec4 col = vec4(0,0,0,1);
	
#if 0
	Hit hit;
	if (trace_ray(ray_pos, ray_dir, INF, hit, vec3(1,0,0),true)) {
		
		col = vec4(hit.col.rgb, hit.emiss_raw);
		if (show_normals) col.rgb = hit.normal * 0.5 + 0.5;
	}
#else
	Hit hit;
	if (trace_ray(ray_pos, ray_dir, INF, hit, vec3(1,0,0),true)) {
		
		#if DEBUGDRAW // visualize tangent space
		//if (_dbgdraw) dbgdraw_vector(hit.pos - WORLD_SIZEf/2.0, hit.gTBN[0]*0.3, vec4(1,0,0,1));
		//if (_dbgdraw) dbgdraw_vector(hit.pos - WORLD_SIZEf/2.0, hit.gTBN[1]*0.3, vec4(0,1,0,1));
		//if (_dbgdraw) dbgdraw_vector(hit.pos - WORLD_SIZEf/2.0, hit.gTBN[2]*0.3, vec4(0,0,1,1));
		//if (_dbgdraw) dbgdraw_vector(hit.pos - WORLD_SIZEf/2.0, hit.normal*0.3, vec4(0,0,1,1));
		#endif
		
	#if BOUNCE_ENABLE
		if (show_light) hit.col.rgb = vec3(1.0);
		
		Hit phit = hit;
		
		vec3 light = vec3(0.0);
		vec3 A = vec3(1.0);
		//hit.col = vec4(1.0); // pretend first surface is white to make TAA store light, not light*prim_col
		
		float dist_remain = bounce_max_dist;
		
		for (int i=0; i<bounce_max_count; ++i) {
			ray_pos = hit.pos + hit.normal * epsilon;
			
		#if 0
			float F = fresnel_roughness(max(0., -dot(hit.normal, ray_dir)), .04, roughness);
			
			if (F > rand()) {
				ray_dir = reflect_roughness(reflect(ray_dir, hit.normal), hit.normal, roughness);
			} else {
				A *= hit.col.rgb;
				ray_dir = generate_TBN(hit.normal) * hemisphere_sample();
			}
		#else
			A *= hit.col.rgb;
			ray_dir = generate_TBN(hit.normal) * hemisphere_sample();
		#endif
			
			if (dot(ray_dir, hit.gnormal) <= 0.0)
				break; // normal mapping made generated ray that went into the surface TODO: what do?
			
			if (max(max(A.x,A.y),A.z) < 0.02)
				break;
			
			if (trace_ray(ray_pos, ray_dir, dist_remain, hit, vec3(0,0,1),false)) {
				light += A * hit.emiss;
				dist_remain -= hit.dist;
				if (dist_remain <= 0.0)
					break;
			} else {
				break;
			}
		}
	
		light = APPLY_TAA(light, phit.pos, phit.coord, phit.normal, pxpos);
		
		col.rgb = light + phit.emiss;
		col.a = phit.col.a;
	#else
		float sun = max(dot(hit.normal, normalize(vec3(1, 1.8, 4.0))) * 0.5 + 0.5, 0.0);
		const vec3 amb = vec3(0.1,0.1,0.3) * 0.4;
		
		hit.col.rgb *= sun*sun * (1.0 - amb) + amb;
		
		col = hit.col;
		
		if (show_normals)
			col.rgb = hit.normal * 0.5 + 0.5;
	#endif
	}
#endif
	
	GET_VISUALIZE_COST(col.rgb);
	
	imageStore(rt_col, pxpos, col);
}
