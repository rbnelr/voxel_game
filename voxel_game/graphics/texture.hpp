#pragma once
#include "gl.hpp"
#include "image.hpp"

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
