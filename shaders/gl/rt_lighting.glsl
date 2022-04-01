#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t
//#extension GL_ARB_shader_group_vote : enable

#if VISUALIZE_COST && VISUALIZE_TIME
	#extension GL_ARB_shader_clock : enable
#endif

#if VCT && !VCT_DBG_PRIMARY
layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y, local_size_z = WG_CONES) in;
#else
layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;
#endif

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

struct Gbuf {
	vec3 pos;
	vec3 normal;
	vec4 col;
	vec3 emiss;
	
	mat3 TBN;
};
bool read_gbuf (ivec2 pxpos, out Gbuf g, out float depth) {
	g.pos     = texelFetch(gbuf_pos , pxpos, 0).rgb;
	vec4 col  = texelFetch(gbuf_col , pxpos, 0);
	g.normal  = texelFetch(gbuf_norm, pxpos, 0).rgb;
	
	g.normal = normalize(g.normal);
	
	g.TBN = calc_TBN(g.normal, generate_tangent(g.normal));
	
	//hit.dist = depth;
	//hit.coord = ivec3(floor(hit.pos));
	
	g.col = vec4(col.rgb, 1.0);
	g.emiss = col.rgb * col.w;
	
	return g.pos.x > -1000.0 * 1000000.0;
}

struct Cone {
	vec3   dir;
	float  slope;
	float  weight;
	float _pad0, _pad1, _pad2;
};
layout(std140, binding = 4) uniform ConeConfig {
	ivec4 count; // vector for padding
	Cone cones[32];
} cones;

uniform float vct_start_dist = 1.0 / 5.0;
uniform float vct_primary_cone_width = 1.0;

#if VCT
shared vec3 cone_results[WG_PIXELS_X*WG_PIXELS_Y][WG_CONES];
	
void main () {
	INIT_VISUALIZE_COST();
	
	uint threadid = gl_LocalInvocationID.y * WG_PIXELS_X + gl_LocalInvocationID.x;
	uint coneid   = gl_LocalInvocationID.z;
	
	ivec2 wgroupid = ivec2(work_group_tiling(20u));
	ivec2 pxpos = wgroupid * ivec2(WG_PIXELS_X,WG_PIXELS_Y) + ivec2(gl_LocalInvocationID.xy);
	
	#if DEBUGDRAW
	_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
	#endif
	
	Gbuf gbuf;
	float hit_depth;
	bool did_hit = read_gbuf(pxpos, gbuf, hit_depth);
	
	#if 0 && DEBUGDRAW
	if (_dbgdraw) {
		vec3 p = hit.pos - WORLD_SIZEf/2.0;
		mat3 TBN = generate_TBN(hit.normal);
		dbgdraw_vector(p, TBN[0] * 0.3, vec4(1,0,0,1));
		dbgdraw_vector(p, TBN[1] * 0.3, vec4(0,1,0,1));
		dbgdraw_vector(p, TBN[2] * 0.3, vec4(0,0,1,1));
	}
	#endif
	
	if (show_light) gbuf.col.rgb = vec3(1.0);
	
#if VCT_DBG_PRIMARY
	vec3 ray_pos, ray_dir;
	get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	vec4 col = trace_cone(ray_pos, ray_dir,
		vct_primary_cone_width, vct_start_dist, 1000.0, true);
	
	if (show_normals) col.rgb = gbuf.normal * 0.5 + 0.5;
	
	GET_VISUALIZE_COST(col.rgb);
	
	imageStore(img_col, pxpos, col);
#else
	
	// let each z-layer of threads of the WG handle one cone
	// to improve cache access pattern of VCT
	// since all cones will be handled simultaneously
	// rather than cone#0 first for each single processor core on the gpu, then cone#1 etc.
	// this pattern presumably casues too many voxels to be accessed at once
	// (looking into the distance, each pixel can be in a different voxel)
	// adding the Z layer to the WG, cuts down on the visible layer and makes it so that
	// all cones of a single pixel are handled at once, which will share significant data due to starting in the same voxel
	// and even though the following cells will differ due to differing directions
	// the LOD increases, thus quickly increasing the amount of shared data again
	// (pixels come nearer and nearer in the scaled down version of the world that is the LOD)
	// This WG optimization makes a _MAJOR_ difference, it avoids 60fps -> 5fps slowdowns in the worst case
	if (did_hit) {
		Cone c = cones.cones[coneid];
		
		vec3 cone_dir = gbuf.TBN * c.dir;
				
		vec3 light = trace_cone(gbuf.pos, cone_dir, c.slope,
			vct_start_dist, 1000.0, true).rgb * c.weight;
		
		cone_results[threadid][coneid] = light;
	}
	barrier();
		
	// Write out results for pixel on z==0 threads in WG
	if (coneid == 0u) {
		
		vec3 light = vec3(0.0);
		
		if (did_hit) {
			for (uint i=0u; i<WG_CONES; ++i)
				light += cone_results[threadid][i];
		}
		
		vec4 col;
		col.rgb = gbuf.col.rgb * light + gbuf.emiss;
		col.a = gbuf.col.a;
		
		if (show_normals) col.rgb = gbuf.normal * 0.5 + 0.5;
		
		GET_VISUALIZE_COST(col.rgb);
		
		imageStore(img_col, pxpos, col);
	}
	
#endif
}
#else //// Ray-based RT

void main () {
	INIT_VISUALIZE_COST();
	
	ivec2 threadid = ivec2(gl_LocalInvocationID.xy);
	ivec2 wgroupid = ivec2(work_group_tiling(20u));
	
	ivec2 pxpos = wgroupid * ivec2(WG_PIXELS_X,WG_PIXELS_Y) + threadid;
	
	srand(pxpos.x, pxpos.y, rand_seed_time);
	
	#if DEBUGDRAW
	_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
	#endif
	
	Gbuf gbuf;
	float hit_depth;
	bool did_hit = read_gbuf(pxpos, gbuf, hit_depth);
	
	if (show_light) gbuf.col.rgb = vec3(1.0);
	
	vec4 col = vec4(0.0);
	
	#if BOUNCE_ENABLE
	vec3 light = vec3(0.0);
	
	if (did_hit) {
		for (int j=0; j<bounce_samples; ++j) {
			Hit hit2;
			hit2.pos = gbuf.pos;
			hit2.normal = gbuf.normal;
			
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
	
	light = APPLY_TAA(light, gbuf.pos, gbuf.normal, pxpos);
	
	col.rgb = gbuf.col.rgb * light + gbuf.emiss;
	col.a = gbuf.col.a;
	#else
	if (did_hit) {
		float sun = max(dot(gbuf.normal, normalize(vec3(1, 1.8, 4.0))) * 0.5 + 0.5, 0.0);
		const vec3 amb = vec3(0.1,0.1,0.3) * 0.4;
		
		gbuf.col.rgb *= sun*sun * (1.0 - amb) + amb;
		
		col = gbuf.col;
	}
	#endif
	
	if (show_normals) col.rgb = gbuf.normal * 0.5 + 0.5;
	
	GET_VISUALIZE_COST(col.rgb);
	
	imageStore(img_col, pxpos, col);
}
#endif
	