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
	uniform sampler3D chunk_tex;

	const float inf = 1.0 / 0.0;

	float scalar_normalize (float x) {
		return x / abs(x);
	}

	int find_next_axis (vec3 next) {
		if (		next.x < next.y && next.x < next.z )	return 0;
		else if (	next.y < next.z )						return 1;
		else												return 2;
	}

	bool hit_block (vec3 voxel_pos, out vec4 hit_col) {
		return false;
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

		DEBUG(cur_dist);

		float count = 0;

		//if (hit_block(cur_voxel, hit_col)) // ray started inside block, -1 as no face was hit
		//	return true;

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
			
			//if (hit_block(cur_voxel, hit_col))
			//	return true;

			count += 1;
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
		if (raycast(ray_pos_world, ray_dir_world, 128.0, col))
			FRAG_COL(col);
		else
			FRAG_COL(vec4(ray_dir_world, 1.0));
	}
$endif
