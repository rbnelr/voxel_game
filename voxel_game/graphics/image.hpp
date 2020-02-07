#pragma once
#include "../kissmath.hpp"
#include "../util/file_io.hpp"
#include "../stb_image.hpp"
#include "../util/move_only_class.hpp"
#include "assert.h"

struct _Format {
	bool flt;
	int bits;
	int channels;
};

template <typename T>
constexpr _Format get_format ();

template<> constexpr _Format get_format<uint8  > () { return { false, 8, 1 }; } // greyscale as 8 bit uint
template<> constexpr _Format get_format<uint8v2> () { return { false, 8, 2 }; } // greyscale, alpha as 8 bit uint
template<> constexpr _Format get_format<uint8v3> () { return { false, 8, 3 }; } // rgb as 8 bit uint
template<> constexpr _Format get_format<uint8v4> () { return { false, 8, 4 }; } // rgb, alpha as 8 bit uint

template<> constexpr _Format get_format<float  > () { return { true, 32, 1 }; } // greyscale as 8 bit uint
template<> constexpr _Format get_format<float2 > () { return { true, 32, 2 }; } // greyscale, alpha as 8 bit uint
template<> constexpr _Format get_format<float3 > () { return { true, 32, 3 }; } // rgb as 8 bit uint
template<> constexpr _Format get_format<float4 > () { return { true, 32, 4 }; } // rgb, alpha as 8 bit uint

template <typename T>
class Image {
public:
	MOVE_ONLY_CLASS(Image)
	static void swap (Image& l, Image& r) {
		std::swap(l.pixels, r.pixels);
		std::swap(l.size, r.size);
	}

private:
	T* pixels = nullptr; // malloc'ed so we can put stb_image data directly into this
public:
	int2 size = -1;

	~Image () {
		if (pixels)
			free(pixels);
	}

	Image () {}
	Image (int2 size): pixels{(T*)malloc(size.x * size.y * sizeof(T))}, size{size} {}
	
	// Loads a image file from disk
	Image (const char* filepath) {
		if (!load_from_file(filepath, this))
			assert(false);
	}

	inline T* data () {
		return pixels;
	}
	inline T const* data () const {
		return pixels;
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
		return get(pos.x, pos.y, val);
	}

	// Loads a image file from disk, potentially converting it to the target pixel type
	static bool load_from_file (const char* filepath, Image<T>* out) {
		uint64_t file_size;
		auto file_data = kiss::read_binary_file(filepath, &file_size);
		if (!file_data)
			return false;

		auto format = get_format<T>();

		T* pixels;
		int2 size;
		int n;

		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up

		if (format.flt) {
			pixels = (T*)stbi_loadf_from_memory(file_data.get(), (int)file_size, &size.x, &size.y, &n, format.channels);
			if (!pixels)
				return false;
		} else {
			switch (format.bits) {

				case 8: {
					pixels = (T*)stbi_load_from_memory(file_data.get(), (int)file_size, &size.x, &size.y, &n, format.channels);
					if (!pixels)
						return false;
				} break;

				case 16: {
					pixels = (T*)stbi_load_16_from_memory(file_data.get(), (int)file_size, &size.x, &size.y, &n, format.channels);
					if (!pixels)
						return false;
				} break;

				default:
					assert(false);
			}
		}

		out->pixels = pixels;
		out->size = size;
		return true;
	}

	//// Image manipulation here:
	// rotate by 90 180 deg
	// resize
	// etc.
};
