﻿#version 430 core // for findMSB, SSBO
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
	// Sparse Voxel Octree Raytracer
	// source for algorithm: https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010i3d_paper.pdf

	// SVO data
	uniform ivec3 svo_root_pos;
	uniform int svo_root_scale;

#define MAX_NODES (1u << 16)
#define BLOCK_ID	0
#define NODE_PTR	1
#define CHUNK_PTR	2

	struct SvoNode {
		uint16_t children_types; // uint16_t children_types
		uint16_t _pad[1];
		uint32_t children[8];
	};

	layout(std430, binding = 0) readonly restrict
	buffer SvoData {
		SvoNode* chunks_nodes[];
	} svo_data;

	uint get_svo_node (uint node_index, int child_index, out uint type) {
		//return texelFetch(svo_texture, ivec2(uvec2((node_index & 0xffff) * 8 + uint(child_index), node_index >> 16)), 0).r;

		uint chunk_index = node_index >> 16;
		node_index = node_index & 0xffffu;

		SvoNode node = svo_data.chunks_nodes[chunk_index][node_index];

		type = (uint)((node.children_types >> child_index*2) & 3u);
		return (uint)node.children[child_index];
	}

#define MAX_SCALE 16
#define MAX_SEC_RAYS 1

#define INF (1.0 / 0.0)
#define PI	3.1415926535897932384626433832795

// max dist of a ray before iteration will stop, also the depth distance of the sky,
// so make sure it is outside of the far plane, so that depth compositing with other drawpasses works
#define MAX_DIST 10000000.0

#define B_AIR 1 // block id
#define B_WATER 2 // block id
#define B_TALLGRASS 11 // block id
#define B_TORCH 10 // block id
	
	uniform float slider = 1.0; // debugging
	uniform bool visualize_iterations = false;
	uniform int max_iterations = 100; // iteration limiter for debugging
	uniform sampler2D gradients;

	uniform vec3 sun_col = vec3(1.0);
	uniform vec3 sun_dir = normalize(vec3(3, 4, 6));
	uniform float sun_radius = 0.1;

	uniform float water_F0 = 0.2;
	uniform float water_IOR = 1.333;

	uniform vec3 water_fog;
	uniform float water_fog_dens;

	uniform sampler2D water_normal;

	uniform vec2 water_scroll_dir1;
	uniform vec2 water_scroll_dir2;
	uniform float water_scale1;
	uniform float water_scale2;
	uniform float water_strength1;
	uniform float water_strength2;

	uniform float water_lod_bias;

	uniform float time;

#define air_IOR 1.0

	// Textures
	uniform	sampler2DArray tile_textures;
	uniform sampler1D block_tile_info; // BlockTileInfo: float4 { base_index, top, bottom, variants }

	// TODO: fix tangent & cotangents for water normal map
	void water_normal_map (inout vec3 normal, vec2 uv, float sign) {
		vec3 a = texture(water_normal, (uv + water_scroll_dir1 * time) * water_scale1, water_lod_bias).rgb * 2.0 - 1.0;
		a.xy *= water_strength1;

		vec3 b = texture(water_normal, (vec2(uv.y, -uv.x) + water_scroll_dir2 * time) * water_scale2, water_lod_bias).rgb * 2.0 - 1.0;
		b.xy *= water_strength2;

		normal = normalize(a + b + normal*0.2) * sign;
	}

	struct QueuedRay {
		vec3 pos;
		vec3 dir;
		vec4 tint;
	};

	int iterations = 0;
	uint cur_medium;

	float fresnel (vec3 view, vec3 norm, float F0) {
		//	float temp = 1.0 -dotVH;
		//	float tempSqr = squared(temp);
		//	fresnel = meshSpecularCol +((1.0 -meshSpecularCol) * (tempSqr * tempSqr * temp));
		float x = clamp(1.0 - dot(view, norm), 0.0, 1.0);
		float x2 = x*x;
		return F0 + ((1.0 - F0) * x2 * x2 * x);
	}
	
	vec3 hemisphere_sample () {
		// cosine weighted sampling (100% diffuse)
		// http://www.rorydriscoll.com/2009/01/07/better-sampling/

		vec2 uv = rand2();

		float r = sqrt(uv.y);
		float theta = 2*PI * uv.x;

		float x = r * cos(theta);
		float y = r * sin(theta);

		vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - uv.y)));
		return dir;
	}

	vec3 _normal;

	void surface_hit (
			vec3 ray_pos, vec3 ray_dir,
			float hit_dist, bvec3 entry_faces, uint block_id,
			inout vec4 accum_col, inout QueuedRay[MAX_SEC_RAYS] queue, inout int queued_ray, vec4 ray_tint) {
		if (cur_medium == -1) {
			cur_medium = block_id;
			return;
		}

		uint hit_id = block_id;

		if (block_id == B_AIR && cur_medium != B_AIR) {
			hit_id = cur_medium; // exit to air always rendered with medium texture
		}

		if ((hit_id == cur_medium && block_id == B_WATER)) {
			// only render entry of water, not all octree cubes
		} else if (hit_id == 0 || hit_id == B_AIR) {
			//
		} else if (hit_id == B_TALLGRASS && entry_faces.z) {
			//
		} else {

			vec3 hit_pos = ray_dir * hit_dist + ray_pos;

			vec3 pos_fract = hit_pos - floor(hit_pos);

			vec2 uv;

			if (entry_faces.x) {
				uv = pos_fract.yz;
			} else if (entry_faces.y) {
				uv = pos_fract.xz;
			} else {
				uv = pos_fract.xy;
			}

			uv.x *= (entry_faces.x && ray_dir.x > 0) || (entry_faces.y && ray_dir.y <= 0) ? -1 : 1;
			uv.y *=  entry_faces.z && ray_dir.z > 0 ? -1 : 1;

			vec3 normal = mix(vec3(0.0), vec3(1.0), entry_faces);
			normal *= step(ray_dir, vec3(0.0)) * 2.0 - 1.0;
			_normal = normal;

			vec4 bti = texelFetch(block_tile_info, int(hit_id), 0);

			float tex_indx = bti.x; // x=base_index
			if (entry_faces.z) {
				tex_indx += ray_dir.z <= 0 ? bti.y : bti.z; // y=top : z=bottom
			}

			vec4 col = texture(tile_textures, vec3(uv, tex_indx));
			//col.rgb = vec3(1,1,1);

			vec3 emmissive = vec3(0.0);
			if (block_id == B_TORCH) {
				col       = vec4(255,224,187,255);
				emmissive = vec3(255,224,187) / 255.0 * 50;
			}

			float alpha_remain = 1.0 - accum_col.a;

			if (	((block_id == B_WATER && cur_medium == B_AIR) || (block_id == B_AIR && cur_medium == B_WATER))
					&& queued_ray <= MAX_SEC_RAYS-2) {

				water_normal_map(normal, hit_pos.xy, normal.z);

				float src = cur_medium == B_AIR ? air_IOR : water_IOR;
				float dst = block_id == B_AIR ? air_IOR : water_IOR;

				float eta = src / dst;

				float reflect_fac = fresnel(-ray_dir, normal, water_F0);
				
				vec3 reflect_dir = reflect(ray_dir, normal);
				vec3 refract_dir = refract(ray_dir, normal, eta);

				if (dot(refract_dir, refract_dir) == 0.0) {
					// total internal reflection, ie. outside of snells window
					reflect_fac = 1.0;
				}

				if (queued_ray < MAX_SEC_RAYS) {
					queue[queued_ray].pos = hit_pos + reflect_dir * 0.0001;
					queue[queued_ray].dir = reflect_dir;
					queue[queued_ray].tint = ray_tint * reflect_fac * alpha_remain;
					queued_ray++;
				}

				if (queued_ray < MAX_SEC_RAYS) {
					queue[queued_ray].pos = hit_pos + refract_dir * 0.0001;
					queue[queued_ray].dir = refract_dir;
					queue[queued_ray].tint = ray_tint * (1.0 - reflect_fac) * alpha_remain;
					queued_ray++;
				}

				accum_col.a += alpha_remain;
			} else if (col.a >= 0.99 && queued_ray < MAX_SEC_RAYS) {
			
				mat3 tangent_to_world;
				{
					vec3 tangent = entry_faces.x ? vec3(0,1,0) : vec3(1,0,0);
					vec3 bitangent = cross(normal, tangent);
				
					tangent_to_world = mat3(tangent, bitangent, normal);
				}
				
				vec3 bounce_dir = tangent_to_world * hemisphere_sample();
				
				queue[queued_ray].pos = hit_pos + bounce_dir * 0.0001;
				queue[queued_ray].dir = bounce_dir;
				queue[queued_ray].tint = ray_tint * vec4(col.rgb, 1.0) * alpha_remain;
				queued_ray++;
			}

			if (accum_col.a <= 0.99999) {
				float effective_alpha = alpha_remain * col.a;

				//if (queued_ray < MAX_SEC_RAYS)
					accum_col.rgb += effective_alpha * col.rgb;
					accum_col.rgb += emmissive;

				accum_col.a += effective_alpha;
			}
		}

		cur_medium = block_id;
	}

	float min_component (vec3 v) {
		return min(min(v.x, v.y), v.z);
	}
	float max_component (vec3 v) {
		return max(max(v.x, v.y), v.z);
	}

	void intersect_ray (
			vec3 rinv_dir, vec3 mirror_ray_pos,
			ivec3 cube_pos, int cube_scale,
			out float t0, out float t1, out bvec3 exit_faces, out bvec3 entry_faces) {

		vec3 cube_min = vec3(cube_pos);
		vec3 cube_max = vec3(cube_pos + (1 << cube_scale));

		vec3 t0v = rinv_dir * (cube_min - mirror_ray_pos);
		vec3 t1v = rinv_dir * (cube_max - mirror_ray_pos);

		t0 = max_component( t0v );
		t1 = min_component( t1v );

		entry_faces = equal(vec3(t0), t0v);
		exit_faces = equal(vec3(t1), t1v);
	}

	void select_child (
			vec3 rinv_dir, vec3 mirror_ray_pos,
			float t0, ivec3 parent_pos, int parent_scale,
			out ivec3 child_pos, out int child_scale) {
		child_scale = parent_scale - 1;

		vec3 parent_mid = vec3(parent_pos + (1 << child_scale));

		vec3 tmid = rinv_dir * (parent_mid - mirror_ray_pos);

		ivec3 bits = ivec3( greaterThanEqual(vec3(t0), tmid) );

		child_pos.x = parent_pos.x | (bits.x << child_scale);
		child_pos.y = parent_pos.y | (bits.y << child_scale);
		child_pos.z = parent_pos.z | (bits.z << child_scale);
	}

	int highest_differing_bit (ivec3 a, ivec3 b) {
		return max(max(findMSB(a.x ^ b.x), findMSB(a.y ^ b.y)), findMSB(a.z ^ b.z));
	}

	vec3 process_ray (
			vec3 ray_pos, vec3 ray_dir,
			out float hit_dist, inout QueuedRay[MAX_SEC_RAYS] queue, inout int queued_rays, vec4 ray_tint) {
		hit_dist = MAX_DIST;
		vec4 accum_col = vec4(0.0);
		cur_medium = -1;

		//// init ray in mirrored coord system
		vec3 root_min = vec3(0);
		vec3 root_max = vec3(float(1 << svo_root_scale));
		vec3 root_mid = 0.5 * (root_min + root_max);

		int mirror_mask_int = 0;

		vec3 mirror_ray_pos = ray_pos;
		vec3 mirror_ray_dir;

		// mirror coord system to make all ray dir components positive, like in "An Efficient Parametric Algorithm for Octree Traversal"
		for (int i=0; i<3; ++i) {
			bool mirror = ray_dir[i] < 0;
		
			mirror_ray_dir[i] = abs(ray_dir[i]);
			if (mirror) mirror_ray_pos[i] = root_max[i] - ray_pos[i]; // mirror along mid  ie.  -1 * (ray_pos - mid) + mid -> 2*mid - ray_pos
			if (mirror) mirror_mask_int |= 1 << i;
		}
		
		vec3 rinv_dir = 1.0 / mirror_ray_dir;

		//// Init root
		ivec3 parent_pos = ivec3(0);
		int parent_scale = svo_root_scale;
		uint parent_node = 0u; // [16bit chunk index, 16bit node index]

		// desired ray bounds
		float tmin = 0;
		float tmax = MAX_DIST;

		// cube ray range
		float t0 = max_component( rinv_dir * (root_min - mirror_ray_pos) );
		float t1 = min_component( rinv_dir * (root_max - mirror_ray_pos) );

		t0 = max(tmin, t0);
		t1 = min(tmax, t1);

		ivec3 child_pos;
		int child_scale;
		select_child(rinv_dir, mirror_ray_pos, t0, parent_pos, parent_scale, child_pos, child_scale);

		uint stack_node[ MAX_SCALE -1 ];
		float stack_t1[ MAX_SCALE -1 ];

		//// Iterate
		for (;;) {
			iterations++;
			if (iterations >= max_iterations) {
				hit_dist = t1;
				break;
			}

			// child cube ray range
			float child_t0, child_t1;
			bvec3 exit_face, entry_faces;
			intersect_ray(rinv_dir, mirror_ray_pos, child_pos, child_scale, child_t0, child_t1, exit_face, entry_faces);

			if (t0 < t1) {

				int idx = 0;
				idx |= ((child_pos.x >> child_scale) & 1) << 0;
				idx |= ((child_pos.y >> child_scale) & 1) << 1;
				idx |= ((child_pos.z >> child_scale) & 1) << 2;

				idx ^= mirror_mask_int;

				uint type;
				uint node = get_svo_node(parent_node, idx, type);

				//// Intersect
				// child cube ray range
				float tv0 = max(child_t0, t0);
				float tv1 = min(child_t1, t1);

				//debug_print(int(type));
				//debug_print(int(node));

				if (tv0 < tv1) {
					
					if (type == BLOCK_ID) {
						float dist = tv0;

						surface_hit(ray_pos, ray_dir, dist, entry_faces, node, accum_col, queue, queued_rays, ray_tint);

						if (accum_col.a > 0.99999) {
							hit_dist = dist;
							break; // final hit
						}
					} else {

						uint chunk = parent_node & 0xffff0000u;
						if (type == CHUNK_PTR) {
							chunk = node << 16u;
							node = 0;
						}

						uint child_node = chunk | node;

						//// Push
						stack_node[child_scale - 1] = parent_node;
						stack_t1  [child_scale - 1] = t1;

						// child becomes parent
						parent_node = child_node;
						parent_pos = child_pos;
						parent_scale = child_scale;

						select_child(rinv_dir, mirror_ray_pos, tv0, parent_pos, parent_scale, child_pos, child_scale);

						t0 = tv0;
						t1 = tv1;

						continue;
					}
				}
			}

			ivec3 old_pos = child_pos;
			bool parent_changed;

			{ //// Advance
				ivec3 step = ivec3(exit_face);
				step.x <<= child_scale;
				step.y <<= child_scale;
				step.z <<= child_scale;

				parent_changed =
					(old_pos.x & step.x) != 0 ||
					(old_pos.y & step.y) != 0 ||
					(old_pos.z & step.z) != 0;

				child_pos += step;

				t0 = child_t1;
			}

			if (parent_changed) {
				//// Pop
				child_scale = highest_differing_bit(old_pos, child_pos);

				if (child_scale >= svo_root_scale) {
					accum_col.rgb += fog_color(ray_dir) * (1.0 - accum_col.a);
					break; // out of root
				}

				parent_node	= stack_node[child_scale - 1];
				t1			= stack_t1  [child_scale - 1];

				int clear_mask = ~((1 << child_scale) - 1);
				child_pos.x &= clear_mask;
				child_pos.y &= clear_mask;
				child_pos.z &= clear_mask;
			}
		}

		return accum_col.rgb;
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

		ray_pos -= vec3(svo_root_pos);
	}

	vec3 shadowed_ray (
			vec3 ray_pos, vec3 ray_dir,
			out float hit_dist, inout QueuedRay[MAX_SEC_RAYS] queue, inout int queued_rays, vec4 ray_tint) {

		vec3 col = process_ray(ray_pos, ray_dir, hit_dist, queue, queued_rays, ray_tint);

		//if (hit_dist < MAX_DIST) { // shadow ray
		//	vec3 hit_pos = ray_pos + ray_dir * hit_dist;
		//	hit_pos += _normal * 0.0005;
		//
		//	vec3 dir = normalize(sun_dir + (rand3() -0.5) * sun_radius);
		//
		//	int dummy_queued_rays = MAX_SEC_RAYS;
		//	float shadow_ray_dist;
		//	process_ray(hit_pos, dir, shadow_ray_dist, queue, dummy_queued_rays, vec4(1.0));
		//
		//	if (shadow_ray_dist < MAX_DIST) {
		//		// in shadow
		//		col *= 0;
		//	} else {
		//		col *= sun_col;
		//	}
		//	//col *= 0;
		//}

		return col;
	}

	void main () {
		vec3 ray_pos, ray_dir;
		get_ray(ray_pos, ray_dir);

		int queued_rays = MAX_SEC_RAYS;
		QueuedRay queue[MAX_SEC_RAYS];

		float hit_dist;
		vec3 accum_col = shadowed_ray(ray_pos, ray_dir, hit_dist, queue, queued_rays, vec4(1.0));
		
		for (int cur_ray = 0; cur_ray < queued_rays; ++cur_ray) {
			float _dist;

			vec3 ray_col = shadowed_ray(queue[cur_ray].pos, queue[cur_ray].dir, _dist, queue, queued_rays, queue[cur_ray].tint);
			accum_col += ray_col * queue[cur_ray].tint.rgb * queue[cur_ray].tint.a;
		}
		
		vec4 col = vec4(accum_col, 1.0);
		
		if (visualize_iterations)
			col = texture(gradients, vec2(float(iterations) / float(max_iterations), 0.25));

		FRAG_COL(col);

		// https://computergraphics.stackexchange.com/questions/6308/why-does-this-gl-fragdepth-calculation-work
		{ // Write depth of furthest hit surface of primary ray (=always opaque) or inf distance if not hit
			vec3 hit_pos_world = ray_pos + ray_dir * hit_dist + vec3(svo_root_pos);

			vec4 clip = world_to_clip * vec4(hit_pos_world, 1.0);
			gl_FragDepth = clip.z / clip.w;

			//FRAG_COL(vec4(vec3(gl_FragDepth), 1.0));
		}
	}
$endif