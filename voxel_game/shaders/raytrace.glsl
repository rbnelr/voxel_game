#version 330 core

$include "common.glsl"

$if vertex
	uniform const vec4[] pos_clip = vec4[] (
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

	uniform vec2 min_chunk;
	uniform vec2 chunks_lut_size;
	uniform float voxels_chunks_count;

	const float inf = 1.0 / 0.0;

	float scalar_normalize (float x) {
		return x / abs(x);
	}

	int find_next_axis (vec3 next) {
		if (		next.x < next.y && next.x < next.z )	return 0;
		else if (	next.y < next.z )						return 1;
		else												return 2;
	}

	bool hit_block (vec3 voxel_pos, vec3 pos_world, int axis, float axis_dir, out vec4 hit_col) {
		
		if (pos_world.z <= 0 || pos_world.z >= CHUNK_DIM)
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
		
		float tex_indx = id; // TODO: this is wrong, need to look into tile info lut
		
		vec3 pos_fract = pos_world - voxel_pos;
		pos_fract.x *= axis_dir;
		pos_fract.y *= -axis_dir;
		
		vec2 uv;
		uv.x = pos_fract[axis != 0 ? 0 : 1];
		uv.y = pos_fract[axis == 2 ? 1 : 2];
		
		hit_col = texture(tile_textures, vec3(uv, tex_indx));
		return true;
	}

	bool raycast (vec3 pos, vec3 dir, float max_dist, out vec4 hit_col) {
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
		next = mix(next, vec3(inf), equal(dir, vec3(0.0)));

		// find the axis of the next voxel step
		int cur_axis = find_next_axis(next);
		float cur_dist = next[cur_axis];

		if (hit_block(cur_voxel, pos + dir * cur_dist, cur_axis, step_delta[cur_axis], hit_col)) // ray started inside block, -1 as no face was hit
			return true;

		while (cur_dist <= max_dist) {
			{
				// find the axis of the cur step
				cur_axis = find_next_axis(next);
				cur_dist = next[cur_axis];

				// clac the distance at which the next voxel step for this axis happens
				next[cur_axis] += step_dist[cur_axis];
				// step into the next voxel
				cur_voxel[cur_axis] += step_delta[cur_axis];
			}

			if (hit_block(cur_voxel, pos + dir * cur_dist, cur_axis, step_delta[cur_axis], hit_col))
				return true;
		}

		return false; // stop stepping because max_dist is reached
	}

	void main () {
		vec2 ndc = gl_FragCoord.xy / viewport_size * 2.0 - 1.0;
		if (ndc.x < -0.3)
			DISCARD();

		vec4 near_plane_clip = cam_to_clip * vec4(0.0, 0.0, -clip_near, 1.0);

		vec4 clip = vec4(ndc, -1.0, 1.0) * near_plane_clip.w; // ndc = clip / clip.w;
		
		vec3 pos_cam = (clip_to_cam * clip).xyz;
		vec3 dir_cam = normalize(pos_cam);

		vec3 ray_pos_world = ( cam_to_world * vec4(pos_cam, 1) ).xyz;
		vec3 ray_dir_world = ( cam_to_world * vec4(dir_cam, 0) ).xyz;

		vec4 col;
		if (raycast(ray_pos_world, ray_dir_world, 200, col))
			FRAG_COL(col);
		else
			DISCARD();
	}
$endif
