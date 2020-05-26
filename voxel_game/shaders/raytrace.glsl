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
	uniform sampler2D chunks_lut;
	uniform sampler3D voxels_tex;
	#define CHUNK_DIM 64

	uniform	sampler2DArray tile_textures;

	uniform float slider;
	uniform float octree_slider;
	uniform vec2 min_chunk;
	uniform vec2 chunks_lut_size;
	uniform float voxels_chunks_count;
	uniform float view_dist;
	uniform float iterations_visualize_max;
	uniform bool iterations_visualize;

	const float INF = 3.4028235e38 + 1.0; // infinity, my laptop shader compiler did not like 1.0 / 0.0

	vec4 hit_col = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 hit_pos_cam = vec4(0.0, 0.0, -INF, 1.0);
	float hit_dist = INF;

	float iterations = 0;

	float scalar_normalize (float x) {
		return x / abs(x);
	}

	bvec3 threeway_min_mask (vec3 v) {
		return lessThanEqual(v.xyz, min(v.yzx, v.zxy));
	}
	bvec3 threeway_max_mask (vec3 v) {
		return greaterThanEqual(v.xyz, max(v.yzx, v.zxy));
	}
	int max_component_index (vec3 v) {
		if (v.x >= v.y) {
			if (v.x >= v.z) return 0;
			else           return 2;
		} else {
			if (v.y >= v.z) return 1;
			else           return 2;
		}
	}
	int min_component_index (vec3 v) {
		if (v.x <= v.y) {
			if (v.x <= v.z) return 0;
			else           return 2;
		} else {
			if (v.y <= v.z) return 1;
			else           return 2;
		}
	}

	//////
	bool uniform_hit_block (vec3 voxel_pos, vec3 ray_pos, vec3 ray_dir, vec3 next, vec3 step_delta, vec3 step_dist, bvec3 mask) {
		
		if (voxel_pos.z <= 0 || voxel_pos.z >= CHUNK_DIM)
			return false;
		
		vec2 chunk_pos = floor(voxel_pos.xy / CHUNK_DIM);
		vec2 chunk_index2d = chunk_pos - min_chunk;
		
		if (chunk_index2d.x < 0 || chunk_index2d.y < 0 ||
			chunk_index2d.x >= chunks_lut_size.x || chunk_index2d.y >= chunks_lut_size.y)
			return false;
		
		float chunk_index = texture(chunks_lut, (chunk_index2d + 0.5) / chunks_lut_size).r;
		if (chunk_index < 0)
			return false;

		voxel_pos.xy -= chunk_pos * CHUNK_DIM;

		vec3 vox_uv = (voxel_pos + 0.5);
		vox_uv.z += chunk_index * CHUNK_DIM;
		vox_uv /= vec3(CHUNK_DIM, CHUNK_DIM, CHUNK_DIM * voxels_chunks_count);
		
		float id = round(texture(voxels_tex, vox_uv).r * 255);
		if (id == 1.0) { // B_AIR == 1
			return false;
		}

		//DEBUG(voxel_pos / 64);
		
		int axis;
		if (mask.x)
			axis = 0;
		else if (mask.y)
			axis = 1;
		else
			axis = 2;

		float dist = next[axis] - step_dist[axis];
		float axis_dir = step_delta[axis];

		vec3 pos_world = ray_dir * dist + ray_pos;

		float tex_indx = id; // TODO: this is wrong, need to look into tile info lut
		
		vec3 pos_fract = pos_world - voxel_pos;
		pos_fract.x *= axis_dir;
		pos_fract.y *= -axis_dir;
		
		vec2 uv;
		uv.x = pos_fract[axis != 0 ? 0 : 1];
		uv.y = pos_fract[axis == 2 ? 1 : 2];
		
		hit_col = texture(tile_textures, vec3(uv, tex_indx));
		hit_pos_cam = world_to_cam * vec4(pos_world, 1.0);
		hit_dist = dist;
		return true;
	}

	bool uniform_raycast (vec3 pos, vec3 dir, float max_dist) {
		// get direction of each axis of ray_dir (-1, 0, +1)
		vec3 step_delta = vec3(	scalar_normalize(dir.x),
								scalar_normalize(dir.y),
								scalar_normalize(dir.z) );

		// get how far you have to travel along the ray to move by 1 unit in each axis
		// (ray_dir / abs(ray_dir.x) normalizes the ray_dir so that its x is 1 or -1
		// a zero in ray_dir produces a NaN in step because 0 / 0
		vec3 step_dist = vec3(	length(dir / abs(dir.x)),
								length(dir / abs(dir.y)),
								length(dir / abs(dir.z)) );

		// get initial positon in block and intial voxel coord
		vec3 pos_floor = floor(pos);
		vec3 pos_in_block = pos - pos_floor;

		vec3 cur_voxel = pos_floor;

		// how far to step along ray to step into the next voxel for each axis
		vec3 next = step_dist * mix(pos_in_block, 1 - pos_in_block, step(vec3(0.0), dir));

		// NaN -> Inf
		next = mix(next, vec3(INF), equal(dir, vec3(0.0)));

		bvec3 mask = lessThanEqual(next.xyz, min(next.yzx, next.zxy));

		if (uniform_hit_block(cur_voxel, pos, dir, next, step_delta, step_dist, mask))
			return true;

		while (any(lessThanEqual(next, vec3(max_dist)))) {

			iterations += 1.0;

			mask = lessThanEqual(next.xyz, min(next.yzx, next.zxy));

			next      += vec3(mask) * step_dist;
			cur_voxel += vec3(mask) * step_delta;

			if (uniform_hit_block(cur_voxel, pos, dir, next, step_delta, step_dist, mask))
				return true;
		}

		return false; // stop stepping because max_dist is reached
	}

	/////
	int chunk_index;

	// An Efficient Parametric Algorithm for Octree Traversal
	// J. Revelles, C.Ure ̃na, M.Lastra
	// http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=F6810427A1DC4136D615FBD178C1669C?doi=10.1.1.29.987&rep=rep1&type=pdf
	vec3 ray_pos;
	vec3 ray_dir;

	bool eval_octree_cell (int level, ivec3 voxel_pos, vec3 min, vec3 mid, vec3 max, vec3 t0, vec3 t1, out bool stop_traversal) {
		int voxel_count = CHUNK_DIM >> level;
		int voxel_size = 1 << level;

		voxel_pos.z += chunk_index * CHUNK_DIM;

		float id = round(texelFetch(voxels_tex, voxel_pos, level).r * 255);
			
		stop_traversal = id != 1;
		return id == 0; // true == need to drill further down into octree
	}

	int mirror_mask_int;
	bvec3 mirror_mask;
	
	void traverse_octree (int level, ivec3 voxel_pos, vec3 _min, vec3 _mid, vec3 _max, vec3 ray_pos, vec3 ray_dir) {
		mirror_mask_int = 0;
		mirror_mask = bvec3(false, false, false);

		for (int i=0; i<3; ++i) {
			if (ray_dir[i] < 0) {
				ray_pos[i] = _mid[i] * 2 - ray_pos[i];
				ray_dir[i] = -ray_dir[i];
				mirror_mask_int |= 1 << i;
				mirror_mask[i] = true;
			}
		}

		vec3 rdir_inv = 1.0f / ray_dir;

		vec3 t0 = (_min - ray_pos) * rdir_inv;
		vec3 t1 = (_max - ray_pos) * rdir_inv;

		if (max(max(t0.x, t0.y), t0.z) < min(min(t1.x, t1.y), t1.z))
			traverse_subtree(level, voxel_pos, _min, _mid, _max, t0, t1);
	}

	const int[] lut_a = new int[] ( 1, 0, 0 );
	const int[] lut_b = new int[] ( 2, 2, 1 );

	const ivec3[] child_offset_lut = new ivec3[8] (
		int3(0,0,0),
		int3(1,0,0),
		int3(0,1,0),
		int3(1,1,0),
		int3(0,0,1),
		int3(1,0,1),
		int3(0,1,1),
		int3(1,1,1)
	);
	const int[] node_seq_lut = new int[8*3] (
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
		int max_comp = max_component_index(t0);

		int a = lut_a[max_comp];
		int b = lut_b[max_comp];

		float cond = t0[max_comp];

		int ret = 0;
		ret |= (tm[a] < cond) ? (1 << a) : 0;
		ret |= (tm[b] < cond) ? (1 << b) : 0;
		return ret;
	}
	int next_node (float3 t1, int child_indx) {
		int min_comp = min_component_index(t1);

		return indices[child_indx * 3 + min_comp];
	}

	bool traverse_subtree (int level, ivec3 voxel_pos, vec3 min, vec3 mid, vec3 max, vec3 t0, vec3 t1) {

		if (any(less(t1, vec3(0.0))))
			return false;

		bool stop;
		bool decend = eval_octree_cell(level, voxel_pos, _min, _mid, _max, t0, t1, stop);
		if (decend) {
			vec3 infs = mix(float3(-INF), float3(+INF), less(ray_pos, _mid));
			vec3 tm = mix(0.5f * (t0 + t1), infs, equal(ray_dir, vec3(0)));

			int cur_node = first_node(t0, tm);

			do {
				bvec3 mask = bvec3(cur_node & 1, (cur_node >> 1) & 1, (cur_node >> 2) & 1);
				bvec3 c_mask = mask ^ mirror_mask;

				ivec3 c_pos = voxel_pos * 2 + child_offset_lut[cur_node ^ mirror_mask_int];
				vec3 c_min = mix(_min, _mid, c_mask);
				vec3 c_max = mix(_mid, _max, c_mask);
				vec3 c_mid = 0.5f * (c_max + c_min);

				vec3 tmt1 = mix(tm, t1, mask);

				stop = traverse_subtree(level - 1, c_pos, c_min, c_mid, c_max, mix(t0, tm, mask), tmt1);
				if (stop)
					return true;

				cur_node = next_node(tmt1, node_seq_lut[cur_node]);
			} while (cur_node < 8);
		}

		return stop;
	}

	bool octree_raycast (vec3 ray_pos, vec3 ray_dir) {
		ivec2 chunk_pos = ivec2(floor(ray_pos.xy / CHUNK_DIM));
		vec3 min = vec3(chunk_pos * CHUNK_DIM, 0);
		vec3 max = min + CHUNK_DIM;
		vec3 min = 0.5f * (max + min);

		chunk_index = 0;

		traverse_octree(7, ivec3(chunk_pos, 0), min, mid, max, ray_pos, ray_dir);
	}

	/////
	void main () {
		vec2 ndc = gl_FragCoord.xy / viewport_size * 2.0 - 1.0;
		if (ndc.x > (slider * 2 - 1))
			DISCARD();

		vec4 near_plane_clip = cam_to_clip * vec4(0.0, 0.0, -clip_near, 1.0);

		vec4 clip = vec4(ndc, -1.0, 1.0) * near_plane_clip.w; // ndc = clip / clip.w;
		
		vec3 pos_cam = (clip_to_cam * clip).xyz;
		vec3 dir_cam = normalize(pos_cam);

		vec3 ray_pos_world = ( cam_to_world * vec4(pos_cam, 1) ).xyz;
		vec3 ray_dir_world = ( cam_to_world * vec4(dir_cam, 0) ).xyz;

		if (ndc.x > (octree_slider * 2 - 1))
			uniform_raycast(ray_pos_world, ray_dir_world, view_dist);
		else
			octree_raycast(ray_pos_world, ray_dir_world);

		if (iterations_visualize)
			DEBUG(vec3(iterations / iterations_visualize_max, 0, 0));

		{ // Write depth
			vec4 clip = cam_to_clip * hit_pos_cam;
			float ndc_depth = clip.z / clip.w - 0.00001f; // bias to fix z fighting with debug overlay
			gl_FragDepth = ((gl_DepthRange.diff * ndc_depth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
		}

		hit_col.rgb = apply_fog(hit_col.rgb, hit_dist, ray_dir_world);

		FRAG_COL(hit_col);
	}
$endif
