#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 4) restrict uniform image2D out_tex;

uniform ivec2 dispatch_size;
uniform sampler2DArray cascade0;

void main () {
	ivec2 px_pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThan(px_pos, dispatch_size))) return;
	
	int num_rays = textureSize(cascade0, 0).z;
	vec4 col = vec4(0);
	for (int ray=0; ray<num_rays; ray++) {
		col += texelFetch(cascade0, ivec3(px_pos, ray), 0);
	}
	col /= float(num_rays);
	
	//if (col.a < 0.0001) col = vec4(vec3(((px_pos.x%2) ^ (px_pos.y%2)) == 0 ? 0.1 : 0.4), 1.0);
	
	imageStore(out_tex, px_pos, col);
}
