#version 460 // for GL_ARB_shader_draw_parameters

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
	vec3	normal_cam;
	//float	brightness;
} vs;

#ifdef _VERTEX
	#define FIXEDPOINT_FAC (1.0 / 256.0)
	
	layout(location = 0) in vec3	voxel_pos; // pos of voxel instance in chunk
	layout(location = 1) in uint	meshid;
	layout(location = 2) in float	texid;
	
	#define MAX_UBO_SIZE (64*1024)
	
	//
	layout(push_constant) uniform PC {
		vec3 chunk_pos;
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
		BlockMeshVertex v = block_meshes.vertices[meshid][gl_VertexIndex];
		vec3 mesh_pos_model		= v.pos.xyz;
		vec3 mesh_norm_model	= v.normal.xyz;
		vec2 uv					= v.uv.xy;
	
		gl_Position =		world_to_clip * vec4(mesh_pos_model + voxel_pos * FIXEDPOINT_FAC + chunk_pos, 1);
		vs.uvi =			vec3(uv, texid);
		vs.normal_cam =		mat3(world_to_cam) * mesh_norm_model;
	}
#endif

#ifdef _FRAGMENT
	#define ALPHA_TEST_THRES 127.0

	layout(set = 0, binding = 2) uniform sampler2DArray textures;

	layout(location = 0) out vec4 frag_col;
	layout(location = 1) out vec4 frag_normal;
	void main () {
		vec4 col = texture(textures, vs.uvi);
		
	#if ALPHA_TEST && !defined(WIREFRAME)
		if (col.a <= ALPHA_TEST_THRES / 255.0)
			discard;
		col.a = 1.0;
	#endif
		
	#if WIREFRAME
		col = vec4(1.0);
	#endif
		
		vec3 norm = normalize(vs.normal_cam); // shouldn't be needed since I don't use geometry with curved geometry, but just in case
		
		frag_col = col;
		frag_normal = vec4(norm.xyz, 1.0); // alpha 1 incase blending happens to never blend normals
	}
#endif
