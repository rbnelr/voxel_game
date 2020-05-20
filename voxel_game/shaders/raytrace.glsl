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

	uniform isampler1D BlockTileInfo_texture;
	uniform	sampler2DArray tile_textures;

	// BlockTileInfo format:
	/*
	struct BlockTileInfo {
		int base_index;

		// side is always at base_index
		int top = 0; // base_index + top to get block top tile
		int bottom = 0; // base_index + bottom to get block bottom tile
	*/

	int get_tile_texture_index (int bid, int face) {
		ivec3 bti = texelFetch(BlockTileInfo_texture, bid, 0).rgb;

		int index = bti.x;
		if (face == 1)
			index += bti.y;
		else if (face == 2)
			index += bti.z;
		return index;
	}
	
	uniform float slider;
	uniform isampler1D svo_texture;
	uniform vec3 svo_root_pos;

	uniform sampler2D heat_gradient;
	
	int get_svo_node (int index) {
		return texelFetch(svo_texture, index, 0).r;
	}

#define CHUNK_DIM 64
#define B_AIR 1

#define INF (1.0 / 0.0)

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

	int min_component_index (vec3 v) {
		if (v.x < v.y && v.x < v.z)
			return 0;
		if (v.y < v.z)
			return 1;
		return 2;
	}
	int max_component_index (vec3 v) {
		if (v.x > v.y && v.x > v.z)
			return 0;
		if (v.y > v.z)
			return 1;
		return 2;
	}

	bool any (bvec3 b) {
		return b.x || b.y || b.z;
	}
	bool all (bvec3 b) {
		return b.x && b.y && b.z;
	}

	vec3 select (bvec3 c, vec3 l, vec3 r) {
		return vec3( c.x ? l.x : r.x,
					 c.y ? l.y : r.y,
					 c.z ? l.z : r.z );
	}
	vec3 select (bool c, vec3 l, vec3 r) {
		return vec3( c ? l.x : r.x,
					 c ? l.y : r.y,
					 c ? l.z : r.z );
	}

	bvec3 xor (bvec3 l, bvec3 r) { // why is this not a language feature if ivec3 ^ ivec3 is a thing?
		//return bvec3(l.x ^ r.x, l.y ^ r.y, l.z ^ r.z);
		return bvec3(ivec3(l) ^ ivec3(r));
	}

	int cur_bid = B_AIR; // block id of the current node the ray is in

	vec3 ray_pos;
	vec3 ray_dir;
	vec3 unmirrored_ray_pos;
	vec3 unmirrored_ray_dir;

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
	}

	// An Efficient Parametric Algorithm for Octree Traversal
	// J. Revelles, C.Ure ̃na, M.Lastra
	// http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=F6810427A1DC4136D615FBD178C1669C?doi=10.1.1.29.987&rep=rep1&type=pdf
	// optimized

	int mirror_mask_int;
	bvec3 mirror_mask;

	bool hit_octree_leaf (int node_data, vec3 t0) {
		int bid = node_data;

		bool did_hit = bid != cur_bid && bid != B_AIR;

		cur_bid = bid;

		if (did_hit) {

			float dist = max_component(t0);
			vec3 pos_world = unmirrored_ray_pos + unmirrored_ray_dir * dist;

			//frag_col = vec4(vec3(dist / 100), 1);
			
			vec3 uvw = pos_world - floor(pos_world);

			vec2 uv;
			int face;
			switch (max_component_index(t0)) {
				case 0: {
					uv = vec2(-uvw.y, uvw.z);
					if (mirror_mask[0])
						uv.x = -uv.x;

					face = 0; // side face
				} break;
				case 1: {
					uv = uvw.xz;
					if (mirror_mask[1])
						uv.x = -uv.x;

					face = 0; // side face
				} break;
				case 2: {
					uv = uvw.xy;
					if (!mirror_mask[2])
						uv.y = -uv.y;

					face = mirror_mask[2] ? 1 : 2;
				} break;
			}

			int tex_indx = get_tile_texture_index(bid, face);
			vec4 col = texture(tile_textures, vec3(uv, tex_indx));

			float remain_alpha = 1.0 - frag_col.a;
			frag_col += vec4(col.rgb * col.a, col.a) * remain_alpha;

			return frag_col.a >= 1.0;
		}

		return false;
	}

	const int[] node_seq_lut = int[8*3] (
		1, 2, 4, // 001 010 100
		8, 3, 5, //   - 011 101
		3, 8, 6, // 011   - 110
		8, 8, 7, //   -   - 111
		5, 6, 8, // 101 110   -
		8, 7, 8, //   - 111   -
		7, 8, 8, // 111   -   -
		8, 8, 8  //   -   -   -
		);

	int first_node (vec3 t0, vec3 tm) {
		float cond = max_component(t0);

		int ret = 0;
		ret |= tm[0] < cond ? 1 : 0;
		ret |= tm[1] < cond ? 2 : 0;
		ret |= tm[2] < cond ? 4 : 0;
		return ret;
	}
	int next_node (vec3 t1, int octant) {
		int exit_face = min_component_index(t1);

		return node_seq_lut[octant * 3 + exit_face];
	}
	
	int iterations = 0;

	uniform bool visualize_iterations = false;
	uniform int max_iterations = 256;

#define RECURSIVE(recursive, recursive2) \
	bool recursive (int node_data, vec3 min, vec3 max, vec3 t0, vec3 t1) {																\
																																		\
		iterations++;																													\
		if (iterations > max_iterations)																								\
			return true;																												\
																																		\
		if (any(lessThan(t1, vec3(0.0))))																								\
			return false;																												\
																																		\
		if ((node_data & 0x80000000) == 0) /* !has_children */																			\
			return hit_octree_leaf(node_data, t0);																						\
																																		\
		/* need to decend further down into octree to find actual voxels */																\
																																		\
		vec3 mid = 0.5 * (min + max);																									\
		vec3 tm = 0.5 * (t0 + t1);																										\
																																		\
		/* account for ray_dir being zero in one or more dimensions	*/																	\
		tm = select(notEqual(ray_dir, vec3(0.0)), tm, select(lessThan(ray_pos, mid), vec3(+INF), vec3(-INF)));							\
																																		\
		int cur_octant = first_node(t0, tm);																							\
																																		\
		do {																															\
			bvec3 oct_mask = bvec3((cur_octant & 1) != 0, (cur_octant & 2) != 0, (cur_octant & 4) != 0);								\
																																		\
			/* undo mirroring to get correct child node data */																			\
			int unmirrored_octant = cur_octant ^ mirror_mask_int;																		\
			bvec3 unmirrored_mask = xor(oct_mask, mirror_mask);																			\
																																		\
			int children_index = (node_data & 0x7fffffff) + unmirrored_octant;															\
																																		\
			int _node_data = get_svo_node(children_index);																				\
			vec3 _min = select(unmirrored_mask, mid, min);																				\
			vec3 _max = select(unmirrored_mask, max, mid);																				\
																																		\
			if (recursive2(_node_data, _min, _max, select(oct_mask, tm, t0), select(oct_mask, t1, tm)))									\
				return true; /* hit in subtree */																						\
																																		\
			cur_octant = next_node(select(oct_mask, t1, tm), cur_octant);																\
		} while (cur_octant < 8);																										\
																																		\
		return false; /* no hit in this node, step into next node */		 															\
	}

	bool non_reachable (int node_data, vec3 min, vec3 max, vec3 t0, vec3 t1) {
		return true;
	}

	// no recursion in shaders, so need to duplicate the function once for each depth
	RECURSIVE(traverse_subtree_1, non_reachable)
	RECURSIVE(traverse_subtree_2, traverse_subtree_1)
	RECURSIVE(traverse_subtree_4, traverse_subtree_2)
	RECURSIVE(traverse_subtree_8, traverse_subtree_4)
	RECURSIVE(traverse_subtree_16, traverse_subtree_8)
	RECURSIVE(traverse_subtree_32, traverse_subtree_16)
	RECURSIVE(traverse_subtree_64, traverse_subtree_32)

	void traverse_svo () {
		unmirrored_ray_pos = ray_pos;
		unmirrored_ray_dir = ray_dir;

		mirror_mask_int = 0;

		int node_data = 0x80000001; //nodes[root]._children; root == 0
		vec3 min = svo_root_pos;
		vec3 max = svo_root_pos + vec3(CHUNK_DIM);
		vec3 mid2 = (min + max); // mid = 0.5 * (min + max); mid2 = mid * 2;

		// mirror ray so that direction is always positive in each direction for purpose of algorithm
		for (int i=0; i<3; ++i) {
			bool mirror = ray_dir[i] < 0;

			ray_dir[i] = abs(ray_dir[i]);
			mirror_mask[i] = mirror;
			if (mirror) ray_pos[i] = mid2[i] - ray_pos[i]; // mirror along mid  ie.  -1 * (ray_pos - mid) + mid -> 2*mid - ray_pos
			if (mirror) mirror_mask_int |= 1 << i;
		}

		vec3 rdir_inv = vec3(1.0) / ray_dir;

		vec3 t0 = (min - ray_pos) * rdir_inv;
		vec3 t1 = (max - ray_pos) * rdir_inv;

		if (max_component(t0) < min_component(t1))
			traverse_subtree_64(node_data, min, max, t0, t1);

	}

	void main () {
		frag_col = vec4(0,0,0,0);

		get_ray();
		traverse_svo();
		
		if (visualize_iterations)
			frag_col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
	}
$endif
