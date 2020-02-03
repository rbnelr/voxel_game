#pragma once
#include "gl.hpp"
#include "image.hpp"
#include "../dear_imgui.hpp"

class Sampler2D {
	gl::Sampler sampler;

public:
	gl::Enum mag_filter = gl::Enum::NEAREST;
	gl::Enum min_filter = gl::Enum::LINEAR_MIPMAP_LINEAR;

	gl::Enum wrap_s = gl::Enum::REPEAT;
	gl::Enum wrap_t = gl::Enum::REPEAT;

	int mipmap_levels = 1000;

	float lod_bias = 0;

	float anisotropy = 16;

	Sampler2D () {
		set();
	}

	void set () {
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, (GLint)mag_filter);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (GLint)min_filter);

		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, (GLint)wrap_s);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, (GLint)wrap_t);

		glSamplerParameteri(sampler, GL_TEXTURE_MAX_LOD, mipmap_levels);
		glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, lod_bias);

		if (GLAD_GL_ARB_texture_filter_anisotropic)
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
		else if (GLAD_GL_EXT_texture_filter_anisotropic)
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	}

	void bind (int texture_unit) {
		glBindSampler(texture_unit, sampler);
	}

	void imgui (const char* name) {
		using namespace gl;
		static constexpr Enum mag_filters[] = { Enum::NEAREST, Enum::LINEAR };
		static constexpr const char* mag_filters_str = "NEAREST\0LINEAR";

		static constexpr Enum min_filters[] = {
			Enum::NEAREST, Enum::LINEAR, Enum::NEAREST_MIPMAP_NEAREST, Enum::LINEAR_MIPMAP_NEAREST, Enum::NEAREST_MIPMAP_LINEAR, Enum::LINEAR_MIPMAP_LINEAR	};
		static constexpr const char* min_filters_str = "NEAREST\0LINEAR\0NEAREST_MIPMAP_NEAREST\0LINEAR_MIPMAP_NEAREST\0NEAREST_MIPMAP_LINEAR\0LINEAR_MIPMAP_LINEAR";

		static constexpr Enum wraps[] = { Enum::REPEAT, Enum::MIRRORED_REPEAT, Enum::CLAMP_TO_EDGE, Enum::CLAMP_TO_BORDER, Enum::MIRROR_CLAMP_TO_EDGE };
		static constexpr const char* wraps_str = "REPEAT\0MIRRORED_REPEAT\0CLAMP_TO_EDGE\0CLAMP_TO_BORDER\0MIRROR_CLAMP_TO_EDGE";

		if (!imgui_push(name, "Sampler2D")) return;

		auto _indexof = [] (gl::Enum const* arr, gl::Enum val) {
			for (int i=0; i<100; ++i) {
				if (arr[i] == val)
					return i;
			}
			return 0;
		};

		bool changed = false;

		int cur = _indexof(mag_filters, mag_filter);
		changed = ImGui::Combo("mag_filter", &cur, mag_filters_str) || changed;
		mag_filter = mag_filters[cur];

		cur = _indexof(min_filters, min_filter);
		changed = ImGui::Combo("min_filter", &cur, min_filters_str) || changed;
		min_filter = min_filters[cur];

		cur = _indexof(wraps, wrap_s);
		changed = ImGui::Combo("wrap_s", &cur, wraps_str) || changed;
		wrap_s = wraps[cur];

		cur = _indexof(wraps, wrap_t);
		changed = ImGui::Combo("wrap_t", &cur, wraps_str) || changed;
		wrap_t = wraps[cur];

		cur = mipmap_levels == 1000 ? 0 : mipmap_levels;
		changed = ImGui::SliderInt("mipmap_levels", &cur, 0, 20) || changed;
		mipmap_levels = cur == 0 ? 1000 : cur;

		changed = ImGui::SliderFloat("lod_bias", &lod_bias, -2, 2) || changed;

		cur = roundi(anisotropy);
		changed = ImGui::SliderInt("anisotropy", &cur, 1, 16) || changed;
		anisotropy = (float)cur;

		imgui_pop();

		if (changed)
			set();
	}
};

class Texture2D {
	//std::string name;
	gl::Texture tex;
public:
	int2 size; // size in pixels

	// Construct empty Texture2D (can upload image later)
	Texture2D ();

	// Construct by loading from file
	Texture2D (char const* filename, bool gen_mips=true);

	// Construct by Texture2D with uploaded image (can reupload different image later)
	template <typename T>
	inline Texture2D (Image<T> const& img, bool gen_mips=true) {
		upload(img, gen_mips);
	}

	// Construct by Texture2D with uploaded image, select srgb mode (only T=uint8v3 or T=uint8v4) (can reupload different image later)
	inline Texture2D (Image<uint8v3> const& img, bool srgb, bool gen_mips) {
		upload(img, srgb, gen_mips);
	}

	// Construct by Texture2D with uploaded image, select srgb mode (only T=uint8v3 or T=uint8v4) (can reupload different image later)
	inline Texture2D (Image<uint8v4> const& img, bool srgb, bool gen_mips) {
		upload(img, srgb, gen_mips);
	}

	// Manual uploading of mipmaps might require
	///////////// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmaps);

	// Upload image to mipmap
	void upload_mip (int mip, int2 size, void* data, GLenum internal_format, GLenum format, GLenum type);

	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	void upload (int2 size, void* data, bool gen_mips, GLenum internal_format, GLenum format, GLenum type);

	void bind ();

	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<uint8> const& img) {
		upload_mip(mip, img.size, (void*)img.data(), GL_R8, GL_RED, GL_UNSIGNED_BYTE);
	}
	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<uint8v2> const& img) {
		upload_mip(mip, img.size, (void*)img.data(), GL_RG8, GL_RG, GL_UNSIGNED_BYTE);
	}
	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<uint8v3> const& img, bool srgb=true) {
		upload_mip(mip, img.size, (void*)img.data(), srgb ? GL_SRGB8        : GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
	}
	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<uint8v4> const& img, bool srgb=true) {
		upload_mip(mip, img.size, (void*)img.data(), srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
	}

	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<float> const& img) {
		upload_mip(mip, img.size, (void*)img.data(), GL_R32F, GL_RED, GL_FLOAT);
	}
	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<float2> const& img) {
		upload_mip(mip, img.size, (void*)img.data(), GL_RG32F, GL_RG, GL_FLOAT);
	}
	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<float3> const& img) {
		upload_mip(mip, img.size, (void*)img.data(), GL_RGB32F, GL_RGB, GL_FLOAT);
	}
	// Upload new image to mipmap
	inline void upload_mip (int mip, Image<float4> const& img) {
		upload_mip(mip, img.size, (void*)img.data(), GL_RGBA32F, GL_RGBA, GL_FLOAT);
	}

	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	inline void upload (Image<uint8> const& img, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
	}
	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	inline void upload (Image<uint8v2> const& img, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, GL_RG8, GL_RG, GL_UNSIGNED_BYTE);
	}
	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	inline void upload ( Image<uint8v3> const& img, bool srgb=true, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, srgb ? GL_SRGB8        : GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
	}
	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	inline void upload (Image<uint8v4> const& img, bool srgb=true, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
	}

	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	inline void upload (Image<float> const& img, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, GL_R32F, GL_RED, GL_FLOAT);
	}
	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	inline void upload (Image<float2> const& img, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, GL_RG32F, GL_RG, GL_FLOAT);
	}
	// Upload image to texture, texture.size becomes size and optionally generate mipmaps
	inline void upload (Image<float3> const& img, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, GL_RGB32F, GL_RGB, GL_FLOAT);
	}
	inline void upload (Image<float4> const& img, bool gen_mips=true) {
		upload(img.size, (void*)img.data(), gen_mips, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	}
};

class Texture2DArray {
	gl::Texture tex;

public:
	int2 size; // size in pixels
	int count; // number of slots in array

	Texture2DArray ();

	// Construct empty Texture2D (can upload images later)
	template <typename T, bool srgb=true>
	Texture2DArray (int2 size, int count) {
		alloc<T, srgb>(size, count);
	}

	// Upload one image to slot in texture array
	void alloc (int2 size, int count, GLenum internal_format, bool mipmaps=true);

	// Upload one image to slot in texture array
	void upload (int index, void* data, GLenum format, GLenum type);

	void gen_mipmaps ();

	// if mipmaps=true then call gen_mipmaps after all textues have been uploaded
	template <typename T, bool srgb>
	void alloc (int2 size, int count, bool mipmaps=true);

	// if mipmaps=true then call gen_mipmaps after all textues have been uploaded
	template<> void alloc<uint8, false> (int2 size, int count, bool mipmaps) {
		alloc(size, count, GL_R8, mipmaps);
	}
	// if mipmaps=true then call gen_mipmaps after all textues have been uploaded
	template<> void alloc<uint8v4, false> (int2 size, int count, bool mipmaps) {
		alloc(size, count, GL_RGBA8, mipmaps);
	}
	// if mipmaps=true then call gen_mipmaps after all textues have been uploaded
	template<> void alloc<uint8v4, true> (int2 size, int count, bool mipmaps) {
		alloc(size, count, GL_SRGB8_ALPHA8, mipmaps);
	}

	// Upload new image to mipmap
	inline void upload (int index, Image<uint8> const& img) {
		assert(equal(img.size, size));
		upload(index, (void*)img.data(), GL_RED, GL_UNSIGNED_BYTE);
	}
	// Upload new image to mipmap
	inline void upload (int index, Image<uint8v4> const& img) {
		assert(equal(img.size, size));
		upload(index, (void*)img.data(), GL_RGBA, GL_UNSIGNED_BYTE);
	}

	// alloc, upload and gen mipmaps from array of images
	template <typename T, bool srgb=true>
	inline void upload (std::vector< Image<T> > const& images, bool mipmaps=true) {
		assert(images.size() > 0);

		alloc<T, srgb>(images[0].size, (int)images.size(), mipmaps);

		for (int i=0; i<(int)images.size(); ++i) {
			upload(i, images[i]);
		}

		if (mipmaps)
			gen_mipmaps();
	}

	void bind ();
};
