#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3 normal;
	vec3 uvi;
} vs;

#ifdef _VERTEX
	uniform int meshid;
	uniform float texids[64];
	uniform mat4 model_to_world;

	struct BlockMeshVertex {
		vec4 pos;
		vec4 normal;
		vec4 uv;
	};
	#define MERGE_INSTANCE_FACTOR 6

	layout(std430, binding = 1) readonly buffer BlockMeshes {
		//BlockMeshVertex vertices[][MERGE_INSTANCE_FACTOR];
		BlockMeshVertex vertices[];
	} block_meshes;

	void main () {
		BlockMeshVertex v = block_meshes.vertices[meshid * MERGE_INSTANCE_FACTOR + gl_VertexID];

		float texid = texids[gl_VertexID / 6];

		gl_Position = view.world_to_clip * (model_to_world * vec4(v.pos.xyz, 1.0));

		vs.normal = mat3(model_to_world) * v.normal.xyz;
		vs.uvi = vec3(v.uv.xy, texid);
	}
#endif

#ifdef _FRAGMENT
	uniform sampler2DArray tile_textures;

	const vec3 light_dir_world = normalize(vec3(1.33, 1.7, 5.9));
	vec3 basic_lighting (vec3 normal_world) {
		vec3 light = vec3(0.1, 0.15, 0.4) * 0.5;
		light += vec3(0.9, 0.9, 0.6) * max(dot(normal_world, light_dir_world), 0.0);
		return light;
	}

	out vec4 frag_col;
	void main () {
		vec4 col = texture(tile_textures, vs.uvi);

		col.rgb *= basic_lighting(normalize(vs.normal));

	#ifdef _WIREFRAME
		col = vec4(1.0);
	#endif
		frag_col = col;
	}
#endif
