#version 460 core
#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec2 uv;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos_world;
	layout(location = 2) in vec2 uv;

	uniform mat4x4 obj2world;
	
	void main () {
		gl_Position = view.world_to_clip * obj2world * vec4(pos_world, 1);
		vs.uv = uv;
	}
#endif

#ifdef _FRAGMENT
	out vec4 frag_col;
#ifdef TEX_ARRAY
	uniform sampler2DArray tex;
	uniform int arr_idx = -1;
	uniform int grid_width = -1;
	void main () {
		vec2 uv = vs.uv;
		int idx = arr_idx;
		
		if (arr_idx < 0) {
			// show all array textures as a grid
			int count = textureSize(tex, 0).z;
			int w = grid_width > 0 ? grid_width : int(sqrt(float(count)));
			int h = (count + w-1) / w;
			
			uv *= vec2(ivec2(w,h));
			ivec2 coord = ivec2(uv);
			uv -= vec2(coord);
			idx = coord.x + coord.y * w;
		}
		// else show requested array index
		frag_col = texture(tex, vec3(uv, float(idx)));
	}
#else
	uniform sampler2D tex;
	void main () {
		frag_col = texture(tex, vs.uv);
	}
#endif
#endif
