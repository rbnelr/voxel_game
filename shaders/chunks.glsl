#version 460 // for GL_ARB_shader_draw_parameters

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
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
	//struct BlockMeshVertex {
	//	vec4 pos;
	//	vec4 normal;
	//	vec4 uv;
	//};
	//#define MERGE_INSTANCE_FACTOR 6
	//#define MAX_BLOCK_MESHES (MAX_UBO_SIZE / (48 * MERGE_INSTANCE_FACTOR)) // sizeof(BlockMeshVertex)
	//
	//layout(std140, set = 0, binding = 1) uniform BlockMeshes {
	//	BlockMeshVertex vertices[MAX_BLOCK_MESHES][MERGE_INSTANCE_FACTOR];
	//} block_meshes;
	//
	//void main () {
	//	BlockMeshVertex v = block_meshes.vertices[v_meshid][gl_VertexIndex];
	//	vec3 mesh_pos_model	= v.pos.xyz;
	//	vec2 uv				= v.uv.xy;
	//
	//	gl_Position =		world_to_clip * vec4(mesh_pos_model + v_pos * FIXEDPOINT_FAC + chunk_pos, 1);
	//	vs.uvi =		    vec3(uv, v_texid);
	//}

	struct BlockMeshVertex {
		vec3 pos;
		vec2 uv;
	};
	
    const BlockMeshVertex vertices[21][6] = {
        {
            BlockMeshVertex( vec3(0.000000,0.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,1.000000,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(0.000000,0.000000,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.000000,0.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,1.000000,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,1.000000,0.000000), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(1.000000,1.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,0.000000), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(1.000000,0.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,0.000000,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,0.000000,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,0.000000,0.000000), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.000000,1.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(0.000000,1.000000,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.000000,1.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,0.000000), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.000000,1.000000,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,0.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,0.000000,0.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,1.000000,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,0.000000), vec2(1.000000,0.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.000000,0.000000,1.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,1.000000,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.000000,0.000000,1.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,0.000000,1.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(1.000000,1.000000,1.000000), vec2(1.000000,0.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.425781,-0.195313,0.593750), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.941406,0.378906,-0.039063), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(0.574219,1.195313,0.406250), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.425781,-0.195313,0.593750), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.574219,1.195313,0.406250), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.058594,0.621094,1.039063), vec2(1.000000,0.000000) ),
        },
        {
            BlockMeshVertex( vec3(-0.144531,0.210938,0.500000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.371094,0.789063,-0.132813), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.144531,0.789063,0.500000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(-0.144531,0.210938,0.500000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(1.144531,0.789063,0.500000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.628906,0.210938,1.132813), vec2(1.000000,0.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.296875,0.089844,-0.039063), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(-0.070313,0.906250,0.406250), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.703125,0.910156,1.039063), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(0.296875,0.089844,-0.039063), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.703125,0.910156,1.039063), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(1.070313,0.093750,0.593750), vec2(1.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.425781,-0.195313,0.593750), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.058594,0.621094,1.039063), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.574219,1.195313,0.406250), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.425781,-0.195313,0.593750), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.574219,1.195313,0.406250), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.941406,0.378906,-0.039063), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(-0.144531,0.210938,0.500000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.628906,0.210938,1.132813), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(1.144531,0.789063,0.500000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(-0.144531,0.210938,0.500000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(1.144531,0.789063,0.500000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.371094,0.789063,-0.132813), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.296875,0.089844,-0.039063), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(1.070313,0.093750,0.593750), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.703125,0.910156,1.039063), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(0.296875,0.089844,-0.039063), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.703125,0.910156,1.039063), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(-0.070313,0.906250,0.406250), vec2(0.000000,0.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.437500,0.437500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.437500,0.437500,0.625000), vec2(0.562500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.562500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.437500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.437500,0.562500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.562500,0.000000), vec2(0.437500,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.437500,0.562500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.437500,0.562500,0.625000), vec2(0.562500,0.375000) ),
            BlockMeshVertex( vec3(0.562500,0.562500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.562500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.562500,0.562500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.562500,0.562500,0.000000), vec2(0.437500,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.562500,0.562500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.562500,0.562500,0.625000), vec2(0.562500,0.375000) ),
            BlockMeshVertex( vec3(0.562500,0.437500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.562500,0.562500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.562500,0.437500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.562500,0.437500,0.000000), vec2(0.437500,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.562500,0.437500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.562500,0.437500,0.625000), vec2(0.562500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.437500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.562500,0.437500,0.000000), vec2(0.562500,1.000000) ),
            BlockMeshVertex( vec3(0.437500,0.437500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.437500,0.000000), vec2(0.437500,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.562500,0.562500,0.625000), vec2(0.562500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.562500,0.625000), vec2(0.437500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.437500,0.625000), vec2(0.437500,0.500000) ),
            BlockMeshVertex( vec3(0.562500,0.562500,0.625000), vec2(0.562500,0.375000) ),
            BlockMeshVertex( vec3(0.437500,0.437500,0.625000), vec2(0.437500,0.500000) ),
            BlockMeshVertex( vec3(0.562500,0.437500,0.625000), vec2(0.562500,0.500000) ),
        },
        {
            BlockMeshVertex( vec3(0.855469,0.855469,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.855469,0.855469,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.144531,0.144531,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.855469,0.855469,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.144531,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.144531,0.144531,0.000000), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.855469,0.144531,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.855469,0.144531,1.000000), vec2(1.000000,0.000000) ),
            BlockMeshVertex( vec3(0.144531,0.855469,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.855469,0.144531,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.855469,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.144531,0.855469,0.000000), vec2(0.000000,1.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.855469,0.855469,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.144531,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.144531,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.855469,0.855469,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.144531,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.855469,0.855469,1.000000), vec2(1.000000,0.000000) ),
        },
        {
            BlockMeshVertex( vec3(0.855469,0.144531,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.855469,0.000000), vec2(0.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.855469,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.855469,0.144531,0.000000), vec2(1.000000,1.000000) ),
            BlockMeshVertex( vec3(0.144531,0.855469,1.000000), vec2(0.000000,0.000000) ),
            BlockMeshVertex( vec3(0.855469,0.144531,1.000000), vec2(1.000000,0.000000) ),
        },
    };
	
	void main () {
		BlockMeshVertex v = vertices[meshid][gl_VertexIndex];
		vec3 mesh_pos_model	= v.pos;
		vec2 uv				= v.uv;
	
		gl_Position =		world_to_clip * vec4(mesh_pos_model + voxel_pos * FIXEDPOINT_FAC + chunk_pos, 1);
		vs.uvi =		    vec3(uv, texid);
	}
#endif

#ifdef _FRAGMENT
	#define ALPHA_TEST_THRES 127.0

	layout(set = 0, binding = 2) uniform sampler2DArray textures;

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
		
		frag_col = col;
	}
#endif
