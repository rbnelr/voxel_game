#version 330 core

$include "common.glsl"
$include "fog.glsl"

$if vertex
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
	
	uniform float slider;
	uniform isampler1D svo_texture;
	uniform vec3 svo_root_pos;

	uniform int max_iterations = 100;
	uniform sampler2D heat_gradient;
	
	int get_svo_node (int index) {
		return texelFetch(svo_texture, index, 0).r;
	}

#define CHUNK_DIM 64
#define MAX_SCALE 6

#define B_AIR 1

	// SVO format:
	/*
		union OctreeNode {
			uint32 children_index; // 0x80000000 set if has_children, ie. this node a non-leaf node -> mask away MSB to get index into svo_texture where the nodes for the 8 children lie contiguously
			uint16 block_id; // block id (valid when leaf node)
		};
	*/

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

	// https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010i3d_paper.pdf

	vec3 rinv_dir;
	int mirror_mask_int = 0;

	vec3 intersect_ray (vec3 planes) {
		return rinv_dir * (planes - ray_pos);
	}
	void intersect_ray (ivec3 cube_pos, int cube_scale, out float t0, out float t1, out bvec3 exit_faces) {
		vec3 cube_min = vec3(cube_pos);
		vec3 cube_max = vec3(cube_pos + (1 << cube_scale));

		vec3 t0v = rinv_dir * (cube_min - ray_pos);
		vec3 t1v = rinv_dir * (cube_max - ray_pos);

		t0 = max_component( t0v );
		t1 = min_component( t1v );

		exit_faces = t1 == t1v;
	}

	void select_child (float t0, ivec3 parent_pos, int parent_scale, out ivec3 child_pos, out int child_scale) {
		child_scale = parent_scale - 1;

		vec3 parent_mid = vec3(parent_pos + (1 << child_scale));

		vec3 tmid = intersect_ray(parent_mid);

		bvec3 comp = t0 >= tmid;
		ivec3 bits = ivec3(comp);

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

	void raytrace (float max_dist, out int iterations) {
		int iter = 0;
		
		ray_pos -= svo_root_pos;

		vec3 root_min = 0;
		vec3 root_max = vec3(CHUNK_DIM);
		//vec3 root_mid = 0.5 * (root_min + root_max);

		ivec3 parent_pos = 0;
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
		float t0 = max_component( intersect_ray(root_min) );
		float t1 = min_component( intersect_ray(root_max) );

		t0 = max(tmin, t0);
		t1 = min(tmax, t1);

		ivec3 child_pos;
		int child_scale;
		select_child(t0, parent_pos, parent_scale, child_pos, child_scale);

		int stack_node[ MAX_SCALE -1 ] = int [ MAX_SCALE -1 ];
		float stack_t1[ MAX_SCALE -1 ] = float [ MAX_SCALE -1 ];

		for (;;) {
			if (++iter >= max_iterations) break;

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

				int node = get_svo_node( (parent_node & 0x7fffffffu) + (idx ^ mirror_mask_int) );

				//// Intersect
				// child cube ray range
				float tv0 = max(child_t0, t0);
				float tv1 = min(child_t1, t1);

				if (tv0 < tv1) {
					bool leaf = (node & 0x80000000) == 0;
					if (leaf) {
						if (node.bid != B_AIR) {
							//hit.did_hit = true;
							//hit.dist = tv0;
							break; // hit
						}
					} else {
						//// Push
						stack_node[child_scale - 1] = parent_node;
						stack_t1  [child_scale - 1] = t1;

						// child becomes parent
						parent_node = node;

						parent_coord = child_coord;
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
					break; // out of root

				parent_node	= stack_node[child_scale - 1];
				t1			= stack_t1  [child_scale - 1];

				int clear_mask = ~((1 << child_scale) - 1);
				child_pos.x &= clear_mask;
				child_pos.y &= clear_mask;
				child_pos.z &= clear_mask;
			}
		}

		iterations = iter;
		//return hit;
	};

	void main () {
		frag_col = vec4(0,0,0,0);

		int iterations;

		get_ray();
		raytrace(100, iterations);
		
		if (visualize_iterations)
			frag_col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
	}
$endif
