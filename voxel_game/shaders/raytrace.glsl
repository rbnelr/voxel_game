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

float max_component (vec3 v) {
	return max(max(v.x, v.y), v.z);
}
float min_component (vec3 v) {
	return min(min(v.x, v.y), v.z);
}

int max_component_index (vec3 v) {
	if (v.x >= v.y) {
		if (v.x >= v.z) return 0;
		else            return 2;
	} else {
		if (v.y >= v.z) return 1;
		else            return 2;
	}
}
int min_component_index (vec3 v) {
	if (v.x <= v.y) {
		if (v.x <= v.z) return 0;
		else            return 2;
	} else {
		if (v.y <= v.z) return 1;
		else            return 2;
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

#define MAX_DEPTH 6

#define B_NULL 0
#define B_AIR 1

struct Stackframe {
	ivec3 pos; // 3d index (scaled to octree level cell size so that one increment is always as step to the next cell on that level)
	vec3 min, max, mid;
	vec3 t0, t1, tm;

	int cur_node;
	vec3 lo, hi;
};

const int[] lut_a = int[] ( 1, 0, 0 );
const int[] lut_b = int[] ( 2, 2, 1 );

const ivec3[] child_offset_lut = ivec3[8] (
	ivec3(0,0,0),
	ivec3(1,0,0),
	ivec3(0,1,0),
	ivec3(1,1,0),
	ivec3(0,0,1),
	ivec3(1,0,1),
	ivec3(0,1,1),
	ivec3(1,1,1)
	);

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
	int max_comp = max_component_index(t0);

	int a = lut_a[max_comp];
	int b = lut_b[max_comp];

	float cond = t0[max_comp];

	int ret = 0;
	ret |= (tm[a] < cond) ? (1 << a) : 0;
	ret |= (tm[b] < cond) ? (1 << b) : 0;
	return ret;
}
int next_node (vec3 t1, int cur_node) {
	int min_comp = min_component_index(t1);

	return node_seq_lut[cur_node*3 + min_comp];
}

Stackframe get_child (in Stackframe stk, int index, bvec3 mask) {
	Stackframe ret;

	ret.pos = stk.pos * 2 + child_offset_lut[index];
	ret.min = mix(stk.min, stk.mid, mask);
	ret.max = mix(stk.mid, stk.max, mask);
	ret.mid = 0.5 * (ret.max + ret.min);

	return ret;
}

bool eval_octree_cell (int depth, in Stackframe stk, out bool hit) {
	vec3 size = stk.max - stk.min;
	
	int level = MAX_DEPTH - depth;
	int voxel_count = 1 << depth;
	int voxel_size = CHUNK_DIM >> depth;

	ivec3 voxel_pos = stk.pos;
	voxel_pos.z += chunk_index * CHUNK_DIM;

	int id = int(round(texelFetch(voxels_tex, voxel_pos, level).r * 255));

	DEBUG(id == B_NULL ? 0.5 : 0);

	//hit = id != B_NULL && id != B_AIR;
	return id == B_NULL; // true == need to drill further down into octree
}

#define PRE_LOOP 0
#define LOOP_PRE_RECURSE 1
#define LOOP_CHILD_POST_RECURSE 2
#define RETURN 3

Stackframe[MAX_DEPTH+1] stk;

bool traverse_octree (in Stackframe _stk) {

	int depth = 0;
	stk[0] = _stk;

	stk[0].cur_node = 0;
	stk[1].cur_node = 0;
	stk[2].cur_node = 0;
	stk[3].cur_node = 0;
	stk[4].cur_node = 0;
	stk[5].cur_node = 0;
	stk[6].cur_node = 0;
	stk[0].hi = vec3(0);
	stk[1].hi = vec3(0);
	stk[2].hi = vec3(0);
	stk[3].hi = vec3(0);
	stk[4].hi = vec3(0);
	stk[5].hi = vec3(0);
	stk[6].hi = vec3(0);

	int mirror_mask_int = 0;
	bvec3 mirror_mask = bvec3(false);

	for (int i=0; i<3; ++i) {
		if (ray_dir[i] < 0) {
			ray_pos[i] = stk[depth].mid[i] * 2 - ray_pos[i];
			ray_dir[i] = -ray_dir[i];
			mirror_mask_int |= 1 << i;
			mirror_mask[i] = true;
		}
	}
	vec3 rdir_inv = 1.0 / ray_dir;

	stk[depth].t0 = (stk[depth].min - ray_pos) * rdir_inv;
	stk[depth].t1 = (stk[depth].max - ray_pos) * rdir_inv;

	if (max_component(stk[depth].t0) >= min_component(stk[depth].t1))
		return false;

	bool hit = false;
	bvec3 ray_mask = notEqual(ray_dir, vec3(0));

	int op = PRE_LOOP;
	
	int it = 0;
	int max_it = 50;
	for (; !hit && it<max_it; ++it) {

		switch (op) {

			case PRE_LOOP: {
				//stk[depth].tm = mix(
				//		mix(vec3(-INF), vec3(+INF), lessThan(ray_pos, stk[depth].mid)),
				//		0.5 * (stk[depth].t0 + stk[depth].t1),
				//	ray_mask);
				stk[depth].tm = 0.5 * (stk[depth].t0 + stk[depth].t1);


				if (all(greaterThanEqual(stk[depth].t1, vec3(0))) && eval_octree_cell(depth, stk[depth], hit)) {

					stk[depth].cur_node = first_node(stk[depth].t0, stk[depth].tm);

					op = LOOP_PRE_RECURSE;

				} else {
					op = RETURN;
				}

				//DEBUG(hit ? 0.5 : 0);
			} break;

			case LOOP_PRE_RECURSE: {

				ivec3 _mask = ivec3(stk[depth].cur_node & 1, (stk[depth].cur_node >> 1) & 1, (stk[depth].cur_node >> 2) & 1);
				bvec3 mask = notEqual(_mask, ivec3(0));
				stk[depth].lo = mix(stk[depth].t0, stk[depth].tm, mask);
				stk[depth].hi = mix(stk[depth].tm, stk[depth].t1, mask);

				// Recursive call
				vec3 lo = stk[depth].lo;
				vec3 hi = stk[depth].hi;
				
				stk[++depth] = get_child(stk[depth], stk[depth].cur_node ^ mirror_mask_int, bvec3(ivec3(mask) ^ ivec3(mirror_mask)));
				stk[depth].t0 = lo,
				stk[depth].t1 = hi;

				op = PRE_LOOP; // recursive call
			} break;

			case LOOP_CHILD_POST_RECURSE: {
				if (!hit)
					stk[depth].cur_node = next_node(stk[depth].hi, stk[depth].cur_node);

				if (hit || stk[depth].cur_node >= 8) {
					op = RETURN;
				} else {
					op = LOOP_PRE_RECURSE;
				}
			} break;

			case RETURN: {
				// Return from recursive call
				--depth;
				if (depth >= 0) {
					op = LOOP_CHILD_POST_RECURSE; // return from recusive call
				} else {
					// finish
				}
			} break;
		}
	}

	if (hit) {
		hit_col = vec4(vec3(float(it) / float(max_it)), 1);
	}
	return hit;
}

bool octree_raycast (vec3 ray_pos_, vec3 ray_dir_) {
	Stackframe stk;

	ray_pos = ray_pos_;
	ray_dir = ray_dir_;

	ivec2 chunk_pos = ivec2(floor(ray_pos.xy / CHUNK_DIM));
	stk.min = vec3(chunk_pos * CHUNK_DIM, 0);
	stk.max = stk.min + CHUNK_DIM;
	stk.mid = 0.5 * (stk.max + stk.min);

	chunk_index = 0;

	return traverse_octree(stk);
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
