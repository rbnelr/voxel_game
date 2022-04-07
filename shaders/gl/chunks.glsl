#version 460 // for GL_ARB_shader_draw_parameters

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
	//vec3	normal_cam;
	float	damage_tile; // -1 for no damage render
} vs;

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

	//
	void main () {
		BlockMeshVertex v = block_meshes.vertices[meshid][gl_VertexID];
		vec3 mesh_pos_model		= v.pos.xyz;
		//vec3 mesh_norm_model	= v.normal.xyz;
		vec2 uv					= v.uv.xy;
		
		vec3 vox_pos_world = voxel_pos * FIXEDPOINT_FAC + chunk_pos;

		gl_Position =		view.world_to_clip * vec4(mesh_pos_model + vox_pos_world, 1);
		vs.uvi =			vec3(uv, texid);
		//vs.normal_cam =		mat3(view.world_to_cam) * mesh_norm_model;
		vs.damage_tile =	calc_damage_tile(vox_pos_world);
	}
#endif

#ifdef _FRAGMENT
	#define ALPHA_TEST_THRES 127.0
	
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

		//vec3 norm = normalize(vs.normal_cam); // shouldn't be needed since I don't use geometry with curved geometry, but just in case

	#ifdef _WIREFRAME
		col = vec4(1.0);
	#endif
		frag_col = col;
		//frag_normal = vec4(norm.xyz, 1.0); // alpha 1 incase blending happens to never blend normals
	}
#endif
