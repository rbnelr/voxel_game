#version 400 core // for findMSB

$include "common.glsl"
$include "fog.glsl"

$if vertex
	// Fullscreen quad
	const vec4[] pos_clip = vec4[] (
		vec4(+1,-1, 0, 1),
		vec4(+1,+1, 0, 1),
		vec4(-1,-1, 0, 1),
		vec4(-1,-1, 0, 1),
		vec4(+1,+1, 0, 1),
		vec4(-1,+1, 0, 1)
	);

	void main () {
		gl_Position = pos_clip[gl_VertexID];
	}
$endif

$if fragment
	// Sparse Voxel Octree Raytracer
	// source for algorithm: https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010i3d_paper.pdf

	// SVO data
	uniform isampler2D svo_texture;
	uniform ivec3 svo_root_pos;
	uniform int svo_root_scale;

	/* SVO format:
		union OctreeNode {
			uint32 children_index; // 0x80000000 set if has_children, ie. this node a non-leaf node -> mask away MSB to get index into svo_texture where the nodes for the 8 children lie contiguously
			uint16 block_id; // block id (valid when leaf node)
		};

		1 bit         31 bit
		[has_children][payload]

		has_children == 0: payload is index of children OctreeNode[8] in svo_texture
		has_children == 1: payload leaf voxel data, ie. block id
	*/

	int get_svo_node (int index) {
		// access 1d data as 2d texture, because of texture size limits
		ivec2 indx2;
		indx2.x = index % 2048;
		indx2.y = index / 2048;
		return texelFetch(svo_texture, indx2, 0).r;
	}

//#define LEAF_BIT 0x80000000 // This triggers an integer overflow on intel??
#define LEAF_BIT (1 << 31) // This works

#define MAX_SCALE 12
#define MAX_SEC_RAYS 2

#define INF (1.0 / 0.0)

#define B_AIR 1 // block id
#define B_WATER 2 // block id
	
	uniform float slider = 1.0; // debugging
	uniform bool visualize_iterations = false;
	uniform int max_iterations = 100; // iteration limiter for debugging
	uniform sampler2D heat_gradient;

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
	int cur_medium;

	float fresnel (vec3 view, vec3 norm, float F0) {
		//	float temp = 1.0 -dotVH;
		//	float tempSqr = squared(temp);
		//	fresnel = meshSpecularCol +((1.0 -meshSpecularCol) * (tempSqr * tempSqr * temp));
		float x = clamp(1.0 - dot(view, norm), 0.0, 1.0);
		float x2 = x*x;
		return F0 + ((1.0 - F0) * x2 * x2 * x);
	}

	void surface_hit (
			vec3 ray_pos, vec3 ray_dir,
			float hit_dist, bvec3 entry_faces, int block_id,
			inout vec4 accum_col, inout QueuedRay[MAX_SEC_RAYS] queue, inout int queued_ray, vec4 ray_tint) {
		if (cur_medium == 0) {
			cur_medium = block_id;
			return;
		}

		int effective_block_id = block_id;

		if ((block_id == cur_medium && block_id == B_WATER)) {
			// only render entry of water, not all octree cubes
		} else if ((block_id == B_AIR && cur_medium != B_WATER) || block_id == 0) {
			// never try to render air
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

			vec4 bti = texelFetch(block_tile_info, effective_block_id, 0);

			float tex_indx = bti.x; // x=base_index
			if (entry_faces.z) {
				tex_indx += ray_dir.z <= 0 ? bti.y : bti.z; // y=top : z=bottom
			}

			vec4 col = texture(tile_textures, vec3(uv, tex_indx));

			float alpha_remain = 1.0 - accum_col.a;

			if (	((block_id == B_WATER && cur_medium == B_AIR) || (block_id == B_AIR && cur_medium == B_WATER))
					&& queued_ray <= MAX_SEC_RAYS-2) {

				vec3 normal = mix(vec3(0.0), vec3(1.0), entry_faces);
				normal *= step(ray_dir, vec3(0.0)) * 2.0 - 1.0;

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
			} else {
				float effective_alpha = alpha_remain * col.a;
				accum_col += vec4(effective_alpha * col.rgb, effective_alpha);
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

	float rand (vec2 co){
		return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
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
		hit_dist = INF;
		vec4 accum_col = vec4(0.0);
		cur_medium = 0;

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
		int parent_node = 0;

		// desired ray bounds
		float tmin = 0;
		float tmax = INF;

		// cube ray range
		float t0 = max_component( rinv_dir * (root_min - mirror_ray_pos) );
		float t1 = min_component( rinv_dir * (root_max - mirror_ray_pos) );

		t0 = max(tmin, t0);
		t1 = min(tmax, t1);

		ivec3 child_pos;
		int child_scale;
		select_child(rinv_dir, mirror_ray_pos, t0, parent_pos, parent_scale, child_pos, child_scale);

		int stack_node[ MAX_SCALE -1 ];
		float stack_t1[ MAX_SCALE -1 ];

		//// Iterate
		for (;;) {
			iterations++;
			if (iterations >= max_iterations) break;

			// child cube ray range
			float child_t0, child_t1;
			bvec3 exit_face, entry_faces;
			intersect_ray(rinv_dir, mirror_ray_pos, child_pos, child_scale, child_t0, child_t1, exit_face, entry_faces);

			bool voxel_exists = (parent_node & LEAF_BIT) == 0;
			if (voxel_exists && t0 < t1) {

				int idx = 0;
				idx |= ((child_pos.x >> child_scale) & 1) << 0;
				idx |= ((child_pos.y >> child_scale) & 1) << 1;
				idx |= ((child_pos.z >> child_scale) & 1) << 2;

				idx ^= mirror_mask_int;

				int node = get_svo_node( parent_node*8 + idx );

				//// Intersect
				// child cube ray range
				float tv0 = max(child_t0, t0);
				float tv1 = min(child_t1, t1);

				if (tv0 < tv1) {
					
					bool leaf = (node & LEAF_BIT) != 0;
					if (leaf) {
						hit_dist = tv0;
						surface_hit(ray_pos, ray_dir, hit_dist, entry_faces, node & ~LEAF_BIT,
							accum_col, queue, queued_rays, ray_tint);
						
						if (accum_col.a > 0.99999)
							break; // final hit
					} else {
						//// Push
						stack_node[child_scale - 1] = parent_node;
						stack_t1  [child_scale - 1] = t1;

						// child becomes parent
						parent_node = node;

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

		vec2 rand_input = gl_FragCoord.xy;
		rand_input += time * vec2(3485.0, 144.0);
		vec2 px_jitter = vec2(rand(rand_input), rand(rand_input + vec2(11234.0, 0.0))) - 0.5;

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

	void main () {
		vec3 ray_pos, ray_dir;
		get_ray(ray_pos, ray_dir);

		int queued_rays = 0;
		QueuedRay queue[MAX_SEC_RAYS];

		float hit_dist;
		vec3 accum_col = process_ray(ray_pos, ray_dir, hit_dist, queue, queued_rays, vec4(1.0));
		
		for (int cur_ray = 0; cur_ray < queued_rays; ++cur_ray) {
			float _dist;

			vec3 ray_col = process_ray(queue[cur_ray].pos, queue[cur_ray].dir, _dist, queue, queued_rays, queue[cur_ray].tint);
			accum_col += ray_col * queue[cur_ray].tint.rgb * queue[cur_ray].tint.a;
		}
		
		vec4 col = vec4(accum_col, 1.0);
		
		if (visualize_iterations)
			col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));

		FRAG_COL(col);

		{ // Write depth of furthest hit surface of primary ray (=always opaque) or inf distance if not hit
			vec3 hit_pos_world = ray_pos + ray_dir * hit_dist + vec3(svo_root_pos);

			vec4 clip = world_to_clip * vec4(hit_pos_world, 1.0);
			float ndc_depth = clip.z / clip.w - 0.00001f; // bias to fix z fighting with debug overlay
			gl_FragDepth = ((gl_DepthRange.diff * ndc_depth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
		}
	}
$endif
