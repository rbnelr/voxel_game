#version 460 core // for GL_ARB_shader_draw_parameters

#include "common.glsl"

layout(local_size_x = 1, local_size_y = 1) in;

layout(std430, binding=0) buffer Test {
	vec4 cols[];
};

uniform vec2 img_size;

layout(rgba16f, binding = 0) uniform image2D img;

void main () {
	vec2 pos = gl_GlobalInvocationID.xy;

	//vec4 col = vec4(pos / img_size, 0.0, 1.0);
	vec4 col = vec4(cols[int(pos.x)].xyz, 1.0);

	imageStore(img, ivec2(pos), col);
}
