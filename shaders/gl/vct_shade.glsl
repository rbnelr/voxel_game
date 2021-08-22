 #version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

#if VCT_DIFFUSE && !VCT_DBG_PRIMARY
layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y, local_size_z = WG_CONES) in;
#else
layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;
#endif

#define RAND_SEED_TIME 1

#include "rt_util.glsl"

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

uniform float vct_start_dist = 1.0 / 16.0;
uniform float vct_stepsize = 1.0;

uniform float vct_test = 1.0;

vec4 read_vct_texture (vec3 texcoord, vec3 dir, float size) {
	float lod = log2(size);
	
	#if 1
	// prevent small samples from being way too blurry
	// -> simulate negative lods (size < 1.0) by snapping texture coords to nearest texels
	// when approaching size=0
	// size = 0.5 would snap [0.25,0.75] to 0.5
	//              and lerp [0.75,1.25] in [0.5,1.5]
	size = min(size, 1.0);
	texcoord = 0.5 * size + texcoord;
	texcoord = min(fract(texcoord) * (1.0 / size) - 0.5, 0.5) + floor(texcoord);
	#endif
	texcoord *= INV_WORLD_SIZEf;
	
	vec4 valX = textureLod(dir.x < 0.0 ? vct_texNX : vct_texPX, texcoord, lod);
	vec4 valY = textureLod(dir.y < 0.0 ? vct_texNY : vct_texPY, texcoord, lod);
	vec4 valZ = textureLod(dir.z < 0.0 ? vct_texNZ : vct_texPZ, texcoord, lod);
	
	vec3 sqr = dir * dir;
	vec4 val = (valX*sqr.x + valY*sqr.y + valZ*sqr.z) * VCT_UNPACK;
	
	//vec3 weight = min(abs(dir) * 15.0, 1.0);
	//valX.a *= weight.x;
	//valY.a *= weight.y;
	//valZ.a *= weight.z;
	
	val.a = max(max(valX.a, valY.a), valZ.a);
	
	return val;
}
vec4 trace_cone (vec3 cone_pos, vec3 cone_dir, float cone_slope, float start_dist, float max_dist, bool dbg) {
	
	float dist = start_dist;
	
	vec3 color = vec3(0.0);
	float transp = 1.0; // inverse alpha to support alpha stepsize fix
	
	for (int i=0; i<4000; ++i) {
		if (gl_LocalInvocationID.z == 0) {
			VISUALIZE_COST_COUNT
		}
		
		vec3 pos = cone_pos + cone_dir * dist;
		float size = cone_slope * 2.0 * dist;
		
		//if (true) {
		//	float lod = log2(size);
		//	lod = round(lod);
		//	//lod = max(lod, 0.0);
		//	
		//	float size_nearest = pow(2.0, lod);
		//	size = size_nearest;
		//	
		//	vec3 pos_nearest = pos;
		//	
		//	pos_nearest /= size_nearest;
		//	pos_nearest = floor(pos_nearest);
		//	pos_nearest += 0.5;
		//	pos_nearest *= size_nearest;
		//	
		//	vec3 dir = abs(cone_dir);
		//	float max_dir = max(max(dir.x, dir.y), dir.z);
		//	
		//	if      (max_dir == dir.x) pos.x = pos_nearest.x;
		//	else if (max_dir == dir.y) pos.y = pos_nearest.y;
		//	else                       pos.z = pos_nearest.z;
		//}
		
		float stepsize = size * vct_stepsize;
		
		vec4 sampl = read_vct_texture(pos, cone_dir, size);
		
		vec3 new_col = color + transp * sampl.rgb;
		float new_transp = transp - transp * sampl.a;
		//transp -= transp * pow(sampl.a, 1.0 / min(stepsize, 1.0));
		
		#if DEBUGDRAW
		if (_dbgdraw_rays && dbg) {
			//vec4 col = vec4(1,0,0,1);
			//vec4 col = vec4(sampl.rgb, 1.0-transp);
			//vec4 col = vec4(vec3(sampl.a), 1.0-transp);
			vec4 col = vec4(vec3(sampl.a), 1.0);
			dbgdraw_wire_cube(pos - WORLD_SIZEf/2.0, vec3(size), col);
		}
		#endif
		
		color = new_col;
		transp = new_transp;
		
		dist += stepsize;
		
		if (transp < 0.005 || dist >= max_dist)
			break;
	}
	
	//return vec4(vec3(dist / 300.0), 1.0);
	//return vec4(vec3(transp), 1.0);
	return vec4(color, 1.0 - transp);
}

layout(rgba16f, binding = 0) writeonly restrict uniform image2D output_color;
uniform vec2 dispatch_size;

// Instead of executing work groups in a simple row major order
// reorder them into columns of width N (by returning a different 2d index)
// in each column the work groups are still row major order
// replicates this: https://developer.nvidia.com/blog/optimizing-compute-shaders-for-l2-locality-using-thread-group-id-swizzling/
uvec2 work_group_tiling (uint N) {
	#if 0
	return gl_WorkGroupID.xy;
	#else
	uint idx = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
	
	uint column_size       = gl_NumWorkGroups.y * N;
	uint full_column_count = gl_NumWorkGroups.x / N;
	uint last_column_width = gl_NumWorkGroups.x % N;
	
	uint column_idx = idx / column_size;
	uint idx_in_column = idx % column_size;
	
	uint column_width = N;
	if (column_idx == full_column_count)
		column_width = last_column_width;
	
	uvec2 wg_swizzled;
	wg_swizzled.y = idx_in_column / column_width;
	wg_swizzled.x = idx_in_column % column_width + column_idx * N;
	return wg_swizzled;
	#endif
}

uvec2 get_pxpos () {
	ivec2 threadid = ivec2(gl_LocalInvocationID.xy);
	ivec2 wgroupid = ivec2(work_group_tiling(8u));
	
	ivec2 pxpos = wgroupid * ivec2(gl_WorkGroupSize.xy) + threadid;
	return pxpos;
}

#if VCT_DIFFUSE
	struct Geometry {
		bool did_hit;
		vec3 pos;
		mat3 TBN;
	};

	uniform sampler2D gbuf_pos ;
	//uniform sampler2D gbuf_col ;
	uniform sampler2D gbuf_norm;
	//uniform sampler2D gbuf_tang;

	Geometry read_gbuf (ivec2 pxpos) {
		vec2 uv = (vec2(pxpos) + 0.5) / dispatch_size;
		
		Geometry g;
		g.pos     = texture(gbuf_pos , uv).rgb;
		g.did_hit = g.pos.x >= -100.0;
		
		vec3 norm  = texture(gbuf_norm, uv).rgb;
		
		norm = normalize(norm);
		g.TBN = calc_TBN(norm, generate_tangent(norm));
		return g;
	}
	
	shared vec3 cone_results[WG_PIXELS_X*WG_PIXELS_Y][WG_CONES];
	
	void main () {
		uint threadid = gl_LocalInvocationID.y * WG_PIXELS_X + gl_LocalInvocationID.x;
		uint coneid   = gl_LocalInvocationID.z;
		
		ivec2 pxpos   = ivec2(get_pxpos());
		
		Geometry g = read_gbuf(pxpos);
		
		INIT_VISUALIZE_COST
		
		//#if DEBUGDRAW
		//_dbgdraw_rays = update_debugdraw && pxpos.x == uint(dispatch_size.x)/2 && pxpos.y == uint(dispatch_size.y)/2;
		//#endif
		
		if (g.did_hit) {
			Cone c = cones.cones[coneid];
			vec3 cone_dir = g.TBN * c.dir;
			
			vec3 res = trace_cone(g.pos, cone_dir, c.slope, vct_start_dist, 400.0, true).rgb * c.weight;
			
			cone_results[threadid][coneid] = res;
		}
		barrier();
		
		// Write out results for pixel
		if (coneid == 0u) {
			
			vec3 light = vec3(0.0);
			if (g.did_hit) {
				for (uint i=0u; i<WG_CONES; ++i)
					light += cone_results[threadid][i];
			}
			
			//light = g.TBN[0];
			
			GET_VISUALIZE_COST(light)
			imageStore(output_color, pxpos, vec4(light, 1.0));
		}
		
		#if DEBUGDRAW
		bool update_dbg_vecs = update_debugdraw && pxpos.x == uint(dispatch_size.x)/2 && pxpos.y == uint(dispatch_size.y)/2;
		
		if (update_dbg_vecs) {
			vec3 pos = g.pos - WORLD_SIZEf/2.0;
			
			// why does only the first vector appear???
			dbgdraw_vector(pos, g.TBN[2], vec4(0,1,1,1));
			dbgdraw_vector(pos, g.TBN[0], vec4(0,1,0,1));
		}
		#endif
	}
	
#else
	#if VCT_DBG_PRIMARY
	void main () {
		ivec2 pxpos   = ivec2(get_pxpos());
		INIT_VISUALIZE_COST
		
		#if DEBUGDRAW
		_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
		#endif
		
		vec3 ray_pos, ray_dir;
		get_ray(vec2(pxpos), ray_pos, ray_dir);
		
		float cone_slope = 1.0 / vct_test;
		vec3 col = trace_cone(ray_pos, ray_dir, cone_slope, 0.5, 400.0, true).rgb;
		
		GET_VISUALIZE_COST(col)
		imageStore(output_color, pxpos, vec4(col, 1.0));
	}

	//if (true) { // specular
	//	vec3 cone_dir = reflect(ray_dir, g.norm);
	//	
	//	float specular_strength = fresnel(-ray_dir, g.norm, 0.02) * 0.3;
	//	float cone_slope = 1.0 / vct_test;
	//	if (hit.bid == B_WATER) {
	//		hit.col *= 0.05;
	//	} else /*if (hit.bid == B_STONE)*/ {
	//		cone_slope = 1.0 / 8.0;
	//	}
	//	
	//	if (specular_strength > 0.0) { // specular
	//		light += trace_cone(g.pos, cone_dir, cone_slope, vct_start_dist, 400.0, true).rgb * specular_strength;
	//	}
	//}

	#else
	
	
	struct Geometry {
		bool did_hit;
		vec3 col;
		float emiss;
		vec3 pos;
		vec3 norm;
		//vec3 tang;
	};

	uniform sampler2D gbuf_pos ;
	uniform sampler2D gbuf_col ;
	uniform sampler2D gbuf_norm;
	//uniform sampler2D gbuf_tang;

	uniform sampler2D vct_diffuse;

	Geometry read_gbuf (ivec2 pxpos) {
		Geometry g;
		g.pos     = texelFetch(gbuf_pos , pxpos, 0).rgb;
		g.did_hit = g.pos.x >= -100.0;
		
		vec4 col  = texelFetch(gbuf_col , pxpos, 0).rgba;
		g.col = col.rgb;
		g.emiss = col.a;
		
		g.norm    = texelFetch(gbuf_norm, pxpos, 0).rgb;
		//g.tang    = texelFetch(gbuf_tang, pxpos, 0).rgb;
		return g;
	}
	
	void main () {
		ivec2 pxpos = ivec2(get_pxpos());
		vec2 pxuv = (vec2(pxpos) + 0.5) / dispatch_size;
		
		Geometry g = read_gbuf(pxpos);
		
		INIT_VISUALIZE_COST
		
		#if DEBUGDRAW
		_dbgdraw_rays = update_debugdraw && pxpos.x == uint(dispatch_size.x)/2 && pxpos.y == uint(dispatch_size.y)/2;
		#endif
		
		vec3 col = vec3(0.0);
		if (g.did_hit) {
			
			if (visualize_light) g.col = vec3(1.0);
			
			vec3 light = texture(vct_diffuse, pxuv).rgb;
			
			col = (light + g.emiss) * g.col;
		}
		
		GET_VISUALIZE_COST(col)
		imageStore(output_color, pxpos, vec4(col, 1.0));
	}
	#endif

#endif
