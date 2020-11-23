#version 450

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
	//float	brightness;
} vs;

#ifdef _VERTEX
#extension GL_ARB_shader_draw_parameters : enable
	
	layout(location = 0) in vec3	v_pos; // pos of voxel instance in chunk
	layout(location = 1) in uint	v_meshid;
	layout(location = 2) in float	v_texid;

#define MAX_UBO_SIZE (64*1024)

	//
	layout(push_constant) uniform PC {
		int drawid_offs;
	};

	struct PerDraw {
		vec4 chunk_pos;
	};
#define MAX_PER_DRAW (MAX_UBO_SIZE / 16) // sizeof(PerDrawData)

	layout(std140, set = 1, binding = 0) uniform PerDrawData {
		PerDraw per_draw[MAX_PER_DRAW];
	};

	//
	struct BlockMeshVertex {
		vec4 pos;
		vec4 normal;
		vec4 uv;
	};
#define MERGE_INSTANCE_FACTOR 6
#define MAX_BLOCK_MESHES (MAX_UBO_SIZE / (48 * MERGE_INSTANCE_FACTOR)) // sizeof(BlockMeshVertex)

	layout(std140, set = 0, binding = 1) uniform BlockMeshes {
		BlockMeshVertex vertices[MAX_BLOCK_MESHES][MERGE_INSTANCE_FACTOR];
	} block_meshes;

	void main () {
		BlockMeshVertex v = block_meshes.vertices[v_meshid][gl_VertexIndex];
		vec3 mesh_pos_model	= v.pos.xyz;
		vec2 uv				= v.uv.xy;

		vec3 chunk_pos = per_draw[gl_DrawIDARB + drawid_offs].chunk_pos.xyz;

		gl_Position =		world_to_clip * vec4(mesh_pos_model + v_pos + chunk_pos, 1);
		vs.uvi =		    vec3(uv, v_texid);
	}
#endif

#ifdef _FRAGMENT
	#define ALPHA_TEST_THRES 127.0

	layout(set = 0, binding = 2) uniform sampler2DArray textures;

	void main () {
		vec4 col = texture(textures, vs.uvi);
		
		//col.rgb *= vec3(vs.brightness);
		
	#if ALPHA_TEST
		if (col.a <= ALPHA_TEST_THRES / 255.0)
			discard;
		col.a = 1.0;
	#endif

		frag_col = col;
	}
#endif
