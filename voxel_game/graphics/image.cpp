#include "image.hpp"
#include "../stb_image.hpp"

void* _stbi_load_from_memory (unsigned char* file_data, uint64_t file_size, _Format format, int2* size) {

	int2 sz;
	int n;
	void* pixels = nullptr;

	stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up

	if (format.flt) {
		pixels = (void*)stbi_loadf_from_memory(file_data, (int)file_size, &sz.x, &sz.y, &n, format.channels);
	} else {
		switch (format.bits) {

			case 8:
				pixels = (void*)stbi_load_from_memory(file_data, (int)file_size, &sz.x, &sz.y, &n, format.channels);
				break;
			case 16:
				pixels = (void*)stbi_load_16_from_memory(file_data, (int)file_size, &sz.x, &sz.y, &n, format.channels);
				break;

			default:
				assert(false);
		}
	}

	if (pixels)
		*size = sz;
	return pixels;
}
