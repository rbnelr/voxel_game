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

#define CHUNK_DIM 64 // size of voxel chunk (cubed)
#define MAX_SCALE 6 // "scale" of root DVO node

#define B_AIR 1 // block id

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
	
#define DEBUG 1
#if DEBUG
	uniform float slider; // debugging
	uniform int max_iterations = 100; // iteration limiter for debugging
	uniform bool visualize_iterations = true;
	uniform sampler2D heat_gradient;
#endif

	float min_component (vec3 v) {
		return min(min(v.x, v.y), v.z);
	}
	float max_component (vec3 v) {
		return max(max(v.x, v.y), v.z);
	}

	//bvec3 xor (bvec3 l, bvec3 r) { // why is this not a language feature if ivec3 ^ ivec3 is a thing?
	//	//return bvec3(l.x ^ r.x, l.y ^ r.y, l.z ^ r.z);
	//	return bvec3(ivec3(l) ^ ivec3(r));
	//}

	vec3 ray_pos;
	vec3 ray_dir;

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

	vec3 rinv_dir;
	int mirror_mask_int = 0;

	void intersect_ray (ivec3 cube_pos, int cube_scale, out float t0, out float t1, out bvec3 exit_faces) {
		vec3 cube_min = vec3(cube_pos);
		vec3 cube_max = vec3(cube_pos + (1 << cube_scale));

		vec3 t0v = rinv_dir * (cube_min - ray_pos);
		vec3 t1v = rinv_dir * (cube_max - ray_pos);

		t0 = max_component( t0v );
		t1 = min_component( t1v );

		exit_faces = equal(vec3(t1), t1v);
	}

	void select_child (float t0, ivec3 parent_pos, int parent_scale, out ivec3 child_pos, out int child_scale) {
		child_scale = parent_scale - 1;

		vec3 parent_mid = vec3(parent_pos + (1 << child_scale));

		vec3 tmid = rinv_dir * (parent_mid - ray_pos);

		ivec3 bits = ivec3( greaterThanEqual(vec3(t0), tmid) );

		child_pos.x = parent_pos.x | (bits.x << child_scale);
		child_pos.y = parent_pos.y | (bits.y << child_scale);
		child_pos.z = parent_pos.z | (bits.z << child_scale);
	}

	int highest_differing_bit (int a, int b) {
		int diff = a ^ b; // bita != bitb => diffbit

		return findMSB(diff);
	}
	int highest_differing_bit (ivec3 a, ivec3 b) {
		return max(max(highest_differing_bit(a.x, b.x), highest_differing_bit(a.y, b.y)), highest_differing_bit(a.z, b.z));
	}

	float raytrace (float max_dist, out int iterations) {
		int iter = 0;
		float hit_dist = -1;

		vec3 root_min = vec3(0);
		vec3 root_max = vec3(CHUNK_DIM);
		vec3 root_mid = 0.5 * (root_min + root_max);

		ivec3 parent_pos = ivec3(0);
		int parent_scale = MAX_SCALE;

		mirror_mask_int = 0;
		
		// mirror coord system to make all ray dir components positive, like in "An Efficient Parametric Algorithm for Octree Traversal"
		for (int i=0; i<3; ++i) {
			bool mirror = ray_dir[i] < 0;
		
			ray_dir[i] = abs(ray_dir[i]);
			if (mirror) ray_pos[i] = root_max[i] - ray_pos[i]; // mirror along mid  ie.  -1 * (ray_pos - mid) + mid -> 2*mid - ray_pos
			if (mirror) mirror_mask_int |= 1 << i;
		}
		
		rinv_dir = 1.0 / ray_dir;

		int parent_node = get_svo_node(0);

		// desired ray bounds
		float tmin = 0;
		float tmax = max_dist;

		// cube ray range
		float t0 = max_component( rinv_dir * (root_min - ray_pos) );
		float t1 = min_component( rinv_dir * (root_max - ray_pos) );

		t0 = max(tmin, t0);
		t1 = min(tmax, t1);

		ivec3 child_pos;
		int child_scale;
		select_child(t0, parent_pos, parent_scale, child_pos, child_scale);

		int stack_node[ MAX_SCALE -1 ];
		float stack_t1[ MAX_SCALE -1 ];

		for (;;) {
		#if DEBUG
			if (iter++ >= max_iterations) break;
		#endif

			// child cube ray range
			float child_t0, child_t1;
			bvec3 exit_face;
			intersect_ray(child_pos, child_scale, child_t0, child_t1, exit_face);

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
						if (node != B_AIR) {
							//hit.did_hit = true;
							hit_dist = tv0;
							break; // hit
						}
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

				if (child_scale >= MAX_SCALE) {
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

		iterations = iter;
		return hit_dist;
	}

	void main () {
		frag_col = vec4(0,0,0,0);

	#if DEBUG
		int iterations;
	#endif
		float max_dist = 100;

		get_ray();
		float hit_dist = raytrace(max_dist, iterations);
		
	#if DEBUG
		if (visualize_iterations)
			frag_col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
		else
			frag_col = vec4(vec3(hit_dist / max_dist), 1.0);
	#endif
	}
$endif
