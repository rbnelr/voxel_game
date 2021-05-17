#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y, local_size_z = WG_CONES) in;

#define RAND_SEED_TIME 1

#include "rt_util.glsl"

layout(rgba16f, binding = 0) writeonly restrict uniform image2D output_color;

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

vec4 _compute_mip0 (ivec3 vox_pos) {
	//uint bid = read_bid(vox_pos);
	uint bid = texelFetch(octree, vox_pos, 0).r;
	
	//float texid = float(block_tiles[bid].sides[0]);
	//vec4 col = textureLod(tile_textures, vec3(vec2(0.5), texid), 100.0);
	int texid = block_tiles[bid].sides[0];
	vec4 col = texelFetch(tile_textures, ivec3(0,0,texid), 4);
	
	//if (bid == B_MAGMA) col = vec4(0.0);
	
	//uint bidNX = read_bid(dst_pos + ivec3(-1,0,0));
	//uint bidPX = read_bid(dst_pos + ivec3(+1,0,0));
	//uint bidNY = read_bid(dst_pos + ivec3(0,-1,0));
	//uint bidPY = read_bid(dst_pos + ivec3(0,+1,0));
	//uint bidNZ = read_bid(dst_pos + ivec3(0,0,-1));
	//uint bidPZ = read_bid(dst_pos + ivec3(0,0,+1));
	//
	//bool blocked =
	//	(bidNX != B_AIR) && (bidPX != B_AIR) &&
	//	(bidNY != B_AIR) && (bidPY != B_AIR) &&
	//	(bidNZ != B_AIR) && (bidPZ != B_AIR);
	//bool blocked = false;
	
	if (bid == B_AIR) return vec4(0.0);
	return vec4(col.rgb * get_emmisive(bid), 1.0);
}
vec4 compute_mip0 (vec3 texcoord) {
	texcoord *= WORLD_SIZEf;
	texcoord -= 0.4999;
	
	vec3 t = fract(texcoord);
	ivec3 pos = ivec3(floor(texcoord));
	
	vec4 v000 = _compute_mip0(ivec3(pos + ivec3(0,0,0)));
	//return v000;
	//if (t.x < 0.001 && t.y < 0.001 && t.z < 0.001) return v000;
	
	vec4 v100 = _compute_mip0(ivec3(pos + ivec3(1,0,0)));
	vec4 v010 = _compute_mip0(ivec3(pos + ivec3(0,1,0)));
	vec4 v110 = _compute_mip0(ivec3(pos + ivec3(1,1,0)));
	vec4 v001 = _compute_mip0(ivec3(pos + ivec3(0,0,1)));
	vec4 v101 = _compute_mip0(ivec3(pos + ivec3(1,0,1)));
	vec4 v011 = _compute_mip0(ivec3(pos + ivec3(0,1,1)));
	vec4 v111 = _compute_mip0(ivec3(pos + ivec3(1,1,1)));
	
	vec4 v00 = mix(v000, v100, vec4(t.x));
	vec4 v10 = mix(v010, v110, vec4(t.x));
	vec4 v01 = mix(v001, v101, vec4(t.x));
	vec4 v11 = mix(v011, v111, vec4(t.x));
	
	vec4 v0 = mix(v00, v10, vec4(t.y));
	vec4 v1 = mix(v01, v11, vec4(t.y));
	
	vec4 val = mix(v0, v1, vec4(t.z));
	return val;
}

vec4 read_vct_texture (vec3 texcoord, vec3 dir, float size) {
	float lod = log2(size);
	
	ivec3 vox_pos = ivec3(floor(texcoord));
	#if 1
	// support negative lod (size < 1.0) by snapping texture coords to nearest texels
	// when approaching size=0
	// size = 0.5 would snap [0.25,0.75] to 0.5
	//              and lerp [0.75,1.25] in [0.5,1.5]
	size = min(size, 1.0);
	texcoord = 0.5 * size + texcoord;
	texcoord = min(fract(texcoord) * (1.0 / size) - 0.5, 0.5) + floor(texcoord);
	#endif
	texcoord *= INV_WORLD_SIZEf;
	
	// TODO: is there a way to share mip0 for 6 texture somehow?
	// one possibility might be sparse textures where you manually assign the same pages to all the first mips
	// according to only possible with glTexturePageCommitmentMemNV which actually requires using a vulkan instance to allocate the memory for the pages?
	
	if (lod < 1.0) {
		// sample mip 0
		vec4 val0 = vct_unpack( textureLod(vct_basetex, texcoord, 0.0) );
		//vec4 val0 = compute_mip0(texcoord);
		
		if (lod <= 0.0) return val0; // no need to also sample larger mip
		
		// sample mip 1
		vec4 valX = vct_unpack( textureLod(dir.x < 0.0 ? vct_texNX : vct_texPX, texcoord, 0.0) );
		vec4 valY = vct_unpack( textureLod(dir.y < 0.0 ? vct_texNY : vct_texPY, texcoord, 0.0) );
		vec4 valZ = vct_unpack( textureLod(dir.z < 0.0 ? vct_texNZ : vct_texPZ, texcoord, 0.0) );
		
		vec4 val1 = valX*abs(dir.x) + valY*abs(dir.y) + valZ*abs(dir.z);
		
		// interpolate mip0 and mip1
		return mix(val0, val1, lod);
	} else {
		// rely on hardware mip interpolation
		vec4 valX = vct_unpack( textureLod(dir.x < 0.0 ? vct_texNX : vct_texPX, texcoord, lod-1.0) );
		vec4 valY = vct_unpack( textureLod(dir.y < 0.0 ? vct_texNY : vct_texPY, texcoord, lod-1.0) );
		vec4 valZ = vct_unpack( textureLod(dir.z < 0.0 ? vct_texNZ : vct_texPZ, texcoord, lod-1.0) );
		
		return valX*abs(dir.x) + valY*abs(dir.y) + valZ*abs(dir.z);
	}
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
		
		vec4 sampl = read_vct_texture(pos, cone_dir, size);
		
		color += transp * sampl.rgb;
		transp -= transp * sampl.a;
		
		#if DEBUGDRAW
		if (_debugdraw && dbg) {
			//dbgdraw_wire_cube(pos - WORLD_SIZEf/2.0, vec3(size), vec4(1,0,0,1));
			//dbgdraw_wire_cube(pos - WORLD_SIZEf/2.0, vec3(size), vec4(transp * sampl.rgb, 1.0-transp));
			dbgdraw_wire_cube(pos - WORLD_SIZEf/2.0, vec3(size), vec4(vec3(sampl.a), 1.0));
		}
		#endif
		
		dist += size * vct_stepsize;
		
		if (transp < 0.01 || dist >= max_dist)
			break;
	}
	
	//return vec4(vec3(dist / 300.0), 1.0);
	//return vec4(vec3(transp), 1.0);
	return vec4(color, 1.0 - transp);
}

uniform sampler2D gbuf_pos ;
uniform sampler2D gbuf_col ;
uniform sampler2D gbuf_norm;
uniform sampler2D gbuf_tang;

struct Geometry {
	bool did_hit;
	vec3 col;
	float emiss;
	vec3 pos;
	vec3 norm;
	vec3 tang;
};
Geometry read_geom (ivec2 pxpos) {
	Geometry g;
	g.pos     = texelFetch(gbuf_pos , pxpos, 0).rgb;
	g.did_hit = g.pos.x >= -100.0;
	
	vec4 col  = texelFetch(gbuf_col , pxpos, 0).rgba;
	g.col = col.rgb;
	g.emiss = col.a;
	
	g.norm    = texelFetch(gbuf_norm, pxpos, 0).rgb;
	g.tang    = texelFetch(gbuf_tang, pxpos, 0).rgb;
	return g;
}

shared vec3 cone_results[WG_PIXELS_X*WG_PIXELS_Y][WG_CONES];

void main () {
	uint threadid = gl_LocalInvocationID.y * WG_PIXELS_X + gl_LocalInvocationID.x;
	uint coneid   = gl_LocalInvocationID.z;
	
	ivec2 pxpos   = ivec2(gl_GlobalInvocationID.xy);
	
	//#if DEBUGDRAW
	//	_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	//#endif
	
	Geometry g = read_geom(pxpos);
	
	INIT_VISUALIZE_COST
	
	if (g.did_hit) {
		vec3 bitang = cross(g.norm, g.tang);
		mat3 TBN = mat3(g.tang, bitang, g.norm);
		
		Cone c = cones.cones[coneid];
		vec3 cone_dir = TBN * c.dir;
		
		vec3 res = trace_cone(g.pos, cone_dir, c.slope, vct_start_dist, 400.0, true).rgb * c.weight;
		
		cone_results[threadid][coneid] = res;
	}
	barrier();
	
	// Write out results for pixel
	if (coneid == 0u) {
		
		vec3 col = vec3(0.0);
		if (g.did_hit) {
			
			vec3 light = vec3(0.0);
			for (uint i=0u; i<WG_CONES; ++i)
				light += cone_results[threadid][i];
			
			if (visualize_light)
				g.col = vec3(1.0);
			col = (light + g.emiss) * g.col;
		}
		
		GET_VISUALIZE_COST(col)
		imageStore(output_color, pxpos, vec4(col, 1.0));
	}
}

