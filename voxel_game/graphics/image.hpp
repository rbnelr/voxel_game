#pragma once
#include "../kissmath_colors.hpp"
#include "../kissmath.hpp"
#include <memory>
#include "assert.h"
#include "../stb_image.hpp"

struct _Format {
	bool flt;
	int bits;
	int channels;
};

template <typename T>
_Format static inline constexpr get_format ();

template<> _Format static inline constexpr get_format<uint8  > () { return { false, 8, 1 }; } // greyscale as 8 bit uint
template<> _Format static inline constexpr get_format<uint8v2> () { return { false, 8, 2 }; } // greyscale, alpha as 8 bit uint
template<> _Format static inline constexpr get_format<uint8v3> () { return { false, 8, 3 }; } // rgb as 8 bit uint
template<> _Format static inline constexpr get_format<uint8v4> () { return { false, 8, 4 }; } // rgb, alpha as 8 bit uint

template<> _Format static inline constexpr get_format<float  > () { return { true, 32, 1 }; } // greyscale as 8 bit uint
template<> _Format static inline constexpr get_format<float2 > () { return { true, 32, 2 }; } // greyscale, alpha as 8 bit uint
template<> _Format static inline constexpr get_format<float3 > () { return { true, 32, 3 }; } // rgb as 8 bit uint
template<> _Format static inline constexpr get_format<float4 > () { return { true, 32, 4 }; } // rgb, alpha as 8 bit uint

struct deleter_free {
	void operator() (void* ptr) {
		free(ptr);
	}
};

template <typename T>
struct Image {
	// need deleter_free since stb_image used malloc to alloc pixel data
	std::unique_ptr<T[], deleter_free> pixels = nullptr;
	int2 size = -1;

	Image () {}
	Image (std::unique_ptr<T[], deleter_free> pixels, int2 size): pixels{std::move(pixels)}, size{size} {}
	
	// Loads a image file from disk
	Image (const char* filepath) {
		if (!load_file(filepath, this))
			assert(false);
	}

	inline T* data () {
		return pixels.get();
	}
	inline T const* data () const {
		return pixels.get();
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
	static bool load_file (const char* filepath, Image* out) {
		auto format = get_format<T>();

		void* pixels;
		int2 size;
		int n;

		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up

		if (format.flt) {
			pixels = (void*)stbi_loadf(filepath, &size.x, &size.y, &n, format.channels);
			if (!pixels)
				return false;
		} else {
			switch (format.bits) {

				case 8: {
					pixels = (void*)stbi_load(filepath, &size.x, &size.y, &n, format.channels);
					if (!pixels)
						return false;
				} break;

				case 16: {
					pixels = (void*)stbi_load_16(filepath, &size.x, &size.y, &n, format.channels);
					if (!pixels)
						return false;
				} break;

				default:
					assert(false);
			}
		}

		assert(n == format.channels);

		*out = Image( std::unique_ptr<T[], deleter_free>((T*)pixels), size );
		return true;
	}

	//// Image manipulation here:
	// rotate by 90 180 deg
	// resize
	// etc.
};
