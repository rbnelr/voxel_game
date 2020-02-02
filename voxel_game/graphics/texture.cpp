#include "texture.hpp"
#include "../stb_image.hpp"

Texture2D::Texture2D () {
	
}

// Construct by loading from file
Texture2D::Texture2D (char const* filename, bool gen_mips) {
	bool flt = stbi_is_hdr(filename);
	int channels;
	int2 size;

	if (!stbi_info(filename, &size.x, &size.y, &channels)) {
		channels = 4;
	}

	if (flt) {
		switch (channels) {
			case 1: upload(Image<float >(filename), gen_mips); break;
			case 2: upload(Image<float2>(filename), gen_mips); break;
			case 3: upload(Image<float3>(filename), gen_mips); break;
			case 4: upload(Image<float4>(filename), gen_mips); break;
		}
	} else {
		switch (channels) {
			case 1: upload(Image<uint8  >(filename), gen_mips); break;
			case 2: upload(Image<uint8v2>(filename), gen_mips); break;
			case 3: upload(Image<uint8v3>(filename), gen_mips); break;
			case 4: upload(Image<uint8v4>(filename), gen_mips); break;
		}
	}
}

// Upload image to mipmap
void Texture2D::upload_mip (int mip, int2 size, void* data, GLenum internal_format, GLenum format, GLenum type) {
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glBindTexture(GL_TEXTURE_2D, tex);

	glTexImage2D(GL_TEXTURE_2D, mip, internal_format, size.x,size.y, 0, format, type, data);
}

// Upload image to texture, texture.size becomes size and optionally generate mipmaps
void Texture2D::upload (int2 size, void* data, bool gen_mips, GLenum internal_format, GLenum format, GLenum type) {
	this->size = size;

	upload_mip(0, size, data, internal_format, format, type);

	if (gen_mips && size.x != 1 || size.y != 1) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}
}

void Texture2D::bind () {
	glBindTexture(GL_TEXTURE_2D, tex);
}
