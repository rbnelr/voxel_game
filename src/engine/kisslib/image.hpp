#pragma once
#include "kissmath.hpp"
#include "macros.hpp"
#include "file_io.hpp"
#include "stb_image.hpp"
#include "assert.h"

namespace {
	struct _Format {
		bool flt;
		int bits;
		int channels;
	};

	template <typename T>
	constexpr inline _Format get_format ();

	// 8 bit uint channels
	template<> constexpr inline _Format get_format<uint8_t> () { return { false, 8, 1 }; } // greyscale (srgb)
	template<> constexpr inline _Format get_format<uint8v2> () { return { false, 8, 2 }; } // greyscale (srgb) + alpha
	template<> constexpr inline _Format get_format<uint8v3> () { return { false, 8, 3 }; } // srgb
	template<> constexpr inline _Format get_format<uint8v4> () { return { false, 8, 4 }; } // srgb + alpha

	// float channels
	template<> constexpr inline _Format get_format<float  > () { return { true, 32, 1 }; } // greyscale (linear)
	template<> constexpr inline _Format get_format<float2 > () { return { true, 32, 2 }; } // greyscale (linear) + alpha
	template<> constexpr inline _Format get_format<float3 > () { return { true, 32, 3 }; } // rgb (linear)
	template<> constexpr inline _Format get_format<float4 > () { return { true, 32, 4 }; } // rgb (linear) + alpha

	template <typename T>
	inline T* _stbi_load_from_memory (unsigned char* file_data, uint64_t file_size, int2* size, bool top_down=false) {
		constexpr _Format F = get_format<T>();

		stbi_set_flip_vertically_on_load(!top_down); // OpenGL has textues bottom-up, Vulkan top-down

		int2 sz;
		int n;
		void* pixels = nullptr;

		if (F.flt)
			pixels = (void*)stbi_loadf_from_memory(file_data, (int)file_size, &sz.x, &sz.y, &n, F.channels);
		else if (F.bits == 8)
			pixels = (void*)stbi_load_from_memory(file_data, (int)file_size, &sz.x, &sz.y, &n, F.channels);
		else if (F.bits == 16)
			pixels = (void*)stbi_load_16_from_memory(file_data, (int)file_size, &sz.x, &sz.y, &n, F.channels);

		if (pixels) *size = sz;
		return (T*)pixels;
	}
}

template <typename T>
struct Image {
	MOVE_ONLY_CLASS(Image)
public:
	static void swap (Image& l, Image& r) {
		std::swap(l.pixels, r.pixels);
		std::swap(l.size, r.size);
	}

	T* pixels = nullptr; // malloc this instead of unique_ptr so we can simply put the stb returned ptr in here
	int2 size = -1;

	~Image () {
		if (pixels)
			free(pixels);
	}

	Image () {}
	Image (int2 size): pixels{ (T*)malloc(size.x * size.y * sizeof(T)) }, size{size} {}

	// Loads an image file from disk
	Image (const char* filepath) {
		if (!load_from_file(filepath, this))
			assert(false);
	}

	inline T& get (int x, int y) {
		assert(x >= 0 && y >= 0 && x < size.x && y < size.y);
		return pixels[y * size.x + x];
	}
	inline T& get (int2 pos) {
		return get(pos.x, pos.y);
	}
	inline T const& get (int x, int y) const {
		assert(x >= 0 && y >= 0 && x < size.x && y < size.y);
		return pixels[y * size.x + x];
	}
	inline T const& get (int2 pos) const {
		return get(pos.x, pos.y);
	}

	inline void set (int x, int y, T const& val) {
		assert(x >= 0 && y >= 0 && x < size.x && y < size.y);
		pixels[y * size.x + x] = val;
	}
	inline void set (int2 pos, T const& val) {
		return set(pos.x, pos.y, val);
	}

	// Loads a image file from disk, potentially converting it to the target pixel type
	static bool load_from_file (const char* filepath, Image<T>* out, bool top_down=false) {
		uint64_t file_size;
		auto file_data = kiss::load_binary_file(filepath, &file_size);
		if (!file_data)
			return false;

		int2 size;
		T* pixels = _stbi_load_from_memory<T>(file_data.get(), file_size, &size, top_down);
		if (!pixels)
			return false;

		out->pixels = pixels;
		out->size = size;
		return true;
	}

	void clear (T col) {
		for (size_t i=0; i<(size_t)(size.x * size.y); ++i)
			pixels[i] = col;
	}

	static void blit_rect (Image<T> const& src, int2 src_pos, Image<T>& dst, int2 dst_pos, int2 size) {
		assert(	src_pos.x >= 0 && (src_pos.x + size.x) <= src.size.x &&
				src_pos.y >= 0 && (src_pos.y + size.y) <= src.size.y &&
				dst_pos.x >= 0 && (dst_pos.x + size.x) <= dst.size.x &&
				dst_pos.y >= 0 && (dst_pos.y + size.y) <= dst.size.y );

		if (src.size.x == dst.size.x && src.size.x == size.x) {
			// single memcpy if copy rect is whole width of src and dst
			memcpy(	dst.pixels + dst_pos.y * dst.size.x + dst_pos.x,
					src.pixels + src_pos.y * src.size.x + src_pos.x, size.x * size.y * sizeof(T) );
		} else {
			// memcpy rect for each row
			for (int y=0; y<size.y; ++y) {
				#if 0
					for (int x=0; x<size.x; ++x) {
						dst.set(p.x + dst_pos.x, p.y + dst_pos.y, src.get(p.x + src_pos.x, p.y + src_pos.y));
					}
				#else
					memcpy(	dst.pixels + (y + dst_pos.y) * dst.size.x + dst_pos.x,
							src.pixels + (y + src_pos.y) * src.size.x + src_pos.x, size.x * sizeof(T) );
				#endif
			}
		}
	}
	static Image<T> rect_copy (Image<T> const& src, int2 src_pos, int2 size) {
		Image<T> img = Image<T>(size);
		rect_copy(src, src_pos, img, 0, size);
		return img;
	}
};
