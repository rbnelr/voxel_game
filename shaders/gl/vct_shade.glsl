 #version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

#if TEST || VCT_DBG_PRIMARY
layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;
#else
layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y, local_size_z = WG_CONES) in;
#endif

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
		if (_debugdraw && dbg) {
			//vec4 col = vec4(1,0,0,1);
			vec4 col = vec4(sampl.rgb, 1.0-transp);
			//vec4 col = vec4(vec3(sampl.a), 1.0-transp);
			//vec4 col = vec4(vec3(sampl.a), 1.0);
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

struct Geometry {
	bool did_hit;
	vec3 col;
	float emiss;
	vec3 pos;
	vec3 norm;
	vec3 tang;
};

#if VCT_DBG_PRIMARY
void main () {
	ivec2 pxpos   = ivec2(gl_GlobalInvocationID.xy);
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
#elif TEST

// All components are in the range [0…1], including hue.
vec3 rgb2hsv (vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0…1], including hue.
vec3 hsv2rgb (vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main () {
	ivec2 pxpos   = ivec2(gl_GlobalInvocationID.xy);
	
	//srand(pxpos.x, pxpos.y, 0);
	
	//#if DEBUGDRAW
	//	_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	//#endif
	
	Geometry g;
	
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	uint start_bid = read_bid_octree(ivec3(floor(ray_pos)));
	
	Hit hit;
	g.did_hit = bray && trace_ray(ray_pos, ray_dir, INF, start_bid, hit, RAYT_PRIMARY);
	
	if (g.did_hit) {
		g.pos   = hit.pos;
		g.col   = hit.col;
		g.emiss = hit.emiss;
		g.norm  = hit.TBN[2];
		g.tang  = hit.TBN[0];
	}
	
	INIT_VISUALIZE_COST
	
	vec3 col = vec3(0.0);
	
	if (g.did_hit) {
		
		//float r = 1.0 / 4.0;
		//g.pos = round(g.pos / r) * r;
		
		g.pos = g.pos + hit.TBN[2] * 0.01;
		
		vec3 light = vec3(0.0);
		
		for (int coneid=0; coneid<12; ++coneid) {
			vec3 bitang = cross(g.norm, g.tang);
			mat3 TBN = mat3(g.tang, bitang, g.norm);
			
			Cone c = cones.cones[coneid];
			vec3 cone_dir = TBN * c.dir;
			
			light += trace_cone(g.pos, cone_dir, c.slope, vct_start_dist, 400.0, true).rgb * c.weight;
		}
		
		if (true) { // specular
			vec3 cone_dir = reflect(ray_dir, g.norm);
			
			float specular_strength = fresnel(-ray_dir, g.norm, 0.02) * 0.3;
			float cone_slope = 1.0 / vct_test;
			if (hit.bid == B_WATER) {
				hit.col *= 0.05;
			} else /*if (hit.bid == B_STONE)*/ {
				cone_slope = 1.0 / 8.0;
			}
			
			if (specular_strength > 0.0) { // specular
				light += trace_cone(g.pos, cone_dir, cone_slope, vct_start_dist, 400.0, true).rgb * specular_strength;
			}
		}
		
		//{
		//	float s = 16.0;
		//	vec3 hsv = rgb2hsv(light);
		//	
		//	hsv.z = floor(hsv.z * s) / s;
		//	
		//	light = hsv2rgb(hsv);
		//}
		
		if (visualize_light)
			g.col = vec3(1.0);
		col = (light + g.emiss) * g.col;
	}
	
	GET_VISUALIZE_COST(col)
	imageStore(output_color, pxpos, vec4(col, 1.0));
}

#else
uniform sampler2D gbuf_pos ;
uniform sampler2D gbuf_col ;
uniform sampler2D gbuf_norm;
uniform sampler2D gbuf_tang;

Geometry read_gbuf (ivec2 pxpos) {
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
	
	Geometry g = read_gbuf(pxpos);
	
	INIT_VISUALIZE_COST
	
	#if DEBUGDRAW
	_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	#endif
	
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
			//col = g.col;
			col = (light + g.emiss) * g.col;
		}
		
		GET_VISUALIZE_COST(col)
		imageStore(output_color, pxpos, vec4(col, 1.0));
	}
}
#endif
