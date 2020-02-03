#include "texture.hpp"
#include "../stb_image.hpp"

Texture2D::Texture2D () {
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,		GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,		GL_LINEAR_MIPMAP_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,			GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,			GL_REPEAT);
}

// Construct by loading from file
Texture2D::Texture2D (char const* filename, bool gen_mips): Texture2D() {
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
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	}
}

void Texture2D::bind () {
	glBindTexture(GL_TEXTURE_2D, tex);
}

int calc_mipmap_count (int2 size) {
	int mips = 1;
	while (size.x > 1 && size.y > 1) {
		mips++;

		size.x = max(1, size.x / 2);
		size.y = max(1, size.y / 2);
	}

	return mips;
}

Texture2DArray::Texture2DArray () {
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,		GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,		GL_LINEAR_MIPMAP_LINEAR);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,			GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,			GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
}

void Texture2DArray::alloc (int2 size, int count, GLenum internal_format, bool mipmaps) {
	this->size = size;
	this->count = count;

	int mip_count = mipmaps ? calc_mipmap_count(size) : 1;

	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, mip_count, internal_format, size.x, size.y, count);
}

void Texture2DArray::upload (int index, void* data, GLenum format, GLenum type) {
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index, size.x, size.y, 1, format, type, data);
}

void Texture2DArray::gen_mipmaps () {
	if (size.x != 1 || size.y != 1) {
		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	}
}

void Texture2DArray::bind () {
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
}
