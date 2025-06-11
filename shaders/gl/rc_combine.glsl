#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 4) restrict uniform image2D out_tex;

uniform ivec2 dispatch_size;
uniform ivec2 num_probes;
uniform ivec2 ray_regions;
uniform sampler2D cascade0;

void main () {
	ivec2 px_pos = ivec2(gl_GlobalInvocationID.xy);
	if (px_pos.x >= dispatch_size.x || px_pos.y >= dispatch_size.y)
		return;
	
	vec4 col = vec4(0);
	for (int y=0; y<ray_regions.y; y++)
	for (int x=0; x<ray_regions.x; x++) {
		ivec2 sample_pos = num_probes * ivec2(x,y) + px_pos;
		col += texelFetch(cascade0, sample_pos, 0);
	}
	col /= float(ray_regions.x * ray_regions.y);
	
	//if (col.a < 0.0001) col = vec4(vec3(((px_pos.x%2) ^ (px_pos.y%2)) == 0 ? 0.1 : 0.4), 1.0);
	
	imageStore(out_tex, px_pos, col);
}
