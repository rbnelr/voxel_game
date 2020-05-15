
struct Test {
	float3 a;
	float3 b;
	//bool3 c;
};

void error (struct Test t) {
	// error
}

#define RECURSE(name, name2) \
void name (struct Test t) { \
	name2(t); \
}

RECURSE( a1, error)
RECURSE( a2,  a1)
RECURSE( a4,  a2)
RECURSE( a8,  a4)
RECURSE(a16,  a8)
RECURSE(a32, a16)
RECURSE(a64, a32)

//struct Data {
//	int mirror_mask_int; // algo assumes ray.dir x,y,z >= 0, x,y,z < 0 cause the ray to be mirrored, to make all components positive, this mask is used to also mirror the octree nodes to make iteration work
//	bool3 mirror_mask; // algo assumes ray.dir x,y,z >= 0, x,y,z < 0 cause the ray to be mirrored, to make all components positive, this mask is used to also mirror the octree nodes to make iteration work
//
//	float3 ray_pos;
//	float3 ray_dir;
//
//	global const uint* SVO;
//	float4 col;
//};
//
//void ray_for_pixel (struct Data* d, int2 pixel, int2 resolution/*, Camera_View const& view*/) {
//	float2 ndc = ((float2)pixel + 0.5f) / (float2)resolution * 2 - 1;
//
//	float4 clip = float4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;
//
//	float3 pos_cam = (float3)(view.clip_to_cam * clip);
//	float3 dir_cam = pos_cam;
//
//	d->ray_pos = (float3)( view.cam_to_world * float4(pos_cam, 1) );
//	d->ray_dir = (float3)( view.cam_to_world * float4(dir_cam, 0) );
//	d->ray_dir = normalize(d->ray_dir);
//}

void kernel raycast (global const uint* SVO, global float4* image,
		const int width, const int height) {

	//struct Data d;
	//d.col = float4(0,0,0,0);
	//d.SVO = SVO;
	//
	//ray_for_pixel(&d, pixel, image->size, view);

	int x = get_global_id(0);
	int y = get_global_id(1);

	float4 col = ((float)x / (float)width, (float)y / (float)height, 0, 1);

	image[y * width + x] = col;

	//d.mirror_mask_int = 0;
	//
	//uint32_t node_data = nodes[root]._children;
	//float3 min = pos;
	//float3 max = pos + (float3)(float)CHUNK_DIM_X;
	//float3 mid = 0.5f * (min + max);
	//
	//for (int i=0; i<3; ++i) {
	//	bool mirror = d.ray_dir[i] < 0;
	//
	//	d.ray_dir[i] = abs(d.ray_dir[i]);
	//	d.mirror_mask[i] = mirror;
	//	if (mirror) d.ray_pos[i] = mid[i] * 2 - d.ray_pos[i];
	//	if (mirror) d.mirror_mask_int |= 1 << i;
	//}
	//
	//float3 rdir_inv = 1.0f / d.ray_dir;
	//
	//float3 t0 = (min - d.ray_pos) * rdir_inv;
	//float3 t1 = (max - d.ray_pos) * rdir_inv;
	//
	//if (max_component(t0) < min_component(t1))
	//	traverse_subtree(d, node_data, min, max, t0, t1);
	//
	//return d.hit;
}
