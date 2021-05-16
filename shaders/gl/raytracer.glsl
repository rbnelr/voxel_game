#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y, local_size_z = WG_CONES   ) in;

#define RAND_SEED_TIME 1

#include "rt_util.glsl"

layout(rgba16f, binding = 5) writeonly restrict uniform image2D output_color;

#if TAA_ENABLE
layout(rgba16f, binding = 6) writeonly restrict uniform image2D taa_color;
layout(r16ui  , binding = 7) writeonly restrict uniform uimage2D taa_posage;

uniform  sampler2D taa_history_color;
uniform usampler2D taa_history_posage;

uniform mat4 prev_world2clip;
uniform float taa_alpha = 0.05;
#endif

uniform uint rand_frame_index = 0;

// get pixel ray in world space based on pixel coord and matricies
bool get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {
	
#if 1 // Normal camera projection
	//vec2 px_center = px_pos + rand2();
	vec2 px_center = px_pos + vec2(0.5);
	vec2 ndc = px_center / view.viewport_size * 2.0 - 1.0;
	
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	// -> clip_to_cam needs translation  cam_to_world needs to _not_ have translation
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
	
	// ray starts on the near plane
	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += float(WORLD_SIZE/2);
	
	return true;

#else // 360 Sphere Projections
	
	vec2 px_center = (px_pos + vec2(0.5)) / view.viewport_size; // [0,1]
	
	#if 0 // Equirectangular projection
		float lon = (px_center.x - 0.5) * PI*2;
		float lat = (px_center.y - 0.5) * PI;
	#else // Mollweide projection
		float x = px_center.x * 2.0 - 1.0;
		float y = px_center.y * 2.0 - 1.0;
		
		if ((x*x + y*y) > 1.0)
			return false;
		
		float theta = asin(y);
		
		float lon = (PI * x) / cos(theta);
		float lat = asin((2.0 * theta + sin(2.0 * theta)) / PI);
	#endif
	
	float c = cos(lat);
	vec3 dir_cam = vec3(c * sin(lon), sin(lat), -c * cos(lon));
	
	ray_dir = (view.cam_to_world * vec4(dir_cam, 0)).xyz;
	ray_pos = (view.cam_to_world * vec4(0,0,0,1)).xyz;
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += float(WORLD_SIZE/2);

	return true;
#endif
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

#if VCT
	////// VCT
	
#if WG_CONES > 1
	struct Geometry {
		bool did_hit;
		vec3 pos;
		vec3 normal;
		vec3 tangent;
	};
	shared Geometry geom         [WG_PIXELS_X*WG_PIXELS_Y];
	shared vec3     cone_results [WG_PIXELS_X*WG_PIXELS_Y][WG_CONES];
#endif

#if WG_CONES > 1
void main () {
	uint threadid = gl_LocalInvocationID.y * WG_PIXELS_X + gl_LocalInvocationID.x;
	uint coneid   = gl_LocalInvocationID.z;
	
	uvec2 pxpos    = gl_GlobalInvocationID.xy;
	
	//#if DEBUGDRAW
	//	_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	//#endif
	
	//INIT_VISUALIZE_COST
	
	Hit hit;
	if (coneid == 0u) {
		// primary ray
		vec3 ray_pos, ray_dir;
		bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
		
		// Raytrace to get our hit from which to cast cones (redunantly for every cone)
		// TODO: test if a seperate (gbuffer) pass could be better
		uint start_bid = read_bid(ivec3(floor(ray_pos)));
		//uint start_bid = B_AIR;
		
		bool did_hit = bray && trace_ray(ray_pos, ray_dir, INF, start_bid, hit, RAYT_PRIMARY);
		
		geom[threadid].did_hit = did_hit;
		if (did_hit) {
			geom[threadid].pos = hit.pos;
			geom[threadid].normal = hit.TBN[2];
			geom[threadid].tangent = hit.TBN[0];
		}
	}
	
	INIT_VISUALIZE_COST
	barrier();
	
	if (geom[threadid].did_hit) {
		vec3 cone_pos = geom[threadid].pos;
		vec3 normal   = geom[threadid].normal;
		vec3 tangent  = geom[threadid].tangent;
		
		vec3 bitangent = cross(normal, tangent);
		mat3 TBN = mat3(tangent, bitangent, normal);
		
		Cone c = cones.cones[coneid];
		vec3 cone_dir = TBN * c.dir;
		
		vec3 res = trace_cone(cone_pos, cone_dir, c.slope, vct_start_dist, 400.0, true).rgb * c.weight;
		
		cone_results[threadid][coneid] = res;
	}
	barrier();
	
	// Write out results for pixel
	if (coneid == 0u) {
		
		vec3 col = vec3(0.0);
		if (geom[threadid].did_hit) {
			
			vec3 light = vec3(0.0);
			for (uint i=0u; i<WG_CONES; ++i)
				light += cone_results[threadid][i];
			
			if (visualize_light)
				hit.col = vec3(1.0);
			col = light * hit.col + hit.emiss;
		}
		
		GET_VISUALIZE_COST(col)
		imageStore(output_color, ivec2(pxpos), vec4(col, 1.0));
	}
}
#else
void main () {
	uvec2 pxpos    = gl_GlobalInvocationID.xy;
	
	INIT_VISUALIZE_COST
	
	#if DEBUGDRAW
		_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	#endif
	srand(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, rand_frame_index);
	//srand(0, 0, rand_frame_index);
	
	
	// primary ray
	vec3 cone_pos, cone_dir;
	bool bray = get_ray(vec2(pxpos), cone_pos, cone_dir);
	float max_dist = INF;
	
	uint start_bid = read_bid(ivec3(floor(cone_pos)));
	
	#if VCT_DBG_PRIMARY
		// primary cone for debugging
		float start_dist = 1.0;
		max_dist = 400.0;
		float cone_slope = 1.0 / vct_test;
		vec3 col = trace_cone(cone_pos, cone_dir, cone_slope, start_dist, max_dist, true).rgb;
	#else
		Hit hit;
		bool did_hit = bray && trace_ray(cone_pos, cone_dir, max_dist, start_bid, hit, RAYT_PRIMARY);
		
		INIT_VISUALIZE_COST
		
		vec3 col = vec3(0.0);
		if (did_hit) {
			float start_dist = vct_start_dist;
			//vec3 dir = hit.normal;
			//vec3 dir = reflect(view_ray_dir, hit.normal);
			
			max_dist = 400.0;
			vec3 light = vec3(0.0);
			
			{ // diffuse
				for (int i=0; i<cones.count.x; ++i) {
					Cone c = cones.cones[i];
					vec3 cone_dir = hit.TBN * c.dir;
					light += trace_cone(hit.pos, cone_dir, c.slope, start_dist, max_dist, true).rgb * c.weight;
				}
				
				if (visualize_light)
					hit.col = vec3(1.0);
				col = light * hit.col + hit.emiss;
			}
			
			//{ // specular
			//	vec3 cone_dir = reflect(view_ray_dir, hit.TBN[2]);
			//	
			//	float specular_strength = fresnel(-view_ray_dir, hit.TBN[2], 0.02) * 0.3;
			//	float cone_slope = 1.0 / vct_test;
			//	if (hit.bid == B_WATER) {
			//		hit.col *= 0.05;
			//	} else /*if (hit.bid == B_STONE)*/ {
			//		cone_slope = 1.0 / 8.0;
			//	}
			//	
			//	if (specular_strength > 0.0) { // specular
			//		col += trace_cone(hit.pos, cone_dir, cone_slope, start_dist, max_dist, true).rgb * specular_strength;
			//	}
			//}
		}
	#endif
	
	GET_VISUALIZE_COST(col)
	imageStore(output_color, ivec2(pxpos), vec4(col, 1.0));
}
#endif

#else
	////// Raytracer
	void main () {
		INIT_VISUALIZE_COST
		uvec2 pxpos = gl_GlobalInvocationID.xy;
		
	#if DEBUGDRAW
		_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	#endif
		srand(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, rand_frame_index);
		//srand(0, 0, rand_frame_index);
		
		// primary ray
		vec3 ray_pos, ray_dir;
		bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
		float max_dist = INF;
		
		uint start_bid = read_bid(ivec3(floor(ray_pos)));
		
		vec3 col = vec3(0.0);
	
	#if ONLY_PRIMARY_RAYS
		Hit hit;
		if (bray && trace_ray(ray_pos, ray_dir, max_dist, start_bid, hit, RAYT_PRIMARY))
			col = hit.col;
	#else
		vec3 surf_light = vec3(0.0); // light on surface for taa write
		
		#if TAA_ENABLE
		uint surf_position = 0xffffu;
		uint age = 0;
		#endif
		
		Hit hit;
		bool was_reflected = false;
		if (bray && trace_ray_refl_refr(ray_pos, ray_dir, max_dist, start_bid, hit, was_reflected, RAYT_PRIMARY)) {
			ray_pos = hit.pos + hit.TBN[2] * 0.001;
			
			surf_light = collect_sunlight(ray_pos, hit.TBN[2]);
			
			#if 1 // specular test
			if (bounces_enable) {
				max_dist = bounces_max_dist;
				
				vec3 cur_pos = ray_pos;
				mat3 TBN = hit.TBN;
				vec3 contrib = vec3(hit.occl_spec.y);
				
				float roughness = 0.8;
				
				vec2 rand = rand2();
				
				float theta = atan(roughness * sqrt(rand.x) / sqrt(1.0 - rand.x));
				float phi = 2.0 * PI * rand.y;
				
				vec3 normal = from_spherical(theta, phi, 1.0);
				normal = TBN * normal;
				
				ray_dir = reflect(ray_dir, normal);
				
				if (dot(ray_dir, TBN[2]) >= 0.0) {
					for (int j=0; j<bounces_max_count-1; ++j) {
						bool was_reflected2;
						Hit hit2;
						if (!trace_ray_refl_refr(cur_pos, ray_dir, max_dist, hit.medium, hit2, was_reflected2, RAYT_SPECULAR))
							break;
						
						cur_pos = hit2.pos + TBN[2] * 0.001;
						max_dist -= hit2.dist;
						
						TBN = hit2.TBN;
						
						ray_dir = TBN * hemisphere_sample(); // already cos weighted
						
						vec3 light2 = collect_sunlight(cur_pos, TBN[2]);
						
						surf_light += (hit2.emiss + hit2.col * light2) * contrib;
						contrib *= hit2.col;
					}
				}
			}
			#endif
			
			if (bounces_enable) {
				//int count = 8;
				//for (int i=0; i<count; ++i) {
					max_dist = bounces_max_dist;
					
					vec3 cur_pos = ray_pos;
					mat3 TBN = hit.TBN;
					vec3 contrib = vec3(hit.occl_spec.x);
					
					for (int j=0; j<bounces_max_count; ++j) {
						ray_dir = TBN * hemisphere_sample(); // already cos weighted
						
						bool was_reflected2;
						Hit hit2;
						if (!trace_ray_refl_refr(cur_pos, ray_dir, max_dist, hit.medium, hit2, was_reflected2, RAYT_DIFFUSE))
							break;
						
						TBN = hit2.TBN;
						
						cur_pos = hit2.pos + TBN[2] * 0.001;
						max_dist -= hit2.dist;
						
						vec3 light2 = collect_sunlight(cur_pos, TBN[2]);
						
						surf_light += (hit2.emiss + hit2.col * light2) * contrib;
						contrib *= hit2.col;
					}
				//}
				//surf_light /= float(count);
			}
			
			#if TAA_ENABLE
			if (!was_reflected) {
				hit.pos -= float(WORLD_SIZE/2);
				
				uvec3 rounded = uvec3(ivec3(round(hit.pos))) & 0x3fffu;
				if      (abs(hit.TBN[2].x) > 0.9) surf_position =           rounded.x;
				else if (abs(hit.TBN[2].y) > 0.9) surf_position = 0x4000u | rounded.y;
				else                              surf_position = 0xc000u | rounded.z;
				
				vec4 prev_clip = prev_world2clip * vec4(hit.pos, 1.0);
				prev_clip.xyz /= prev_clip.w;
				
				vec2 uv = prev_clip.xy * 0.5 + 0.5;
				if (all(greaterThan(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)))) {
					uvec2 sampl = texture(taa_history_posage, uv).rg;
					
					uint sampl_pos = sampl.r;
					uint sampl_age = sampl.g;
					
					if (surf_position == sampl_pos) {
						uint max_age = (uint)round((1.0 / taa_alpha) - 1.0);
						
						age = min(sampl_age, max_age);
						float alpha = 1.0 / (float(age) + 1.0);
						
						vec3 prev_light = texture(taa_history_color, uv).rgb;
						surf_light = mix(prev_light, surf_light, vec3(alpha));
					}
				}
			}
			#endif
			
			if (visualize_light)
				hit.col = vec3(1.0);
			col = hit.emiss + hit.col * surf_light;
		}
		
		#if TAA_ENABLE
		//col = vec3(float(age) / 100.0);
		
		imageStore(taa_color,  ivec2(pxpos), vec4(surf_light, 0.0));
		imageStore(taa_posage, ivec2(pxpos), uvec4(surf_position, age + 1,0,0));
		#endif
		
	#endif
		
		GET_VISUALIZE_COST(col)
		imageStore(output_color, ivec2(pxpos), vec4(col, 1.0));
	}
#endif
