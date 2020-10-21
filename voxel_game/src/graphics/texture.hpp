#pragma once
#include "stdafx.hpp"
#include "gl.hpp"
#include "image.hpp"

class Sampler {
	gl::Sampler sampler;

public:
	gl::Enum mag_filter = gl::Enum::LINEAR;
	gl::Enum min_filter = gl::Enum::LINEAR_MIPMAP_LINEAR;

	gl::Enum wrap_x = gl::Enum::CLAMP_TO_EDGE;
	gl::Enum wrap_y = gl::Enum::CLAMP_TO_EDGE;
	gl::Enum wrap_z = gl::Enum::CLAMP_TO_EDGE;

	int mipmap_levels = 1000;

	float lod_bias = 0;

	float anisotropy = 16;

	Sampler () {
		set();
	}
	Sampler (gl::Enum mag_filter, gl::Enum min_filter, gl::Enum wrap): mag_filter{mag_filter}, min_filter{min_filter}, wrap_x{wrap}, wrap_y{wrap}, wrap_z{wrap} {
		set();
	}

	void set () {
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, (GLint)mag_filter);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (GLint)min_filter);

		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, (GLint)wrap_x);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, (GLint)wrap_y);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, (GLint)wrap_z);

		glSamplerParameteri(sampler, GL_TEXTURE_MAX_LOD, mipmap_levels);
		glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, lod_bias);

		if (GLAD_GL_ARB_texture_filter_anisotropic)
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
		else if (GLAD_GL_EXT_texture_filter_anisotropic)
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	}

	void bind (int texture_unit) const {
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

		cur = _indexof(wraps, wrap_x);
		changed = ImGui::Combo("wrap_x", &cur, wraps_str) || changed;
		wrap_x = wraps[cur];

		cur = _indexof(wraps, wrap_y);
		changed = ImGui::Combo("wrap_y", &cur, wraps_str) || changed;
		wrap_y = wraps[cur];

		cur = _indexof(wraps, wrap_z);
		changed = ImGui::Combo("wrap_z", &cur, wraps_str) || changed;
		wrap_z = wraps[cur];

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


class Texture1D {
	//std::string name;
	gl::Texture tex;
public:
	int size = -1; // size in pixels

	Texture1D ();

	// Manual uploading of mipmaps might require
	///////////// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmaps);

	void upload_mip (int mip, void const* data, int size, GLenum internal_format, GLenum format, GLenum type);

	void upload (void const* data, int size, bool gen_mips, GLenum internal_format, GLenum format, GLenum type);

	template <typename T>
	inline void upload (Image<T> const& img, bool srgb=true, bool gen_mips=true) {
		constexpr auto format = get_format<T>();

		if (format.flt) {
			upload((float*)img.data(), img.size, format.channels, gen_mips);
		} else {
			assert(format.bits == 8);
			upload((uint8_t*)img.data(), img.size, format.channels, srgb, gen_mips);
		}
	}

	inline void upload (uint8_t const* data, int size, int channels, bool srgb, bool gen_mips) {
		GLenum internal_format, format;
		switch (channels) {
			case 1:	internal_format = GL_R8;								format = GL_RED;	break;
			case 2:	internal_format = GL_RG8;								format = GL_RG;		break;
			case 3:	internal_format = srgb ? GL_SRGB8        : GL_RGB8;		format = GL_RGB;	break;
			case 4:	internal_format = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;	format = GL_RGBA;	break;
			default:
				assert(false);
				return;
		}

		upload(data, size, gen_mips, internal_format, format, GL_UNSIGNED_BYTE);
	}

	inline void upload (float const* data, int size, int channels, bool gen_mips) {
		GLenum internal_format, format;
		switch (channels) {
			case 1:	internal_format = GL_R32F;		format = GL_RED;	break;
			case 2:	internal_format = GL_RG32F;		format = GL_RG;		break;
			case 3:	internal_format = GL_RGB32F;	format = GL_RGB;	break;
			case 4:	internal_format = GL_RGBA32F;	format = GL_RGBA;	break;
			default:
				assert(false);
				return;
		}

		upload(data, size, gen_mips, internal_format, format, GL_FLOAT);
	}

	void bind () const;
};

class Texture2D {
public:
	//std::string name;
	gl::Texture tex;
//public:
	int2 size = -1; // size in pixels

	Texture2D ();

	Texture2D (char const* filename, bool srgb=true, bool gen_mips=true);

	template <typename T>
	inline Texture2D (Image<T> const& img, bool srgb=true, bool gen_mips=true) {
		upload(img, srgb, gen_mips);
	}

	// Manual uploading of mipmaps might require
	///////////// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmaps);

	void upload_mip (int mip, void const* data, int2 size, GLenum internal_format, GLenum format, GLenum type);

	void upload (void const* data, int2 size, bool gen_mips, GLenum internal_format, GLenum format, GLenum type);
	
	template <typename T>
	inline void upload (Image<T> const& img, bool srgb=true, bool gen_mips=true) {
		constexpr auto format = get_format<T>();

		if (format.flt) {
			upload((float*)img.data(), img.size, format.channels, gen_mips);
		} else {
			assert(format.bits == 8);
			upload((uint8_t*)img.data(), img.size, format.channels, srgb, gen_mips);
		}
	}

	inline void upload (uint8_t const* data, int2 size, int channels, bool srgb, bool gen_mips) {
		GLenum internal_format, format;
		switch (channels) {
			case 1:	internal_format = GL_R8;								format = GL_RED;	break;
			case 2:	internal_format = GL_RG8;								format = GL_RG;		break;
			case 3:	internal_format = srgb ? GL_SRGB8        : GL_RGB8;		format = GL_RGB;	break;
			case 4:	internal_format = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;	format = GL_RGBA;	break;
			default:
				assert(false);
				return;
		}

		upload(data, size, gen_mips, internal_format, format, GL_UNSIGNED_BYTE);
	}

	inline void upload (float const* data, int2 size, int channels, bool gen_mips) {
		GLenum internal_format, format;
		switch (channels) {
			case 1:	internal_format = GL_R32F;		format = GL_RED;	break;
			case 2:	internal_format = GL_RG32F;		format = GL_RG;		break;
			case 3:	internal_format = GL_RGB32F;	format = GL_RGB;	break;
			case 4:	internal_format = GL_RGBA32F;	format = GL_RGBA;	break;
			default:
				assert(false);
				return;
		}

		upload(data, size, gen_mips, internal_format, format, GL_FLOAT);
	}

	void bind () const;
};


class Texture3D {
	//std::string name;
	gl::Texture tex;
public:
	int3 size = -1; // size in pixels

	Texture3D ();

	// Manual uploading of mipmaps might require
	///////////// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmaps);

	void upload_mip (int mip, void const* data, int3 size, GLenum internal_format, GLenum format, GLenum type);

	void upload (void const* data, int3 size, bool gen_mips, GLenum internal_format, GLenum format, GLenum type);

	inline void upload (uint8_t const* data, int3 size, int channels, bool srgb, bool gen_mips) {
		GLenum internal_format, format;
		switch (channels) {
			case 1:	internal_format = GL_R8;								format = GL_RED;	break;
			case 2:	internal_format = GL_RG8;								format = GL_RG;		break;
			case 3:	internal_format = srgb ? GL_SRGB8        : GL_RGB8;		format = GL_RGB;	break;
			case 4:	internal_format = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;	format = GL_RGBA;	break;
			default:
				assert(false);
				return;
		}

		upload(data, size, gen_mips, internal_format, format, GL_UNSIGNED_BYTE);
	}

	inline void upload (float const* data, int3 size, int channels, bool gen_mips) {
		GLenum internal_format, format;
		switch (channels) {
			case 1:	internal_format = GL_R32F;		format = GL_RED;	break;
			case 2:	internal_format = GL_RG32F;		format = GL_RG;		break;
			case 3:	internal_format = GL_RGB32F;	format = GL_RGB;	break;
			case 4:	internal_format = GL_RGBA32F;	format = GL_RGBA;	break;
			default:
				assert(false);
				return;
		}

		upload(data, size, gen_mips, internal_format, format, GL_FLOAT);
	}

	void bind () const;
};

class Texture2DArray {
	gl::Texture tex;

public:
	int2 size = -1; // size in pixels
	int count = -1; // number of slots in array

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
		assert(img.size == size);
		upload(index, (void*)img.data(), GL_RED, GL_UNSIGNED_BYTE);
	}
	// Upload new image to mipmap
	inline void upload (int index, Image<uint8v4> const& img) {
		assert(img.size == size);
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

	void bind () const;
};
