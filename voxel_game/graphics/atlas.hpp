#pragma 
#include "../kissmath.hpp"
#include "vulkan.hpp"
#include "util/image.hpp"
#include "../stb_rect_pack.hpp"
#include "dear_imgui.hpp"
#include <string>

struct AtlasedTexture {
	std::string filepath;

	int2 size_px;

	// in uv
	float2 atlas_pos;
	float2 atlas_size;
};

template <typename T>
Texture2D load_texture_atlas (std::initializer_list<AtlasedTexture*> textures, int2 atlas_size, T clear_col=0, int border_px=8, bool srgb=true, bool gen_mips=true) {

	std::vector<Image<T>> images;
	for (auto& tex : textures) {
		Image<T> img;
		if (!Image<T>::load_from_file(tex->filepath.c_str(), &img))
			clog(ERROR, "Could not load file \"%s\" to load texture for atlas!", tex->filepath.c_str());
		else
			images.push_back(std::move(img));
	}

	stbrp_context context;
	std::vector<stbrp_node> nodes (atlas_size.x);
	std::vector<stbrp_rect> rects ((int)images.size());

	for (int i=0; i<(int)images.size(); ++i) {
		rects[i].id = i;
		rects[i].w = images[i].size.x;
		rects[i].h = images[i].size.y;
	}

	stbrp_init_target(&context, atlas_size.x, atlas_size.y, nodes.data(), (int)nodes.size());
	if (!stbrp_pack_rects(&context, rects.data(), (int)rects.size()))
		clog(ERROR, "Could not pack some textures into texture atlas!");

	Image<T> atlas_img = Image<T>(atlas_size);
	atlas_img.clear(clear_col);

	for (auto& rect : rects) {
		int i = rect.id;

		textures.begin()[i]->size_px = images[i].size;

		if (rects[i].was_packed) {
			int2 pos = int2(rect.x, rect.y);
			int2 size = images[i].size;

			textures.begin()[i]->atlas_pos = (float2)pos / (float2)atlas_size;
			textures.begin()[i]->atlas_size = (float2)size / (float2)atlas_size;

			Image<T>::rect_copy(images[i], 0, atlas_img, pos, size);
		} else {
			textures.begin()[i]->atlas_pos = 0;
			textures.begin()[i]->atlas_size = 1;
		}
	}

	return Texture2D();
	//return Texture2D(atlas_img, srgb, gen_mips);
}
