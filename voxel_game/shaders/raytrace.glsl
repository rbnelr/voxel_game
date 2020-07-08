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
	uniform isampler1D svo_texture;
	uniform vec3 svo_root_pos;

#define INF (1.0 / 0.0)

#define CHUNK_DIM 64 // size of voxel chunk (cubed)
#define MAX_SCALE 6 // "scale" of root DVO node

#define B_AIR 1 // block id
#define B_WATER 2 // block id

	int get_svo_node (int index) {
		return texelFetch(svo_texture, index, 0).r;
	}
	
	// SVO format:
	/*
		union OctreeNode {
			uint32 children_index; // 0x80000000 set if has_children, ie. this node a non-leaf node -> mask away MSB to get index into svo_texture where the nodes for the 8 children lie contiguously
			uint16 block_id; // block id (valid when leaf node)
		};

		 1 bit         31 bit
		[has_children][payload]

		has_children == 0: payload is index of children OctreeNode[8] in svo_texture
		has_children == 1: payload leaf voxel data, ie. block id
	*/
	
	uniform float slider; // debugging
	uniform int max_iterations = 100; // iteration limiter for debugging
	uniform bool visualize_iterations = true;
	uniform sampler2D heat_gradient;

	uniform float max_dist = 100;

	vec3 ray_pos;
	vec3 ray_dir;

	vec3 mirror_ray_pos;

	vec3 rinv_dir;
	int mirror_mask_int = 0;

	// Textures
	uniform	sampler2DArray tile_textures;
	uniform sampler1D block_tile_info; // BlockTileInfo: float4 { base_index, top, bottom, variants }

	float hit_dist = INF;
	vec4 hit_col = vec4(0,0,0,0);

	int cur_medium = 0;

	int iterations = 0;

	void calc_hit (float t0, bvec3 entry_faces, int block_id) {
		if (cur_medium == 0) {
			cur_medium = block_id;
			return;
		}

		int effective_block_id = block_id;

		if ((block_id == cur_medium && block_id == B_WATER))
			return; // only render entry of water, not all octree cubes
		
		if (cur_medium == B_WATER && block_id == B_AIR) {
			effective_block_id = B_WATER; // render water exit into air as water
		} else if (block_id == B_AIR)
			return; // never try to render air
		
		cur_medium = block_id;

		hit_dist = t0;

		vec3 hit_pos_chunk = ray_dir * hit_dist + ray_pos;

		vec3 pos_fract = hit_pos_chunk - floor(hit_pos_chunk);

		vec2 uv;

		if (entry_faces.x)
			uv = pos_fract.yz;
		else if (entry_faces.y)
			uv = pos_fract.xz;
		else
			uv = pos_fract.xy;

		uv.x *= (entry_faces.x && ray_dir.x > 0) || (entry_faces.y && ray_dir.y <= 0) ? -1 : 1;
		uv.y *=  entry_faces.z && ray_dir.z > 0 ? -1 : 1;

		vec4 bti = texelFetch(block_tile_info, effective_block_id, 0);

		float tex_indx = bti.x; // x=base_index
		if (entry_faces.z) {
			tex_indx += ray_dir.z <= 0 ? bti.y : bti.z; // y=top : z=bottom
		}

		vec4 col = texture(tile_textures, vec3(uv, tex_indx));

		float remain_alpha = 1.0 - hit_col.a;
		float effective_alpha = remain_alpha * col.a;
		hit_col += vec4(effective_alpha * col.rgb, effective_alpha);

	}

	// get pixel ray in world space based on pixel coord and matricies
	void get_ray () {
		vec2 ndc = gl_FragCoord.xy / viewport_size * 2.0 - 1.0;

		if (ndc.x > (slider * 2 - 1))
			discard;

		vec4 clip = vec4(ndc, -1, 1) * clip_near; // ndc = clip / clip.w;

		vec3 pos_cam = (clip_to_cam * clip).xyz;
		vec3 dir_cam = pos_cam;

		ray_pos = ( cam_to_world * vec4(pos_cam, 1)).xyz;
		ray_dir = ( cam_to_world * vec4(dir_cam, 0)).xyz;
		ray_dir = normalize(ray_dir);

		ray_pos -= svo_root_pos;
	}

	float min_component (vec3 v) {
		return min(min(v.x, v.y), v.z);
	}
	float max_component (vec3 v) {
		return max(max(v.x, v.y), v.z);
	}

	void intersect_ray (ivec3 cube_pos, int cube_scale, out float t0, out float t1, out bvec3 exit_faces, out bvec3 entry_faces) {
		vec3 cube_min = vec3(cube_pos);
		vec3 cube_max = vec3(cube_pos + (1 << cube_scale));

		vec3 t0v = rinv_dir * (cube_min - mirror_ray_pos);
		vec3 t1v = rinv_dir * (cube_max - mirror_ray_pos);

		t0 = max_component( t0v );
		t1 = min_component( t1v );

		entry_faces = equal(vec3(t0), t0v);
		exit_faces = equal(vec3(t1), t1v);
	}

	void select_child (float t0, ivec3 parent_pos, int parent_scale, out ivec3 child_pos, out int child_scale) {
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

	void raytrace () {
		float hit_dist = -1.0;

		vec3 root_min = vec3(0);
		vec3 root_max = vec3(CHUNK_DIM);
		vec3 root_mid = 0.5 * (root_min + root_max);

		ivec3 parent_pos = ivec3(0);
		int parent_scale = MAX_SCALE;

		mirror_mask_int = 0;

		mirror_ray_pos = ray_pos;
		vec3 mirror_ray_dir;

		// mirror coord system to make all ray dir components positive, like in "An Efficient Parametric Algorithm for Octree Traversal"
		for (int i=0; i<3; ++i) {
			bool mirror = ray_dir[i] < 0;
		
			mirror_ray_dir[i] = abs(ray_dir[i]);
			if (mirror) mirror_ray_pos[i] = root_max[i] - ray_pos[i]; // mirror along mid  ie.  -1 * (ray_pos - mid) + mid -> 2*mid - ray_pos
			if (mirror) mirror_mask_int |= 1 << i;
		}
		
		rinv_dir = 1.0 / mirror_ray_dir;

		int parent_node = get_svo_node(0);

		// desired ray bounds
		float tmin = 0;
		float tmax = max_dist;

		// cube ray range
		float t0 = max_component( rinv_dir * (root_min - mirror_ray_pos) );
		float t1 = min_component( rinv_dir * (root_max - mirror_ray_pos) );

		t0 = max(tmin, t0);
		t1 = min(tmax, t1);

		ivec3 child_pos;
		int child_scale;
		select_child(t0, parent_pos, parent_scale, child_pos, child_scale);

		int stack_node[ MAX_SCALE -1 ];
		float stack_t1[ MAX_SCALE -1 ];

		for (;;) {
			if (iterations++ >= max_iterations) break;

			// child cube ray range
			float child_t0, child_t1;
			bvec3 exit_face, entry_faces;
			intersect_ray(child_pos, child_scale, child_t0, child_t1, exit_face, entry_faces);

			bool voxel_exists = (parent_node & 0x80000000) != 0;
			if (voxel_exists && t0 < t1) {

				int idx = 0;
				idx |= ((child_pos.x >> child_scale) & 1) << 0;
				idx |= ((child_pos.y >> child_scale) & 1) << 1;
				idx |= ((child_pos.z >> child_scale) & 1) << 2;

				int node = get_svo_node( (parent_node & 0x7fffffff) + (idx ^ mirror_mask_int) );

				//// Intersect
				// child cube ray range
				float tv0 = max(child_t0, t0);
				float tv1 = min(child_t1, t1);

				if (tv0 < tv1) {
					
					bool leaf = (node & 0x80000000) == 0;
					if (leaf) {
						//if (node != B_AIR) {
							calc_hit(tv0, entry_faces, node);
							
							if (hit_col.a > 0.999)
								return; // final hit
						//}
					} else {
						//// Push
						stack_node[child_scale - 1] = parent_node;
						stack_t1  [child_scale - 1] = t1;

						// child becomes parent
						parent_node = node;

						parent_pos = child_pos;
						parent_scale = child_scale;

						select_child(tv0, parent_pos, parent_scale, child_pos, child_scale);

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

				if (child_scale >= MAX_SCALE)
					return; // out of root

				parent_node	= stack_node[child_scale - 1];
				t1			= stack_t1  [child_scale - 1];

				int clear_mask = ~((1 << child_scale) - 1);
				child_pos.x &= clear_mask;
				child_pos.y &= clear_mask;
				child_pos.z &= clear_mask;
			}
		}
	}

	void main () {
		get_ray();
		raytrace();
		
		if (visualize_iterations)
			frag_col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
		else
			frag_col = hit_col;

		{ // Write depth
			vec3 hit_pos_world = ray_dir * hit_dist + ray_pos + svo_root_pos;

			vec4 clip = world_to_clip * vec4(hit_pos_world, 1.0);
			float ndc_depth = clip.z / clip.w - 0.00001f; // bias to fix z fighting with debug overlay
			gl_FragDepth = ((gl_DepthRange.diff * ndc_depth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
		}
	}
$endif
