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
	uniform vec2 min_chunk;
	uniform vec2 chunks_lut_size;
	uniform float voxels_chunks_count;
	uniform float view_dist;
	uniform float iterations_visualize_max;
	uniform bool iterations_visualize;

	const float inf = 3.4028235e38 + 1.0; // infinity, my laptop shader compiler did not like 1.0 / 0.0

	vec4 hit_col = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 hit_pos_cam = vec4(0.0, 0.0, -inf, 1.0);
	float hit_dist = inf;

	float iterations = 0;

	float scalar_normalize (float x) {
		return x / abs(x);
	}

	bool hit_block (vec3 voxel_pos, vec3 ray_pos, vec3 ray_dir, vec3 next, vec3 step_delta, vec3 step_dist, bvec3 mask) {
		
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

	bool raycast (vec3 pos, vec3 dir, float max_dist) {
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

		bvec3 mask = lessThanEqual(next.xyz, min(next.yzx, next.zxy));

		if (hit_block(cur_voxel, pos, dir, next, step_delta, step_dist, mask))
			return true;

		while (any(lessThanEqual(next, vec3(max_dist)))) {

			iterations += 1.0;

			mask = lessThanEqual(next.xyz, min(next.yzx, next.zxy));

			next      += vec3(mask) * step_dist;
			cur_voxel += vec3(mask) * step_delta;

			if (hit_block(cur_voxel, pos, dir, next, step_delta, step_dist, mask))
				return true;
		}

		return false; // stop stepping because max_dist is reached
	}

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

		raycast(ray_pos_world, ray_dir_world, view_dist);

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