#pragma once
#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define WIN32_NOMINMAX

#undef NOMINMAX
#define NOMINMAX

#include "windows.h"

#undef near
#undef far
#undef min
#undef max

#include "glad/glad.h"
#include "timer.hpp"
#include "string.hpp"
#include "file_io.hpp"
#include "kissmath.hpp"
#include "move_only_class.hpp"
using namespace kiss;

#include "stb_image.hpp"

#include "assert.h"

#include <string>
#include <vector>

struct Source_File {
	std::string			filepath;

	HANDLE		fh;
	FILETIME	last_change_t;

	void init (std::string const& f) {
		filepath = f;
		last_change_t = {}; // zero for debuggability
		open();
	}

	bool open () {
		fh = CreateFile(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fh != INVALID_HANDLE_VALUE) {
			GetFileTime(fh, NULL, NULL, &last_change_t);
		}
		return fh != INVALID_HANDLE_VALUE;
	}

	void close () {
		if (fh != INVALID_HANDLE_VALUE) {
			auto ret = CloseHandle(fh);
			assert(ret != 0);
		}
	}

	bool poll_did_change () {
		if (fh == INVALID_HANDLE_VALUE) return open();

		FILETIME cur_last_change_t;

		GetFileTime(fh, NULL, NULL, &cur_last_change_t);

		auto result = CompareFileTime(&last_change_t, &cur_last_change_t);
		assert(result == 0 || result == -1);

		last_change_t = cur_last_change_t;

		bool did_change = result != 0;
		if (did_change) {
			//Sleep(5); // files often are not completely written when the first change get's noticed, so we might want to wait for a bit
		}
		return did_change;
	}
};

struct Source_Files {
	std::vector<Source_File>	v;

	bool poll_did_change () {
		for (auto& i : v) if (i.poll_did_change()) return true;
		return false;
	}
	void close_all () {
		for (auto& i : v) i.close();
	}
};

typedef uint32_t vert_indx_t;

#define SHADERS_BASE_PATH	"shaders/"
#define TEXTURES_BASE_PATH	"textures"

static void inplace_flip_vertical (void* data, uint64_t h, uint64_t stride) {
	assert((stride % 4) == 0);
	stride /= 4;
	
	uint32_t* line_a =		(uint32_t*)data;
	uint32_t* line_b =		line_a +((h -1) * stride);
	uint32_t* line_a_end =	line_a +((h / 2) * stride);
	
	for (uint32_t j=0; line_a != line_a_end; ++j) {
		
		for (uint32_t i=0; i<stride; ++i) {
			uint32_t tmp = line_a[i];
			line_a[i] = line_b[i];
			line_b[i] = tmp;
		}
		
		line_a += stride;
		line_b -= stride;
	}
}

static float				max_aniso;
static constexpr GLint	MAX_TEXTURE_UNIT = 8; // for debugging only, to unbind textures from unused texture units

enum pixel_type {
	PT_SRGB8_LA8	=0, // srgb rgb and linear alpha
	PT_LRGBA8		,
	PT_SRGB8		,
	PT_LRGB8		,
	PT_LR8			,
	
	PT_LRGBA32F		,
	PT_LRGB32F		,
	
	PT_DXT1			,
	PT_DXT3			,
	PT_DXT5			,
};
enum src_color_space {
	CS_LINEAR		=0,
	CS_SRGB			,
	
	CS_AUTO			,
};

struct Data_Block {
	uint8_t*		data;
	uint64_t			size;

	void free () {
		delete[] data;
	}

	static Data_Block alloc (uint64_t s) {
		return { new uint8_t[s], s };
	}
};

struct Texture {
	std::string					name;
	pixel_type			type;
	
	GLuint				tex;
	
	Data_Block			data;
	
	Texture (std::string const& n) {
		name = n;
		
		glGenTextures(1, &tex);
		
		data.data = nullptr;
	}
	virtual ~Texture () {
		glDeleteTextures(1, &tex);
		
		data.free();
	}
	
	uint32_t get_pixel_size () {
		switch (type) {
			case PT_SRGB8_LA8	:	return 4 * sizeof(uint8_t);
			case PT_LRGBA8		:	return 4 * sizeof(uint8_t);
			case PT_SRGB8		:	return 3 * sizeof(uint8_t);
			case PT_LRGB8		:	return 3 * sizeof(uint8_t);
			case PT_LR8			:	return 1 * sizeof(uint8_t);
			
			case PT_DXT1		:	return 8 * sizeof(uint8_t);
			case PT_DXT3		:	return 16 * sizeof(uint8_t);
			case PT_DXT5		:	return 16 * sizeof(uint8_t);
			
			default: assert(false); return 0;
		}
	}
	
	virtual bool load () = 0;
	virtual bool reload_if_needed () = 0;
	
	virtual void upload () = 0;
	
	virtual void bind () = 0;
};

static void bind_texture_unit (GLint tex_unit, Texture* tex) {
	//assert(tex_unit >= 0 && tex_unit < MAX_TEXTURE_UNIT, "increase MAX_TEXTURE_UNIT (%d, tex_unit: %d)", MAX_TEXTURE_UNIT, tex_unit);
	
	glActiveTexture(GL_TEXTURE0 +tex_unit);
	tex->bind();
}
static void unbind_texture_unit (GLint tex_unit) { // just for debugging
	//assert(tex_unit >= 0 && tex_unit < MAX_TEXTURE_UNIT);
	
	glActiveTexture(GL_TEXTURE0 +tex_unit);
	
	glBindTexture(GL_TEXTURE_2D, 0); // TODO: We dont care if we bound a cubemap to this tex unit?
}

struct Texture2D : public Texture {
	int2					dim;
	
	struct Mip {
		uint8_t*	data;
		uint64_t		size;
		
		int2		dim;
		uint64_t		stride;
	};
	
	std::vector<Mip>	mips;
	
	Texture2D (std::string const& n): Texture{n} {
		glBindTexture(GL_TEXTURE_2D, tex);
	}
	
	void alloc_cpu_single_mip (pixel_type pt, int2 d) {
		type = pt;
		dim = d;
		
		uint64_t stride = (uint64_t)dim.x * get_pixel_size();
		
		data.free();
		data = Data_Block::alloc((uint64_t)dim.y * stride);
		
		mips.resize(1);
		mips[0] = { data.data, data.size, dim, stride };
	}
	
	void upload () {

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
		glBindTexture(GL_TEXTURE_2D, tex);
		
		switch (type) {
			case PT_SRGB8_LA8	:	upload_uncompressed(GL_SRGB8_ALPHA8,	GL_RGBA,	GL_UNSIGNED_BYTE);	break;
			case PT_LRGBA8		:	upload_uncompressed(GL_RGBA8,			GL_RGBA,	GL_UNSIGNED_BYTE);	break;
			case PT_SRGB8		:	upload_uncompressed(GL_SRGB8,			GL_RGB,		GL_UNSIGNED_BYTE);	break;
			case PT_LRGB8		:	upload_uncompressed(GL_RGB8,			GL_RGB,		GL_UNSIGNED_BYTE);	break;
			case PT_LR8			:	upload_uncompressed(GL_R8,				GL_RED,		GL_UNSIGNED_BYTE);	break;
			
			case PT_LRGBA32F	:	upload_uncompressed(GL_RGBA32F,			GL_RGBA,	GL_FLOAT);	break;
			case PT_LRGB32F		:	upload_uncompressed(GL_RGB32F,			GL_RGB,		GL_FLOAT);	break;
			
			//case PT_DXT1		:	upload_compressed(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);		break;
			//case PT_DXT3		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);	break;
			//case PT_DXT5		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);	break;
			
			default: assert(false);
		}
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,		GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,		GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,			GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,			GL_REPEAT);

		// always produces an error
		// maybe just works on sampler objects??
		//if (GLAD_GL_ARB_texture_filter_anisotropic)
		//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,	max_aniso);
		//else if (GLAD_GL_EXT_texture_filter_anisotropic)
		//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,	max_aniso);
	}
	
	virtual void bind () {
		glBindTexture(GL_TEXTURE_2D, tex);
	}
	
	void flip_vertical () {
		assert(type == PT_LR8);
		assert(mips.size() == 1);
		
		auto& m = mips[0];
		inplace_flip_vertical(m.data, m.dim.y, m.dim.x * get_pixel_size());
	}
	
	virtual bool load () { assert(false); return false; }
	virtual bool reload_if_needed () { return false; }
	
private:
	//void upload_compressed (GLenum internalFormat) {
	//	assert((uint32_t)mips.size() >= 1);
	//	
	//	int w=dim.x, h=dim.y;
	//	
	//	uint32_t mip_i;
	//	for (mip_i=0; mip_i<(uint32_t)mips.size();) {
	//		auto& m = mips[mip_i];
	//		
	//		glCompressedTexImage2D(GL_TEXTURE_2D, mip_i, internalFormat, m.dim.x,m.dim.y, 0, m.size, m.data);
	//		
	//		if (++mip_i == (uint32_t)mips.size()) break;
	//		
	//		if (w == 1 && h == 1) break;
	//		if (w > 1) w /= 2;
	//		if (h > 1) h /= 2;
	//	}
	//	
	//	if (mip_i != (uint32_t)mips.size() || w != 1 || h != 1) {
	//		//assert(false, "Not tested");
	//		
	//		glGenerateMipmap(GL_TEXTURE_2D);
	//	}
	//}
	void upload_uncompressed (GLenum internalFormat, GLenum format, GLenum type) {
		assert((uint32_t)mips.size() >= 1);
		
		int w=dim.x, h=dim.y;
		
		uint32_t mip_i;
		for (mip_i=0; mip_i<(uint32_t)mips.size();) {
			auto& m = mips[mip_i];
			
			glTexImage2D(GL_TEXTURE_2D, mip_i, internalFormat, m.dim.x,m.dim.y, 0, format, type, m.data);
			
			if (++mip_i == (uint32_t)mips.size()) break;
			
			if (w == 1 && h == 1) break;
			if (w > 1) w /= 2;
			if (h > 1) h /= 2;
		}
		
		if (mip_i != (uint32_t)mips.size() || w != 1 || h != 1) {
			//assert(mip_i == 1, "%u %u %u %u", mip_i, (uint32_t)mips.size(), w, h);
			
			glGenerateMipmap(GL_TEXTURE_2D);
		}
	}
	
};

struct TextureCube : public Texture {
	int2					dim;
	
	struct Mip {
		uint8_t*	data;
		uint64_t		size;
		
		int2		dim;
		uint64_t		stride;
		uint64_t		face_size;
	};
	
	std::vector<Mip>	mips;
	
	TextureCube (std::string const& n): Texture{n} {
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	}
	
	void alloc_gpu_single_mip (pixel_type pt, int2 d) {
		type = pt;
		dim = d;
		
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
		
		switch (type) {
			case PT_SRGB8_LA8	:	alloc_uncompressed(GL_SRGB8_ALPHA8,	GL_RGBA,	GL_UNSIGNED_BYTE);	break;
			case PT_LRGBA8		:	alloc_uncompressed(GL_RGBA8,		GL_RGBA,	GL_UNSIGNED_BYTE);	break;
			case PT_SRGB8		:	alloc_uncompressed(GL_SRGB8,		GL_RGB,		GL_UNSIGNED_BYTE);	break;
			case PT_LRGB8		:	alloc_uncompressed(GL_RGB8,			GL_RGB,		GL_UNSIGNED_BYTE);	break;
			case PT_LR8			:	alloc_uncompressed(GL_R8,			GL_RED,		GL_UNSIGNED_BYTE);	break;
			
			case PT_LRGBA32F	:	alloc_uncompressed(GL_RGBA32F,		GL_RGBA,	GL_FLOAT);	break;
			case PT_LRGB32F		:	alloc_uncompressed(GL_RGB32F,		GL_RGB,		GL_FLOAT);	break;
			
			//case PT_DXT1		:	alloc_compressed(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);	break;
			//case PT_DXT3		:	alloc_compressed(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);	break;
			//case PT_DXT5		:	alloc_compressed(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);	break;
			
			default: assert(false);
		}
	}
	
	virtual void upload () {
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
		
		switch (type) {
			case PT_SRGB8_LA8	:	upload_uncompressed(GL_SRGB8_ALPHA8,	GL_RGBA,	GL_UNSIGNED_BYTE);	break;
			case PT_LRGBA8		:	upload_uncompressed(GL_RGBA8,			GL_RGBA,	GL_UNSIGNED_BYTE);	break;
			case PT_SRGB8		:	upload_uncompressed(GL_SRGB8,			GL_RGB,		GL_UNSIGNED_BYTE);	break;
			case PT_LRGB8		:	upload_uncompressed(GL_RGB8,			GL_RGB,		GL_UNSIGNED_BYTE);	break;
			case PT_LR8			:	upload_uncompressed(GL_R8,				GL_RED,		GL_UNSIGNED_BYTE);	break;
			
			//case PT_DXT1		:	upload_compressed(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);		break;
			//case PT_DXT3		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);	break;
			//case PT_DXT5		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);	break;
			
			default: assert(false);
		}
		
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
		
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,		GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,		GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,			GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,			GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,			GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY,	max_aniso);
	}
	
	virtual void bind () {
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	}
	
	virtual bool load () { assert(false); return false; }
	virtual bool reload_if_needed () { return false; }
	
private:
	void upload_compressed (GLenum internalFormat) {
		//assert(false, "not implemented");
	}
	void upload_uncompressed (GLenum internalFormat, GLenum format, GLenum type) {
		//assert((uint32_t)mips.size() >= 1);
		
		int w=dim.x, h=dim.y;
		
		uint32_t mip_i;
		for (mip_i=0; mip_i<(uint32_t)mips.size();) {
			auto& m = mips[mip_i];
			
			uint8_t* data_cur = m.data;
			
			for (int face_i=0; face_i<6; ++face_i) {
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X +face_i, mip_i, internalFormat, m.dim.x,m.dim.y, 0, format, type, data_cur);
				
				data_cur += m.face_size;
			}
			
			if (++mip_i == (uint32_t)mips.size()) break;
			
			if (w == 1 && h == 1) break;
			if (w > 1) w /= 2;
			if (h > 1) h /= 2;
		}
		
		if (mip_i != (uint32_t)mips.size() || w != 1 || h != 1) {
			//assert(mip_i == 1, "%u %u %u %u", mip_i, (uint32_t)mips.size(), w, h);
			
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}
	}
	
	void alloc_compressed (GLenum internalFormat) {
		//assert(false, "not implemented");
	}
	void alloc_uncompressed (GLenum internalFormat, GLenum format, GLenum type) {
		for (int face_i=0; face_i<6; ++face_i) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X +face_i, 0, internalFormat, dim.x,dim.y, 0, format, type, NULL);
		}
	}
	
};

// "foo/bar/README.txt"	-> "foo/bar/"
// "README.txt"			-> ""
// ""					-> ""
static std::string get_path_dir (std::string const& path) {
	auto last_slash = path.begin();
	for (auto ch=path.begin(); ch!=path.end(); ++ch) {
		if (*ch == '/') last_slash = ch +1;
	}
	return std::string(path.begin(), last_slash);
}
// "foo/bar/README.txt"	-> return true; ext = "txt"
// "README.txt"			-> return true; ext = "txt"
// "README."			-> return true; ext = ""
// "README"				-> return false;
// ""					-> return false;
// ".ext"				-> return true; ext = "ext";
static bool get_fileext (std::string const& path, std::string* ext) {
	for (auto c = path.end(); c != path.begin();) {
		--c;
		if (*c == '.') {
			*ext = std::string(c +1, path.end());
			return true;
		}
	}
	return false;
}

struct Texture2D_File : public Texture2D {
	Source_File		srcf;
	src_color_space	cs;
	
	Texture2D_File (src_color_space cs_, std::string const& fn): Texture2D{fn}, cs{cs_} {
		auto filepath = prints("%s/%s", TEXTURES_BASE_PATH, name.c_str());
		
		srcf.init(filepath);
	}
	
	virtual ~Texture2D_File () {
		srcf.close();
	}
	
	virtual bool load () {
		
		data.free();
		
		Timer timer;
		if (1) {
			printf("Loading File_Texture2D '%s'...\n", name.c_str());
			timer = Timer::start();
		}
		
		if (!load_texture()) {
			fprintf(stderr,"\"%s\" could not be loaded!\n", name.c_str());
			return false;
		}
		
		if (1) {
			auto dt = timer.end();
			printf(">>> %f ms\n", dt * 1000);
		}
		
		return true;
	}
	
	virtual bool reload_if_needed () {
		bool reloaded = srcf.poll_did_change();
		if (!reloaded) return false;
		
		printf("Texture2D_File source file changed, reloading \"%s\".\n", name.c_str());
		
		reloaded = load();
		if (reloaded) {
			upload();
		}
		return reloaded;
	}
	
private:
	bool load_texture () {
		std::string ext;
		get_fileext(srcf.filepath, &ext);
		
		if (		ext.compare("dds") == 0 ) {
			//return load_dds(srcf.filepath, cs, &type, &dim, &mips, &data);
			assert(false);
			return false;
		} else if (	ext.compare("hdr") == 0 ) {
			return load_img_stb_f32(srcf.filepath, cs, &type, &dim, &mips, &data);
		} else {
			return load_img_stb(srcf.filepath, cs, &type, &dim, &mips, &data);
		}
	}
	
	static bool load_img_stb (std::string const& filepath, src_color_space cs, pixel_type* type, int2* dim, std::vector<Mip>* mips, Data_Block* data) {
		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up
		
		int n;
		data->data = stbi_load(filepath.c_str(), &dim->x, &dim->y, &n, 0);
		if (!data->data) return false;
		
		switch (n) {
			case 4: {
				switch (cs) {
					case CS_LINEAR:				*type = PT_LRGBA8;		break;
					case CS_SRGB: case CS_AUTO:	*type = PT_SRGB8_LA8;	break;
					default: assert(false);
				}
			} break;
			case 3: {
				switch (cs) {
					case CS_LINEAR:				*type = PT_LRGB8;		break;
					case CS_SRGB: case CS_AUTO:	*type = PT_SRGB8;		break;
					default: assert(false);
				}
			} break;
			default: assert(false);
		}
		
		uint64_t stride = dim->x * n;
		data->size = dim->y * stride;
		
		mips->resize(1);
		(*mips)[0] = { data->data, data->size, *dim, stride };
		
		return true;
	}
	static bool load_img_stb_f32 (std::string const& filepath, src_color_space cs, pixel_type* type, int2* dim, std::vector<Mip>* mips, Data_Block* data) {
		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up
		
		int n;
		data->data = (uint8_t*)stbi_loadf(filepath.c_str(), &dim->x, &dim->y, &n, 0);
		if (!data->data) return false;
		
		switch (n) {
			case 4: {
				switch (cs) {
					case CS_LINEAR: case CS_AUTO:	*type = PT_LRGBA32F;	break;
					default: assert(false);
				}
			} break;
			case 3: {
				switch (cs) {
					case CS_LINEAR: case CS_AUTO:	*type = PT_LRGB32F;		break;
					default: assert(false);
				}
			} break;
			default: assert(false);
		}
		
		uint64_t stride = dim->x * n * sizeof(float);
		data->size = dim->y * stride;
		
		mips->resize(1);
		(*mips)[0] = { data->data, data->size, *dim, stride };
		
		return true;
	}
	
};

struct TextureCube_Equirectangular_File : public TextureCube {
	Source_File		srcf;
	src_color_space	cs;
	
	int2				equirect_max_res;
	Texture2D_File*	equirect;
	
	TextureCube_Equirectangular_File (src_color_space cs_, std::string const& fn, int2 equirect_max_res_=4096): TextureCube{fn}, cs{cs_}, equirect_max_res{equirect_max_res_}, equirect{nullptr} {
		auto filepath = prints("%s/%s", TEXTURES_BASE_PATH, name.c_str());
		
		srcf.init(filepath);
	}
	
	virtual ~TextureCube_Equirectangular_File () {
		srcf.close();
		
		delete equirect;
	}
	
	virtual bool load () {
		
		data.free();
		
		Timer timer;
		if (1) {
			printf("Loading File_TextureCube '%s'...", name.c_str());
			timer = Timer::start();
		}
		
		if (!load_texture()) {
			fprintf(stderr,"\"%s\" could not be loaded!", name.c_str());
			return false;
		}
		
		if (1) {
			auto dt = timer.end();
			printf(">>> %f ms", dt * 1000);
		}
		
		return true;
	}
	
	virtual bool reload_if_needed () {
		bool reloaded = srcf.poll_did_change();
		if (!reloaded) return false;
		
		printf("File_TextureCube source file changed, reloading \"%s\".\n", name.c_str());
		
		delete equirect;
		equirect = nullptr;
		
		reloaded = load();
		if (reloaded) {
			upload();
		}
		return reloaded;
	}
	
	virtual void upload () {
		if (!equirect) {
			TextureCube::upload();
		} else {
			type = equirect->type;
			//dim = (int)round_up_to_pot((uint32_t)max(equirect->dim.x, equirect->dim.y) / 4);
			dim = equirect->dim;
			
			equirect->upload();
			
			alloc_gpu_single_mip(type, dim);
			
			gpu_convert_equirectangular_to_cubemap();
			
			glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
			
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}
	}
	
private:
	bool load_texture () {
		std::string ext;
		get_fileext(srcf.filepath, &ext);
		
		if (ext.compare("dds") == 0) {
			//assert(false, "not implemented");
			return false;
		} else {
			// we are loading a cubemap from a equirectangular 2d image
			
			delete equirect;
			equirect = new Texture2D_File(cs, name);
			
			return equirect->load();
		}
	}
	
	void gpu_convert_equirectangular_to_cubemap ();
	
};

struct TextureCube_Multi_File : public TextureCube { // Cubemap with one image file per face
	Source_Files	srcf;
	src_color_space	cs;
	
	TextureCube_Multi_File (src_color_space cs_, std::string const& common_filename, std::string filenames[]): TextureCube{common_filename}, cs{cs_} {
		for (int i=0; i<6; ++i) {
			std::string filepath = prints("%s/%s", TEXTURES_BASE_PATH, filenames[i].c_str());
			
			srcf.v.emplace_back();
			srcf.v.back().init(filepath);
		}
	}
	
	virtual ~TextureCube_Multi_File () {
		srcf.close_all();
	}
	
	virtual bool load () {
		
		data.free();

		Timer timer;
		if (1) {
			printf("Loading Multi_File_TextureCube '%s'...", name.c_str());
			timer = Timer::start();
		}
		
		if (!load_textures()) {
			fprintf(stderr,"\"%s\" could not be loaded!", name.c_str());
			return false;
		}
		
		if (1) {
			auto dt = timer.end();
			printf(">>> %f ms", dt * 1000);
		}
		
		return true;
	}
	
	virtual bool reload_if_needed () {
		bool reloaded = srcf.poll_did_change();
		if (!reloaded) return false;
		
		printf("Multi_File_TextureCube source file changed, reloading \"%s\".\n", name.c_str());
		
		reloaded = load();
		if (reloaded) {
			upload();
		}
		return reloaded;
	}
	
private:
	bool load_textures () {
		for (int i=0; i<6; ++i) {
			std::string ext;
			get_fileext(srcf.v[i].filepath, &ext);
			
			if (ext.compare("dds") == 0) {
				fprintf(stderr,"loading Multi_File_TextureCube with .dds files not supported, please save the cubemap as one .dds file");
				return false;
			}
		}
		return load_cubemap_faces_stb(srcf, cs, &type, &dim, &mips, &data);
	}
	
	static bool load_cubemap_faces_stb (Source_Files const& filespath, src_color_space cs, pixel_type* type, int2* dim, std::vector<Mip>* mips, Data_Block* data) {
		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up
		
		int n;
		
		uint8_t* data_cur;
		
		uint64_t stride;
		uint64_t face_size;
		
		for (int i=0; i<6; ++i) {
			int face_n;
			int2 face_dim;
			
			uint8_t* face_data = stbi_load(filespath.v[i].filepath.c_str(), &face_dim.x, &face_dim.y, &face_n, 0);
			if (!face_data) return false;
			
			if (i == 0) {
				n = face_n;
				*dim = face_dim;
				
				stride = (uint64_t)dim->x * (uint64_t)n;
				face_size = (uint64_t)dim->y * stride;
				
				*data = Data_Block::alloc(6 * face_size);
				data_cur = data->data;
			} else {
				if (face_n != n || any(face_dim != *dim)) return false;
			}
			
			memcpy(data_cur, face_data, face_size);
			data_cur += face_size;
			
			free(face_data);
		}
		
		switch (n) {
			case 4: {
				switch (cs) {
					case CS_LINEAR:				*type = PT_LRGBA8;		break;
					case CS_SRGB: case CS_AUTO:	*type = PT_SRGB8_LA8;	break;
					default: assert(false);
				}
			} break;
			case 3: {
				switch (cs) {
					case CS_LINEAR:				*type = PT_LRGB8;		break;
					case CS_SRGB: case CS_AUTO:	*type = PT_SRGB8;		break;
					default: assert(false);
				}
			} break;
			default: assert(false);
		}
		
		mips->resize(1);
		(*mips)[0] = { data->data, data->size, *dim, stride, face_size };
		
		return true;
	}

};

namespace old {
	enum data_type {
		T_FLT		=0,
		T_V2		,
		T_V3		,
		T_V4		,
	
		T_INT		,
		T_IV2		,
		T_IV3		,
		T_IV4		,
	
		T_U8V4		,
	
		T_M3		,
		T_M4		,
	
		T_BOOL		,
	};

	struct Uniform {
		GLint		loc;
		data_type	type;
		const char*		name;
	
		Uniform (data_type t, const char* n): type{t}, name{n} {}
	
		void set (float v) {
			//assert(type == T_FLT, "%s", name);
			glUniform1f(loc, v);
		}
		void set (float2 v) {
			//assert(type == T_V2, "%s", name);
			glUniform2fv(loc, 1, &v.x);
		}
		void set (float3 v) {
			//assert(type == T_V3, "%s", name);
			glUniform3fv(loc, 1, &v.x);
		}
		void set (float4 v) {
			//assert(type == T_V4, "%s", name);
			glUniform4fv(loc, 1, &v.x);
		}
		void set (int v) {
			//assert(type == T_INT, "%s", name);
			glUniform1i(loc, v);
		}
		void set (int2 v) {
			//assert(type == T_IV2, "%s", name);
			glUniform2iv(loc, 1, &v.x);
		}
		void set (int3 v) {
			//assert(type == T_IV3, "%s", name);
			glUniform3iv(loc, 1, &v.x);
		}
		void set (int4 v) {
			//assert(type == T_IV4, "%s", name);
			glUniform4iv(loc, 1, &v.x);
		}
		void set (float3x3 v) {
			//assert(type == T_M3, "%s", name);
			glUniformMatrix3fv(loc, 1, GL_FALSE, &v.arr[0][0]);
		}
		void set (float4x4 v) {
			//assert(type == T_M4, "%s", name);
			glUniformMatrix4fv(loc, 1, GL_FALSE, &v.arr[0][0]);
		}
		void set (bool b) {
			//assert(type == T_BOOL, "%s", name);
			glUniform1i(loc, (int)b);
		}
	};
}

struct Shader_old {
	std::string								vert_filename;
	std::string								frag_filename;
	
	Source_Files					srcf;
	
	std::string								vert_src;
	std::string								frag_src;
	
	struct Uniform_Texture {
		GLint			tex_unit;
		GLint			loc;
		const char*			name;
		
		Uniform_Texture (GLint unit, const char* n): tex_unit{unit}, name{n} {}
	};
	
	GLuint							prog;
	
	std::vector<old::Uniform>			uniforms;
	std::vector<Uniform_Texture>	textures;
	
	Shader_old (std::string const& v, std::string const& f, std::vector<old::Uniform> const& u, std::vector<Uniform_Texture> const& t):
			vert_filename{v}, frag_filename{f}, prog{0}, uniforms{u}, textures{t} {}
	
	~Shader_old () {
		unload_program();
		srcf.close_all();
	}
	
	bool valid () {
		return prog != 0;
	}
	
	bool load () {
		bool v = load_shader_source(vert_filename, &vert_src);
		bool f = load_shader_source(frag_filename, &frag_src);
		if (!v || !f) return false;
		
		bool res = load_program();
		if (res) {
			get_uniform_locations();
			setup_uniform_textures();
		}
		return res;
	}
	bool reload_if_needed () {
		if (srcf.poll_did_change()) {
			
			printf("shader source changed, reloading shader \"%s\";\"%s\".", vert_filename.c_str(),frag_filename.c_str());
			
			unload_program();
			
			srcf.close_all();
			srcf.v.clear();
			
			return load();
		}
		return false;
	}
	
	void bind () {
		glUseProgram(prog);
	}
	
	old::Uniform* get_uniform (const char* name) {
		for (auto& u : uniforms) {
			if (strcmp(u.name, name) == 0) {
				return &u;
			}
		}
		return nullptr;
	}
	
	template <typename T>
	void set_unif (const char* name, T v) {
		auto* u = get_uniform(name);
		if (u) u->set(v);
		else {
			fprintf(stderr, "Uniform %s is not a uniform in the shader!", name);
		}
	}
	
private:
	bool load_shader_source (std::string const& filename, std::string* src_text) {
		
		{
			std::string filepath = prints("%s%s", SHADERS_BASE_PATH, filename.c_str());
			
			srcf.v.emplace_back(); // add file to list dependent files even before we know if it exist, so that we can find out when it becomes existant
			srcf.v.back().init(filepath);
			
			if (!read_text_file(filepath.c_str(), src_text)) {
				fprintf(stderr,"load_shader_source:: $include \"%s\" could not be loaded!", filename.c_str());
				return false;
			}
		}
		
		for (auto c=src_text->begin(); c!=src_text->end();) {
			
			if (*c == '$') {
				auto line_begin = c;
				++c;
				
				auto syntax_error = [&] () {
					
					while (*c != '\n' && *c != '\r') ++c;
					std::string line (line_begin, c);
					
					fprintf(stderr,"load_shader_source:: expected '$include \"filename\"' syntax but got: '%s'!", line.c_str());
				};
				
				while (*c == ' ' || *c == '\t') ++c;
				
				auto cmd = c;
				
				while ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '_') ++c;
				
				if (std::string(cmd, c).compare("include") == 0) {
					
					while (*c == ' ' || *c == '\t') ++c;
					
					if (*c != '"') {
						syntax_error();
						return false;
					}
					++c;
					
					auto filename_str_begin = c;
					
					while (*c != '"') ++c;
					
					std::string inc_filename(filename_str_begin, c);
					
					if (*c != '"') {
						syntax_error();
						return false;
					}
					++c;
					
					while (*c == ' ' || *c == '\t') ++c;
					
					if (*c != '\r' && *c != '\n') {
						syntax_error();
						return false;
					}
					
					auto line_end = c;
					
					{
						inc_filename = get_path_dir(filename).append(inc_filename);
						
						std::string inc_text;
						if (!load_shader_source(inc_filename, &inc_text)) return false;
						
						auto line_begin_i = line_begin -src_text->begin();
						
						src_text->erase(line_begin, line_end);
						src_text->insert(src_text->begin() +line_begin_i, inc_text.begin(), inc_text.end());
						
						c = src_text->begin() +line_begin_i +inc_text.length();
					}
					
				}
			} else {
				++c;
			}
			
		}
		
		return true;
	}
	
	static bool get_shader_compile_log (GLuint shad, std::string* log) {
		GLsizei log_len;
		{
			GLint temp = 0;
			glGetShaderiv(shad, GL_INFO_LOG_LENGTH, &temp);
			log_len = (GLsizei)temp;
		}
		
		if (log_len <= 1) {
			return false; // no log available
		} else {
			// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in std::string, so we have to allocate one additional char and then resize it away at the end
			
			log->resize(log_len);
			
			GLsizei written_len = 0;
			glGetShaderInfoLog(shad, log_len, &written_len, &(*log)[0]);
			assert(written_len == (log_len -1));
			
			log->resize(written_len);
			
			return true;
		}
	}
	static bool get_program_link_log (GLuint prog, std::string* log) {
		GLsizei log_len;
		{
			GLint temp = 0;
			glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &temp);
			log_len = (GLsizei)temp;
		}
		
		if (log_len <= 1) {
			return false; // no log available
		} else {
			// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in std::string, so we have to allocate one additional char and then resize it away at the end
			
			log->resize(log_len);
			
			GLsizei written_len = 0;
			glGetProgramInfoLog(prog, log_len, &written_len, &(*log)[0]);
			assert(written_len == (log_len -1));
			
			log->resize(written_len);
			
			return true;
		}
	}
	
	static bool load_shader (GLenum type, std::string const& filename, std::string const& source, GLuint* shad) {
		*shad = glCreateShader(type);
		
		{
			const char* ptr = source.c_str();
			glShaderSource(*shad, 1, &ptr, NULL);
		}
		
		glCompileShader(*shad);
		
		bool success;
		{
			GLint status;
			glGetShaderiv(*shad, GL_COMPILE_STATUS, &status);
			
			std::string log_str;
			bool log_avail = get_shader_compile_log(*shad, &log_str);
			
			success = status == GL_TRUE;
			if (!success) {
				// compilation failed
				fprintf(stderr,"OpenGL error in shader compilation \"%s\"!\n>>>\n%s\n<<<\n", filename.c_str(), log_avail ? log_str.c_str() : "<no log available>");
			} else {
				// compilation success
				if (log_avail) {
					fprintf(stderr,"OpenGL shader compilation log \"%s\":\n>>>\n%s\n<<<\n", filename.c_str(), log_str.c_str());
				}
			}
		}
		
		return success;
	}
	bool load_program () {
		
		prog = glCreateProgram();
		
		GLuint vert;
		GLuint frag;
		
		if (	!load_shader(GL_VERTEX_SHADER,		vert_filename, vert_src, &vert) ||
				!load_shader(GL_FRAGMENT_SHADER,	frag_filename, frag_src, &frag)) {
			unload_program();
			prog = 0;
			return false;
		}
		
		glAttachShader(prog, vert);
		glAttachShader(prog, frag);
		
		glLinkProgram(prog);
		
		bool success;
		{
			GLint status;
			glGetProgramiv(prog, GL_LINK_STATUS, &status);
			
			std::string log_str;
			bool log_avail = get_program_link_log(prog, &log_str);
			
			success = status == GL_TRUE;
			if (!success) {
				// linking failed
				fprintf(stderr,"OpenGL error in shader linkage \"%s\"|\"%s\"!\n>>>\n%s\n<<<\n", vert_filename.c_str(), frag_filename.c_str(), log_avail ? log_str.c_str() : "<no log available>");
			} else {
				// linking success
				if (log_avail) {
					fprintf(stderr,"OpenGL shader linkage log \"%s\"|\"%s\":\n>>>\n%s\n<<<\n", vert_filename.c_str(), frag_filename.c_str(), log_str.c_str());
				}
			}
		}
		
		glDetachShader(prog, vert);
		glDetachShader(prog, frag);
		
		glDeleteShader(vert);
		glDeleteShader(frag);
		
		return success;
	}
	void unload_program () {
		glDeleteProgram(prog); // 0 for prog is valid (silently ignored)
	}
	
	void get_uniform_locations () {
		for (auto& u : uniforms) {
			u.loc = glGetUniformLocation(prog, u.name);
			//if (u.loc <= -1) log_warning("Uniform not valid %s!", u.name);
		}
	}
	void setup_uniform_textures () {
		glUseProgram(prog);
		for (auto& t : textures) {
			t.loc = glGetUniformLocation(prog, t.name);
			//if (t.loc <= -1) log_warning("Uniform Texture not valid '%s'!", t.name);
			glUniform1i(t.loc, t.tex_unit);
		}
	}
	
};

#include <array>
#include <string_view>

namespace old {
	struct Vertex_Attribute {
		std::string_view	name; // must be null-terminated
		data_type			type;
		uint64_t			stride;
		uint64_t			offs;

		constexpr Vertex_Attribute (std::string_view name, data_type type, uint64_t stride, uint64_t offs): name{name}, type{type}, stride{stride}, offs{offs} {}
	};

	struct Vertex_Layout {
		Vertex_Attribute const* attribs = nullptr;
		int count = 0;

		Vertex_Layout () {}

		template<int N> Vertex_Layout (std::array<Vertex_Attribute, N> const& arr): attribs{&arr[0]}, count{N} {}

		uint32_t bind_attrib_arrays (Shader_old const* shad) {
			uint32_t vertex_size = 0;

			for (int i=0; i<count; ++i) {
				auto& a = attribs[i];

				GLint comps = 1;
				GLenum type = GL_FLOAT;
				uint32_t size = sizeof(float);

				bool int_format = false;

				switch (a.type) {
					case T_FLT:	comps = 1;	type = GL_FLOAT;	size = sizeof(float);	break;
					case T_V2:	comps = 2;	type = GL_FLOAT;	size = sizeof(float);	break;
					case T_V3:	comps = 3;	type = GL_FLOAT;	size = sizeof(float);	break;
					case T_V4:	comps = 4;	type = GL_FLOAT;	size = sizeof(float);	break;

					case T_INT:	comps = 1;	type = GL_INT;		size = sizeof(int);	break;
					case T_IV2:	comps = 2;	type = GL_INT;		size = sizeof(int);	break;
					case T_IV3:	comps = 3;	type = GL_INT;		size = sizeof(int);	break;
					case T_IV4:	comps = 4;	type = GL_INT;		size = sizeof(int);	break;

					case T_U8V4:	int_format = true;	comps = 4;	type = GL_UNSIGNED_BYTE;		size = sizeof(uint8_t);	break;

					default: assert(false);
				}

				vertex_size += size * comps;

				GLint loc = glGetAttribLocation(shad->prog, a.name.data()); // potentially unsafe to assume std::string_view is null-terminated
				//if (loc <= -1) fprintf(stderr,"Attribute %s is not used in the shader!", a.name);

				if (loc != -1) {
					assert(loc > -1);

					glEnableVertexAttribArray(loc);
					glVertexAttribPointer(loc, comps, type, int_format ? GL_TRUE : GL_FALSE, (GLsizei)a.stride, (void*)a.offs);

				}
			}

			return vertex_size;
		}
	};
}

struct Vbo_old {
	GLuint						vbo_vert;
	GLuint						vbo_indx;
	std::vector<uint8_t>		vertecies;
	std::vector<vert_indx_t>	indices;
	
	old::Vertex_Layout				layout;

	Vbo_old (old::Vertex_Layout layout): layout{layout} {}
	
	bool format_is_indexed () {
		return indices.size() > 0;
	}
	
	void init () {
		glGenBuffers(1, &vbo_vert);
		glGenBuffers(1, &vbo_indx);
		
	}
	~Vbo_old () {
		glDeleteBuffers(1, &vbo_vert);
		glDeleteBuffers(1, &vbo_indx);
	}
	
	void clear () {
		vertecies.clear();
		indices.clear();
	}
	
	void upload () {
		glBindBuffer(GL_ARRAY_BUFFER, vbo_vert);
		glBufferData(GL_ARRAY_BUFFER, vertecies.size() * sizeof(vertecies[0]), NULL, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, vertecies.size() * sizeof(vertecies[0]), vertecies.data(), GL_STATIC_DRAW);
		
		if (format_is_indexed()) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indx);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), NULL, GL_STATIC_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
		}
	}
	
	uint32_t bind (Shader_old const* shad) {
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo_vert);
		
		uint32_t vertex_size = layout.bind_attrib_arrays(shad);
		
		if (format_is_indexed()) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indx);
		} else {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
		
		return vertex_size;
	}
	
	void draw_entire (Shader_old const* shad) {
		uint32_t vertex_size = bind(shad);
		
		if (format_is_indexed()) {
			if (indices.size() > 0)
				glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, NULL);
		} else {
			if (vertecies.size() > 0) {
				//assert(vertecies.size() % vertex_size == 0, "%d %d", (int)vertecies.size(), vertex_size);
				glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertecies.size() / vertex_size);
			}
		}
	}
};

// to add verticies to type-erased std::vector<uint8_t> easier
template <typename T> static typename T* vector_append (std::vector<T>* vec) {
	uintptr_t old_len = vec->size();
	vec->resize( old_len +1 );
	return &*(vec->begin() +old_len);

}
template <typename T> static typename T* vector_append (std::vector<T>* vec, uintptr_t n) {
	uintptr_t old_len = vec->size();
	vec->resize( old_len +n );
	return &*(vec->begin() +old_len);
}

static Shader_old* shad_equirectangular_to_cubemap;

inline void TextureCube_Equirectangular_File::gpu_convert_equirectangular_to_cubemap () {
	
	shad_equirectangular_to_cubemap->bind();
	bind_texture_unit(0, equirect);
	
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	
	glViewport(0,0, dim.x,dim.y);
	
	for (int face_i=0; face_i<6; ++face_i) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X +face_i, tex, 0);
		
		{
			auto ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			assert(ret == GL_FRAMEBUFFER_COMPLETE);
		}
		
		glDrawArrays(GL_TRIANGLES, face_i * 6, 6);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
}
