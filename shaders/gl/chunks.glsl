#version 460 // for GL_ARB_shader_draw_parameters

#include "common.glsl"
#include "gpu_voxels.glsl"
#include "rand.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
	
	vec3	pos_world;
	vec3	pos_cam;
	mat3	cam2world;
	//vec3	normal_world;
	
	float	damage_tile; // -1 for no damage render
	vec3	dbg_col;
	
	flat uint bid;
} vs;

uniform float detail_draw_dist = 256;
uniform sampler2D water_displ_tex;
uniform float water_scrolling_t = 0;

#ifdef _VERTEX
	layout(location = 0) in vec3	voxel_pos; // pos of voxel instance in chunk
	layout(location = 1) in uint	meshid;
	layout(location = 2) in float	texid;
	
	uniform vec3 chunk_pos;
	
	uniform float damage;
	uniform ivec3 damaged_block;

	uniform float damage_tiles_first;
	uniform float damage_tiles_count;

	float calc_damage_tile (vec3 voxel_pos_world) {
		if (damage == 0.0f) return -1;

		// TODO: because of block mesh jittering our input voxel positons are actually not integers
		//  so to get the correct coord here I either have to:
		//  1: pass the int coord + the jittered coord/jitter offset   ->  blows up memory size for no good reason
		//  2: jitter in the vertex shader  ->  would prefer this anyway,
		//       but I think that random memory accesses like we alread do into block_meshes are currently the bottleneck for render performance
		//       so should avoid this
		//  3: simply assume jitter will never be >= 0.5 and just round the coord to ints
		ivec3 voxel_coord = ivec3(round(voxel_pos_world));
		if (damaged_block != voxel_coord) return -1;

		return clamp(floor(damage * damage_tiles_count), 0.0, damage_tiles_count) + damage_tiles_first;
	}
	
	vec3 water_displ (vec2 pos_world) {
		const float scale1 = 1.0 / 20;
		const float scale2 = 1.0 / 45;
		const vec2 dir1 = vec2(3,3);
		const vec2 dir2 = vec2(-2,3);
		float val1 = texture(water_displ_tex, pos_world * scale1 + dir1 * water_scrolling_t).r;
		float val2 = texture(water_displ_tex, pos_world * scale2 + dir2 * water_scrolling_t).r;
		//float x = val1 * 0.1;
		//float y = val2 * 0.1;
		float z = mix(val1, val2, .4) * -0.5;
		
		return vec3(0, 0, z);
	}
	
	// returns [-1, 1)
	vec3 rand_offset (ivec3 vertex_pos) {
		srand(vertex_pos.x, vertex_pos.y, vertex_pos.z);
		return rand3()*2.0 - 1.0;
	}
	
	// encode into block 'class'
	int encode (uint bid) {
		// don't nudge ever
		if (bid == B_NULL      ) return 0;
		if (bid == B_AIR       ) return 0;
		if (bid == B_WATER     ) return 4;
		if (bid == B_TORCH     ) return 0;
		if (bid == B_TALLGRASS ) return 0;
		if (bid == B_GLOWSHROOM) return 0;
		
		if (bid == B_TREE_LOG ) return 2;
		if (bid == B_LEAVES   ) return 3;
		
		return 1; // normal connection to each other
	}
	bool nudge (uint bid, int other_cls) {
		uint cls = encode(bid);
		if (cls == other_cls) return false;
		if (cls == 2 && other_cls == 1 ||
		    cls == 1 && other_cls == 2) return false;
		return true;
	}
	void vertex_displacement (vec3 vox_pos, inout vec3 vert_pos, inout vec3 dbg_col) {
		float dist = distance(vert_pos, view.lod_center);
		float fade_dist = 32;
		if (dist > detail_draw_dist + fade_dist) return;
		float fade = clamp(map(dist, detail_draw_dist + fade_dist, detail_draw_dist), 0.0, 1.0);
		
		//ivec3 vox_coord = ivec3(floor(vox_pos));
		ivec3 vox_coord = ivec3(floor(vox_pos + vec3(0.5f, 0.5f, 0.5f))); // Why does this work?
		ivec3 vert_coord = ivec3(round(vert_pos));
		
		vs.bid = read_voxel(vox_coord);
		int cls = encode(vs.bid);
		//if (cls == 0) dbg_col = vec3(1,0,0);
		if (cls <= 0) return;
		
		ivec3 s = mix(ivec3(-1), ivec3(+1), greaterThan(vert_coord, vox_coord));
		
		bool v100 = nudge(read_voxel(vox_coord + ivec3(s.x,  0,  0)), cls);
		bool v010 = nudge(read_voxel(vox_coord + ivec3(  0,s.y,  0)), cls);
		bool v110 = nudge(read_voxel(vox_coord + ivec3(s.x,s.y,  0)), cls);
		bool v001 = nudge(read_voxel(vox_coord + ivec3(  0,  0,s.z)), cls);
		bool v101 = nudge(read_voxel(vox_coord + ivec3(s.x,  0,s.z)), cls);
		bool v011 = nudge(read_voxel(vox_coord + ivec3(  0,s.y,s.z)), cls);
		bool v111 = nudge(read_voxel(vox_coord + ivec3(s.x,s.y,s.z)), cls);
		
		int count = int(v100) + int(v010) + int(v110) +
		            int(v001) + int(v101) + int(v011) + int(v111);
		
		vec3 d = vec3(0);
		if (count == 7) {
			d = vec3(1,1,1);
		}
		else if (count == 6) {
			if      (!v100) d = vec3(0,1,1);
			else if (!v010) d = vec3(1,0,1);
			else if (!v001) d = vec3(1,1,0);
		}
		//dbg_col = vec3(d);
		
		vec3 offs = vec3(0);
		if (vs.bid == B_WATER) {
			offs += water_displ(vert_pos.xy);
		}
		else {
			offs += d * s*vec3(-0.2f) + rand_offset(vert_coord)*0.05f;
		}
		
		vert_pos += offs * fade;
	}

	//
	void main () {
		BlockMeshVertex v = block_meshes.vertices[meshid][gl_VertexID];
		vec3 mesh_pos_model		= v.pos.xyz;
		//vec3 mesh_norm_model	= v.normal.xyz;
		vec2 uv					= v.uv.xy;
		
		vec3 vox_pos_world = voxel_pos * FIXEDPOINT_FAC + chunk_pos;
		vec3 vert_pos_world = mesh_pos_model + vox_pos_world;
		
		vs.dbg_col = vec3(0);
		vertex_displacement(vox_pos_world, vert_pos_world, vs.dbg_col);

		gl_Position =		view.world_to_clip * vec4(vert_pos_world, 1);
		vs.uvi =			vec3(uv, texid);
		
		vs.pos_world =		vert_pos_world;
		vs.pos_cam =		(view.world_to_cam * vec4(vert_pos_world, 1)).xyz;
		vs.cam2world =		mat3(view.cam_to_world);
		//vs.normal_world =	mesh_norm_model;
		
		vs.damage_tile =	calc_damage_tile(vox_pos_world);
	}
#endif

#ifdef _FRAGMENT
	#define ALPHA_TEST_THRES 127.0
	
	vec3 normal_from_deriv () {
		vec3 norm_cam = normalize( cross(dFdx(vs.pos_cam), dFdy(vs.pos_cam)) );
		return vs.cam2world * norm_cam;
	}
	
	layout(location = 0) out vec4 frag_col;
	//layout(location = 1) out vec4 frag_normal;
	void main () {
		vec4 col = texture(tile_textures, vs.uvi);

	#if ALPHA_TEST && !defined(_WIREFRAME)
		if (col.a <= ALPHA_TEST_THRES / 255.0)
			discard;
		col.a = 1.0;
	#endif
		
		if (vs.damage_tile >= 0.0) {
			float dmg_tint = texture(tile_textures, vec3(vs.uvi.xy, vs.damage_tile)).r;
			dmg_tint = (pow(dmg_tint, 1.0/2.2) - 0.5) * 2.5;

			col.rgb *= 1.0 + vec3(dmg_tint);
		}
		
		vec3 norm = normal_from_deriv();

		//vec3 norm = normalize(vs.normal_world); // shouldn't be needed since I don't use geometry with curved geometry, but just in case

		const vec3 amb = vec3(0.1,0.1,0.3) * 0.4;
		float sun = max(dot(norm, normalize(vec3(1, 1.8, 4.0))) * 0.5 + 0.5, 0.0);
		col.rgb *= sun*sun*sun*sun * (1.0 - amb) + amb;
		col.rgb += col.rgb * get_emmisive(vs.bid)*2;
		
		col.rgb = calc_fog(col.rgb, vs.pos_cam, vs.pos_world);
		
		col.rgb = col.rgb + vs.dbg_col;
	
	#ifdef _WIREFRAME
		if (_WIREFRAME_PASS) col.rgb = vec3(0);
	#endif
		frag_col = col;
		//frag_normal = vec4(norm.xyz, 1.0); // alpha 1 incase blending happens to never blend normals
	}
#endif
