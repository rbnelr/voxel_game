#version 430 core // for findMSB, SSBO
#extension GL_NV_gpu_shader5 : enable

#define ALLOW_WIREFRAME 1
$include "common.glsl"
$include "fog.glsl"

$if vertex
	struct VoxelInstance {
		int8_t		posx;
		int8_t		posy;
		int8_t		posz;
		uint8_t		scale_face; // [4bit scale][4bit face_id]
		int			tex_indx;
	};

	layout(std430, binding = 0) readonly restrict buffer InstanceData {
		VoxelInstance instance_data[];
	};

	uniform vec3 chunk_pos;
	uniform float chunk_lod_size; // 1 << chunk lod

	out vec3	vs_pos_cam;
	out vec2	vs_uv;
	out float	vs_tex_indx;

	const int FACE_INDICES[6] = {
		1,3,0, 0,3,2
	};
	const vec3 FACES[6][4] = {
		{ // X- face
			vec3(0,1,0),
			vec3(0,0,0),
			vec3(0,1,1),
			vec3(0,0,1),
		},
		{ // X+ face
			vec3(1,0,0),
			vec3(1,1,0),
			vec3(1,0,1),
			vec3(1,1,1),
		},
		{ // Y- face
			vec3(0,0,0),
			vec3(1,0,0),
			vec3(0,0,1),
			vec3(1,0,1),
		},
		{ // Y+ face
			vec3(1,1,0),
			vec3(0,1,0),
			vec3(1,1,1),
			vec3(0,1,1),
		},
		{ // Z- face
			vec3(0,1,0),
			vec3(1,1,0),
			vec3(0,0,0),
			vec3(1,0,0),
		},
		{ // Z+ face
			vec3(0,0,1),
			vec3(1,0,1),
			vec3(0,1,1),
			vec3(1,1,1),
		},
	};
	const vec2 UVS[4] = {
		vec2(0, 0),
		vec2(1, 0),
		vec2(0, 1),
		vec2(1, 1),
	};

	void main () {
		int instance_id	= gl_VertexID / 6;
		int vertex_id	= gl_VertexID % 6;

		VoxelInstance i = instance_data[instance_id];

		int face			= (int)i.scale_face & 0x0f;
		int scale			= ((int)i.scale_face >> 4) & 0x0f;
		float size			= float(1 << scale);
		vec3 pos			= vec3((float)i.posx, (float)i.posy, (float)i.posz);

		int idx				= FACE_INDICES[gl_VertexID % 6];

		vec3 pos_model		= (FACES[face][idx] * size + pos) * chunk_lod_size;
		vec2 uv				= UVS[idx] * size * chunk_lod_size;

		//
		vec4 pos_cam = world_to_cam * vec4(pos_model + chunk_pos, 1);
		
		gl_Position =		cam_to_clip * pos_cam;

		vs_pos_cam =		pos_cam.xyz;
		vs_uv =		        uv;
		vs_tex_indx =		float(i.tex_indx);

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	in vec3		vs_pos_cam;
	in vec2	    vs_uv;
	in float	vs_tex_indx;

	uniform	sampler2DArray tile_textures;

	uniform bool alpha_test;
	#define ALPHA_TEST_THRES 127.0

	void main () {
		float dist_sqr = dot(vs_pos_cam, vs_pos_cam);
		vec3 dir_world = (cam_to_world * vec4(vs_pos_cam, 0)).xyz / sqrt(dist_sqr);
		
		vec4 col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
	
		//col.rgb *= vec3(vs_brightness);

		if (alpha_test) {
			if (col.a <= ALPHA_TEST_THRES / 255.0)
				DISCARD();
			col.a = 1.0;
		}

		col.rgb = apply_fog(col.rgb, dist_sqr, dir_world);
		FRAG_COL(col);
	}
$endif
