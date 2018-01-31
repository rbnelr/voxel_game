
namespace dds_n {
	typedef u32 DWORD;
	
	struct DDS_PIXELFORMAT {
		DWORD			dwSize;
		DWORD			dwFlags;
		DWORD			dwFourCC;
		DWORD			dwRGBBitCount;
		DWORD			dwRBitMask;
		DWORD			dwGBitMask;
		DWORD			dwBBitMask;
		DWORD			dwABitMask;
	};
	
	struct DDS_HEADER {
		DWORD			dwSize;
		DWORD			dwFlags;
		DWORD			dwHeight;
		DWORD			dwWidth;
		DWORD			dwPitchOrLinearSize;
		DWORD			dwDepth;
		DWORD			dwMipMapCount;
		DWORD			dwReserved1[11];
		DDS_PIXELFORMAT	ddspf;
		DWORD			dwCaps;
		DWORD			dwCaps2;
		DWORD			dwCaps3;
		DWORD			dwCaps4;
		DWORD			dwReserved2;
	};
	
	static constexpr DWORD DDSD_CAPS			=0x1;
	static constexpr DWORD DDSD_HEIGHT			=0x2;
	static constexpr DWORD DDSD_WIDTH			=0x4;
	static constexpr DWORD DDSD_PITCH			=0x8;
	static constexpr DWORD DDSD_PIXELFORMAT		=0x1000;
	static constexpr DWORD DDSD_MIPMAPCOUNT		=0x20000;
	static constexpr DWORD DDSD_LINEARSIZE		=0x80000;
	static constexpr DWORD DDSD_DEPTH			=0x800000;
	
	static constexpr DWORD DDSCAPS_COMPLEX		=0x8;
	static constexpr DWORD DDSCAPS_MIPMAP		=0x400000;
	static constexpr DWORD DDSCAPS_TEXTURE		=0x1000;
	
	static constexpr DWORD DDPF_ALPHAPIXELS		=0x1;
	static constexpr DWORD DDPF_ALPHA			=0x2;
	static constexpr DWORD DDPF_FOURCC			=0x4;
	static constexpr DWORD DDPF_RGB				=0x40;
	static constexpr DWORD DDPF_YUV				=0x200;
	static constexpr DWORD DDPF_LUMINANCE		=0x20000;

	struct DXT_Block_128 {
		u64	alpha_table; // 4-bit [4][4] table of alpha values
		u16	c0; // RGB 565
		u16	c1; // RGB 565
		u32 col_LUT; // 2-bit [4][4] LUT into c0 - c4
	};
	
	static DXT_Block_128 flip_vertical (DXT_Block_128 b) {
		
		b.alpha_table =
			(((b.alpha_table >>  0) & 0xffff) << 48) |
			(((b.alpha_table >> 16) & 0xffff) << 32) |
			(((b.alpha_table >> 32) & 0xffff) << 16) |
			(((b.alpha_table >> 48) & 0xffff) <<  0);
		
		b.col_LUT =
			(((b.col_LUT >>  0) & 0xff) << 24) |
			(((b.col_LUT >>  8) & 0xff) << 16) |
			(((b.col_LUT >> 16) & 0xff) <<  8) |
			(((b.col_LUT >> 24) & 0xff) <<  0);
		
		return b;
	}

	static void inplace_flip_DXT64_vertical (void* data, u32 w, u32 h) {
		
		DXT_Block_128* line_a =		(DXT_Block_128*)data;
		DXT_Block_128* line_b =		line_a +((h -1) * w);
		DXT_Block_128* line_a_end =	line_a +((h / 2) * w);
		
		for (u32 j=0; line_a != line_a_end; ++j) {
			
			for (u32 i=0; i<w; ++i) {
				DXT_Block_128 tmp = line_a[i];
				line_a[i] = flip_vertical(line_b[i]);
				line_b[i] = flip_vertical(tmp);
			}
			
			line_a += w;
			line_b -= w;
		}
	}
}

static void inplace_flip_vertical (void* data, u64 h, u64 stride) {
	dbg_assert((stride % 4) == 0);
	stride /= 4;
	
	u32* line_a =		(u32*)data;
	u32* line_b =		line_a +((h -1) * stride);
	u32* line_a_end =	line_a +((h / 2) * stride);
	
	for (u32 j=0; line_a != line_a_end; ++j) {
		
		for (u32 i=0; i<stride; ++i) {
			u32 tmp = line_a[i];
			line_a[i] = line_b[i];
			line_b[i] = tmp;
		}
		
		line_a += stride;
		line_b -= stride;
	}
}

static f32				max_aniso;
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

struct Texture {
	pixel_type			type;
	
	GLuint				tex;
	
	Data_Block			data;
	
	Texture () {
		glGenTextures(1, &tex);
		
		data.data = nullptr;
	}
	virtual ~Texture () {
		glDeleteTextures(1, &tex);
		
		data.free();
	}
	
	u32 get_pixel_size () {
		switch (type) {
			case PT_SRGB8_LA8	:	return 4 * sizeof(u8);
			case PT_LRGBA8		:	return 4 * sizeof(u8);
			case PT_SRGB8		:	return 3 * sizeof(u8);
			case PT_LRGB8		:	return 3 * sizeof(u8);
			case PT_LR8			:	return 1 * sizeof(u8);
			
			case PT_DXT1		:	return 8 * sizeof(byte);
			case PT_DXT3		:	return 16 * sizeof(byte);
			case PT_DXT5		:	return 16 * sizeof(byte);
			
			default: dbg_assert(false); return 0;
		}
	}
	
	virtual bool load () = 0;
	virtual bool reload_if_needed () = 0;
	
	virtual void upload () = 0;
	
	virtual void bind () = 0;
};

static void bind_texture_unit (GLint tex_unit, Texture* tex) {
	dbg_assert(tex_unit >= 0 && tex_unit < MAX_TEXTURE_UNIT, "increase MAX_TEXTURE_UNIT (%d, tex_unit: %d)", MAX_TEXTURE_UNIT, tex_unit);
	
	glActiveTexture(GL_TEXTURE0 +tex_unit);
	tex->bind();
}
static void unbind_texture_unit (GLint tex_unit) { // just for debugging
	dbg_assert(tex_unit >= 0 && tex_unit < MAX_TEXTURE_UNIT);
	
	glActiveTexture(GL_TEXTURE0 +tex_unit);
	
	glBindTexture(GL_TEXTURE_2D, 0); // TODO: We dont care if we bound a cubemap to this tex unit?
}

struct Texture2D : public Texture {
	iv2					dim;
	
	struct Mip {
		byte*	data;
		u64		size;
		
		iv2		dim;
		u64		stride;
	};
	
	std::vector<Mip>	mips;
	
	Texture2D (): Texture{} {
		glBindTexture(GL_TEXTURE_2D, tex);
	}
	
	void alloc_cpu_single_mip (pixel_type pt, iv2 d) {
		type = pt;
		dim = d;
		
		u64 stride = (u64)dim.x * get_pixel_size();
		
		data.free();
		data = Data_Block::alloc((u64)dim.y * stride);
		
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
			
			case PT_DXT1		:	upload_compressed(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);		break;
			case PT_DXT3		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);	break;
			case PT_DXT5		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);	break;
			
			default: dbg_assert(false);
		}
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,		GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,		GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,			GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,			GL_REPEAT);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,	max_aniso);
	}
	
	virtual void bind () {
		glBindTexture(GL_TEXTURE_2D, tex);
	}
	
	void flip_vertical () {
		dbg_assert(type == PT_LR8);
		dbg_assert(mips.size() == 1);
		
		auto& m = mips[0];
		inplace_flip_vertical(m.data, m.dim.y, m.dim.x * get_pixel_size());
	}
	
	virtual bool load () { dbg_assert(false); return false; }
	virtual bool reload_if_needed () { return false; }
	
private:
	void upload_compressed (GLenum internalFormat) {
		dbg_assert((u32)mips.size() >= 1);
		
		s32 w=dim.x, h=dim.y;
		
		u32 mip_i;
		for (mip_i=0; mip_i<(u32)mips.size();) {
			auto& m = mips[mip_i];
			
			glCompressedTexImage2D(GL_TEXTURE_2D, mip_i, internalFormat, m.dim.x,m.dim.y, 0, m.size, m.data);
			
			if (++mip_i == (u32)mips.size()) break;
			
			if (w == 1 && h == 1) break;
			if (w > 1) w /= 2;
			if (h > 1) h /= 2;
		}
		
		if (mip_i != (u32)mips.size() || w != 1 || h != 1) {
			dbg_assert(false, "Not tested");
			
			glGenerateMipmap(GL_TEXTURE_2D);
		}
	}
	void upload_uncompressed (GLenum internalFormat, GLenum format, GLenum type) {
		dbg_assert((u32)mips.size() >= 1);
		
		s32 w=dim.x, h=dim.y;
		
		u32 mip_i;
		for (mip_i=0; mip_i<(u32)mips.size();) {
			auto& m = mips[mip_i];
			
			glTexImage2D(GL_TEXTURE_2D, mip_i, internalFormat, m.dim.x,m.dim.y, 0, format, type, m.data);
			
			if (++mip_i == (u32)mips.size()) break;
			
			if (w == 1 && h == 1) break;
			if (w > 1) w /= 2;
			if (h > 1) h /= 2;
		}
		
		if (mip_i != (u32)mips.size() || w != 1 || h != 1) {
			dbg_assert(mip_i == 1, "%u %u %u %u", mip_i, (u32)mips.size(), w, h);
			
			glGenerateMipmap(GL_TEXTURE_2D);
		}
	}
	
};

struct TextureCube : public Texture {
	iv2					dim;
	
	struct Mip {
		byte*	data;
		u64		size;
		
		iv2		dim;
		u64		stride;
		u64		face_size;
	};
	
	std::vector<Mip>	mips;
	
	TextureCube (): Texture{} {
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	}
	
	void alloc_gpu_single_mip (pixel_type pt, iv2 d) {
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
			
			case PT_DXT1		:	alloc_compressed(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);	break;
			case PT_DXT3		:	alloc_compressed(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);	break;
			case PT_DXT5		:	alloc_compressed(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);	break;
			
			default: dbg_assert(false);
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
			
			case PT_DXT1		:	upload_compressed(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);		break;
			case PT_DXT3		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);	break;
			case PT_DXT5		:	upload_compressed(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);	break;
			
			default: dbg_assert(false);
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
	
	virtual bool load () { dbg_assert(false); return false; }
	virtual bool reload_if_needed () { return false; }
	
private:
	void upload_compressed (GLenum internalFormat) {
		dbg_assert(false, "not implemented");
	}
	void upload_uncompressed (GLenum internalFormat, GLenum format, GLenum type) {
		dbg_assert((u32)mips.size() >= 1);
		
		s32 w=dim.x, h=dim.y;
		
		u32 mip_i;
		for (mip_i=0; mip_i<(u32)mips.size();) {
			auto& m = mips[mip_i];
			
			byte* data_cur = m.data;
			
			for (ui face_i=0; face_i<6; ++face_i) {
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X +face_i, mip_i, internalFormat, m.dim.x,m.dim.y, 0, format, type, data_cur);
				
				data_cur += m.face_size;
			}
			
			if (++mip_i == (u32)mips.size()) break;
			
			if (w == 1 && h == 1) break;
			if (w > 1) w /= 2;
			if (h > 1) h /= 2;
		}
		
		if (mip_i != (u32)mips.size() || w != 1 || h != 1) {
			dbg_assert(mip_i == 1, "%u %u %u %u", mip_i, (u32)mips.size(), w, h);
			
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}
	}
	
	void alloc_compressed (GLenum internalFormat) {
		dbg_assert(false, "not implemented");
	}
	void alloc_uncompressed (GLenum internalFormat, GLenum format, GLenum type) {
		for (ui face_i=0; face_i<6; ++face_i) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X +face_i, 0, internalFormat, dim.x,dim.y, 0, format, type, NULL);
		}
	}
	
};

struct Texture2D_File : public Texture2D {
	str				filename;
	
	Source_File		srcf;
	src_color_space	cs;
	
	Texture2D_File (src_color_space cs_, strcr fn): Texture2D{}, filename{fn}, cs{cs_} {
		auto filepath = prints("%s/%s", textures_base_path, filename.c_str());
		
		srcf.init(filepath);
	}
	
	virtual ~Texture2D_File () {
		srcf.close();
	}
	
	virtual bool load () {
		
		data.free();
		
		f64 begin;
		if (1) {
			logf("Loading File_Texture2D '%s'...", filename.c_str());
			begin = glfwGetTime();
		}
		
		if (!load_texture()) {
			logf_warning("\"%s\" could not be loaded!", filename.c_str());
			return false;
		}
		
		if (1) {
			auto dt = glfwGetTime() -begin;
			logf(">>> %f ms", dt * 1000);
		}
		
		return true;
	}
	
	virtual bool reload_if_needed () {
		bool reloaded = srcf.poll_did_change();
		if (!reloaded) return false;
		
		printf("Texture2D_File source file changed, reloading \"%s\".\n", filename.c_str());
		
		reloaded = load();
		if (reloaded) {
			upload();
		}
		return reloaded;
	}
	
private:
	bool load_texture () {
		str ext;
		get_fileext(srcf.filepath, &ext);
		
		if (		ext.compare("dds") == 0 ) {
			return load_dds(srcf.filepath, cs, &type, &dim, &mips, &data);
		} else if (	ext.compare("hdr") == 0 ) {
			return load_img_stb_f32(srcf.filepath, cs, &type, &dim, &mips, &data);
		} else {
			return load_img_stb(srcf.filepath, cs, &type, &dim, &mips, &data);
		}
	}
	
	static bool load_dds (strcr filepath, src_color_space cs, pixel_type* type, iv2* dim, std::vector<Mip>* mips, Data_Block* data) {
		using namespace dds_n;
		
		if (!read_entire_file(filepath.c_str(), data)) return false; // fail
		byte* cur = data->data;
		byte* end = data->data +data->size;
		
		if (	(u64)(end -cur) < 4 ||
				memcmp(cur, "DDS ", 4) != 0 ) return false; // fail
		cur += 4;
		
		if (	(u64)(end -cur) < sizeof(DDS_HEADER) ) return false; // fail
		
		auto* header = (DDS_HEADER*)cur;
		cur += sizeof(DDS_HEADER);
		
		dbg_assert(header->dwSize == sizeof(DDS_HEADER));
		dbg_assert((header->dwFlags & (DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT)) == (DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT), "0x%x", header->dwFlags);
		dbg_assert(header->ddspf.dwSize == sizeof(DDS_PIXELFORMAT));
		
		if (!(header->dwFlags & DDSD_MIPMAPCOUNT) || !(header->dwCaps & DDSCAPS_MIPMAP)) {
			header->dwMipMapCount = 1;
		} else {
			dbg_assert(header->dwFlags & DDSD_MIPMAPCOUNT);
		}
		
		*dim = iv2((s32)header->dwWidth, (s32)header->dwHeight);
		
		mips->resize(header->dwMipMapCount);
		
		if (header->ddspf.dwFlags & DDPF_FOURCC) {
			if (		memcmp(&header->ddspf.dwFourCC, "DXT1", 4) == 0 )	*type = PT_DXT1;
			else if (	memcmp(&header->ddspf.dwFourCC, "DXT3", 4) == 0 )	*type = PT_DXT3;
			else if (	memcmp(&header->ddspf.dwFourCC, "DXT5", 4) == 0 )	*type = PT_DXT5;
			else															return false; // fail
			
			switch (cs) {
				case CS_LINEAR: case CS_AUTO:
					break;
				case CS_SRGB: logf_warning("trying to force contents of compressed .dds to srgb colorspace, not supported, ignoring!");
					break;
				default: dbg_assert(false);
			}
			
			u64 block_size = *type == PT_DXT1 ? 8 : 16;
			
			s32 w=dim->x, h=dim->y;
			
			for (u32 i=0; i<header->dwMipMapCount; ++i) {
				u32 blocks_w = max( (u32)1, ((u32)w +3)/4 );
				u32 blocks_h = max( (u32)1, ((u32)h +3)/4 );
				
				u64 size =	blocks_w * blocks_h * block_size;
				if ((u64)(end -cur) < size) return false; // fail
				
				inplace_flip_DXT64_vertical(cur, blocks_w, blocks_h);
				
				(*mips)[i] = {cur, size, iv2(w,h)};
				
				if (w > 1) w /= 2;
				if (h > 1) h /= 2;
				
				cur += size;
			}
			
		} else {
			dbg_assert(header->ddspf.dwFlags & DDPF_RGB);
			
			switch (header->ddspf.dwRGBBitCount) {
				case 32: {
					dbg_assert(header->ddspf.dwFlags & DDPF_ALPHAPIXELS);
					
					switch (cs) {
						case CS_LINEAR: case CS_AUTO:	*type = PT_LRGBA8;		break;
						case CS_SRGB:					*type = PT_SRGB8_LA8;	break;
						default: dbg_assert(false);
					}
				} break;
				case 24: {
					switch (cs) {
						case CS_LINEAR: case CS_AUTO:	*type = PT_LRGB8;		break;
						case CS_SRGB:					*type = PT_SRGB8;		break;
						default: dbg_assert(false);
					}
				} break;
				
				default: dbg_assert(false);
			}
			
			dbg_assert(header->dwFlags & DDSD_PITCH);
			
			s32 w=dim->x, h=dim->y;
			
			for (u32 i=0; i<header->dwMipMapCount; ++i) {
				
				u64 size =	h * header->dwPitchOrLinearSize;
				if ((u64)(end -cur) < size) return false; // fail
				
				inplace_flip_vertical(cur, h, header->dwPitchOrLinearSize);
				
				(*mips)[i] = {cur, size, iv2(w,h)};
				
				if (w > 1) w /= 2;
				if (h > 1) h /= 2;
				
				cur += size;
			}
		}
		return true;
	}
	static bool load_img_stb (strcr filepath, src_color_space cs, pixel_type* type, iv2* dim, std::vector<Mip>* mips, Data_Block* data) {
		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up
		
		int n;
		data->data = stbi_load(filepath.c_str(), &dim->x, &dim->y, &n, 0);
		if (!data->data) return false;
		
		switch (n) {
			case 4: {
				switch (cs) {
					case CS_LINEAR:				*type = PT_LRGBA8;		break;
					case CS_SRGB: case CS_AUTO:	*type = PT_SRGB8_LA8;	break;
					default: dbg_assert(false);
				}
			} break;
			case 3: {
				switch (cs) {
					case CS_LINEAR:				*type = PT_LRGB8;		break;
					case CS_SRGB: case CS_AUTO:	*type = PT_SRGB8;		break;
					default: dbg_assert(false);
				}
			} break;
			default: dbg_assert(false);
		}
		
		u64 stride = dim->x * n;
		data->size = dim->y * stride;
		
		mips->resize(1);
		(*mips)[0] = { data->data, data->size, *dim, stride };
		
		return true;
	}
	static bool load_img_stb_f32 (strcr filepath, src_color_space cs, pixel_type* type, iv2* dim, std::vector<Mip>* mips, Data_Block* data) {
		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up
		
		int n;
		data->data = (byte*)stbi_loadf(filepath.c_str(), &dim->x, &dim->y, &n, 0);
		if (!data->data) return false;
		
		switch (n) {
			case 4: {
				switch (cs) {
					case CS_LINEAR: case CS_AUTO:	*type = PT_LRGBA32F;	break;
					default: dbg_assert(false);
				}
			} break;
			case 3: {
				switch (cs) {
					case CS_LINEAR: case CS_AUTO:	*type = PT_LRGB32F;		break;
					default: dbg_assert(false);
				}
			} break;
			default: dbg_assert(false);
		}
		
		u64 stride = dim->x * n * sizeof(f32);
		data->size = dim->y * stride;
		
		mips->resize(1);
		(*mips)[0] = { data->data, data->size, *dim, stride };
		
		return true;
	}
	
};

struct TextureCube_Equirectangular_File : public TextureCube {
	str				filename;
	
	Source_File		srcf;
	src_color_space	cs;
	
	iv2				equirect_max_res;
	Texture2D_File*	equirect;
	
	TextureCube_Equirectangular_File (src_color_space cs_, strcr fn, iv2 equirect_max_res_=4096): TextureCube{}, filename{fn}, cs{cs_}, equirect_max_res{equirect_max_res_}, equirect{nullptr} {
		auto filepath = prints("%s/%s", textures_base_path, filename.c_str());
		
		srcf.init(filepath);
	}
	
	virtual ~TextureCube_Equirectangular_File () {
		srcf.close();
		
		delete equirect;
	}
	
	virtual bool load () {
		
		data.free();
		
		f64 begin;
		if (1) {
			logf("Loading File_TextureCube '%s'...", filename.c_str());
			begin = glfwGetTime();
		}
		
		if (!load_texture()) {
			logf_warning("\"%s\" could not be loaded!", filename.c_str());
			return false;
		}
		
		if (1) {
			auto dt = glfwGetTime() -begin;
			logf(">>> %f ms", dt * 1000);
		}
		
		return true;
	}
	
	virtual bool reload_if_needed () {
		bool reloaded = srcf.poll_did_change();
		if (!reloaded) return false;
		
		printf("File_TextureCube source file changed, reloading \"%s\".\n", filename.c_str());
		
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
			dim = (s32)round_up_to_pot((u32)max(equirect->dim.x, equirect->dim.y) / 4);
			
			equirect->upload();
			
			alloc_gpu_single_mip(type, dim);
			
			gpu_convert_equirectangular_to_cubemap();
			
			glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
			
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}
	}
	
private:
	bool load_texture () {
		str ext;
		get_fileext(srcf.filepath, &ext);
		
		if (ext.compare("dds") == 0) {
			dbg_assert(false, "not implemented");
			return false;
		} else {
			// we are loading a cubemap from a equirectangular 2d image
			
			delete equirect;
			equirect = new Texture2D_File(cs, filename);
			
			return equirect->load();
		}
	}
	
	void gpu_convert_equirectangular_to_cubemap ();
	
};

struct TextureCube_Multi_File : public TextureCube { // Cubemap with one image file per face
	str				filename;
	
	Source_Files	srcf;
	src_color_space	cs;
	
	TextureCube_Multi_File (src_color_space cs_, strcr common_filename, std::array<str, 6>const& filenames): TextureCube{}, filename{common_filename}, cs{cs_} {
		for (ui i=0; i<6; ++i) {
			str filepath = prints("%s/%s", textures_base_path, filenames[i].c_str());
			
			srcf.v.emplace_back();
			srcf.v.back().init(filepath);
		}
	}
	
	virtual ~TextureCube_Multi_File () {
		srcf.close_all();
	}
	
	virtual bool load () {
		
		data.free();
		
		f64 begin;
		if (1) {
			logf("Loading Multi_File_TextureCube '%s'...", filename.c_str());
			begin = glfwGetTime();
		}
		
		if (!load_textures()) {
			logf_warning("\"%s\" could not be loaded!", filename.c_str());
			return false;
		}
		
		if (1) {
			auto dt = glfwGetTime() -begin;
			logf(">>> %f ms", dt * 1000);
		}
		
		return true;
	}
	
	virtual bool reload_if_needed () {
		bool reloaded = srcf.poll_did_change();
		if (!reloaded) return false;
		
		printf("Multi_File_TextureCube source file changed, reloading \"%s\".\n", filename.c_str());
		
		reloaded = load();
		if (reloaded) {
			upload();
		}
		return reloaded;
	}
	
private:
	bool load_textures () {
		for (ui i=0; i<6; ++i) {
			str ext;
			get_fileext(srcf.v[i].filepath, &ext);
			
			if (ext.compare("dds") == 0) {
				logf_warning("loading Multi_File_TextureCube with .dds files not supported, please save the cubemap as one .dds file");
				return false;
			}
		}
		return load_cubemap_faces_stb(srcf, cs, &type, &dim, &mips, &data);
	}
	
	static bool load_cubemap_faces_stb (Source_Files const& filespath, src_color_space cs, pixel_type* type, iv2* dim, std::vector<Mip>* mips, Data_Block* data) {
		stbi_set_flip_vertically_on_load(true); // OpenGL has textues bottom-up
		
		int n;
		
		byte* data_cur;
		
		u64 stride;
		u64 face_size;
		
		for (ui i=0; i<6; ++i) {
			int face_n;
			iv2 face_dim;
			
			byte* face_data = stbi_load(filespath.v[i].filepath.c_str(), &face_dim.x, &face_dim.y, &face_n, 0);
			if (!face_data) return false;
			
			if (i == 0) {
				n = face_n;
				*dim = face_dim;
				
				stride = (u64)dim->x * (u64)n;
				face_size = (u64)dim->y * stride;
				
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
					default: dbg_assert(false);
				}
			} break;
			case 3: {
				switch (cs) {
					case CS_LINEAR:				*type = PT_LRGB8;		break;
					case CS_SRGB: case CS_AUTO:	*type = PT_SRGB8;		break;
					default: dbg_assert(false);
				}
			} break;
			default: dbg_assert(false);
		}
		
		mips->resize(1);
		(*mips)[0] = { data->data, data->size, *dim, stride, face_size };
		
		return true;
	}

};

enum data_type {
	T_FLT		=0,
	T_V2		,
	T_V3		,
	T_V4		,
	
	T_INT		,
	T_IV2		,
	T_IV3		,
	T_IV4		,
	
	T_M3		,
	T_M4		,
};

struct Uniform {
	GLint		loc;
	data_type	type;
	cstr		name;
	
	Uniform (data_type t, cstr n): type{t}, name{n} {}
	
	void set (f32 v) {
		dbg_assert(type == T_FLT, "%s", name);
		glUniform1fv(loc, 1, &v);
	}
	void set (v2 v) {
		dbg_assert(type == T_V2, "%s", name);
		glUniform2fv(loc, 1, &v.x);
	}
	void set (v3 v) {
		dbg_assert(type == T_V3, "%s", name);
		glUniform3fv(loc, 1, &v.x);
	}
	void set (v4 v) {
		dbg_assert(type == T_V4, "%s", name);
		glUniform4fv(loc, 1, &v.x);
	}
	void set (s32 v) {
		dbg_assert(type == T_INT, "%s", name);
		glUniform1iv(loc, 1, &v);
	}
	void set (iv2 v) {
		dbg_assert(type == T_IV2, "%s", name);
		glUniform2iv(loc, 1, &v.x);
	}
	void set (iv3 v) {
		dbg_assert(type == T_IV3, "%s", name);
		glUniform3iv(loc, 1, &v.x);
	}
	void set (iv4 v) {
		dbg_assert(type == T_IV4, "%s", name);
		glUniform4iv(loc, 1, &v.x);
	}
	void set (m3 v) {
		dbg_assert(type == T_M3, "%s", name);
		glUniformMatrix3fv(loc, 1, GL_FALSE, &v.arr[0][0]);
	}
	void set (m4 v) {
		dbg_assert(type == T_M4, "%s", name);
		glUniformMatrix4fv(loc, 1, GL_FALSE, &v.arr[0][0]);
	}
};

struct Shader {
	str								vert_filename;
	str								frag_filename;
	
	Source_Files					srcf;
	
	str								vert_src;
	str								frag_src;
	
	struct Uniform_Texture {
		GLint			tex_unit;
		GLint			loc;
		cstr			name;
		
		Uniform_Texture (GLint unit, cstr n): tex_unit{unit}, name{n} {}
	};
	
	GLuint							prog;
	
	std::vector<Uniform>			uniforms;
	std::vector<Uniform_Texture>	textures;
	
	Shader (strcr v, strcr f, std::vector<Uniform> const& u, std::vector<Uniform_Texture> const& t):
			vert_filename{v}, frag_filename{f}, prog{0}, uniforms{u}, textures{t} {}
	
	~Shader () {
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
			
			logf("shader source changed, reloading shader \"%s\";\"%s\".", vert_filename.c_str(),frag_filename.c_str());
			
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
	
	Uniform* get_uniform (cstr name) {
		for (auto& u : uniforms) {
			if (strcmp(u.name, name) == 0) {
				return &u;
			}
		}
		return nullptr;
	}
	
	template <typename T>
	void set_unif (cstr name, T v) {
		auto* u = get_uniform(name);
		if (u) u->set(v);
		else {
			dbg_warning("Uniform %s is not a uniform in the shader!", name);
		}
	}
	
private:
	bool load_shader_source (strcr filename, str* src_text) {
		
		{
			str filepath = prints("%s%s", shaders_base_path, filename.c_str());
			
			srcf.v.emplace_back(); // add file to list dependent files even before we know if it exist, so that we can find out when it becomes existant
			srcf.v.back().init(filepath);
			
			if (!read_text_file(filepath.c_str(), src_text)) {
				logf_warning("load_shader_source:: $include \"%s\" could not be loaded!", filename.c_str());
				return false;
			}
		}
		
		for (auto c=src_text->begin(); c!=src_text->end();) {
			
			if (*c == '$') {
				auto line_begin = c;
				++c;
				
				auto syntax_error = [&] () {
					
					while (*c != '\n' && *c != '\r') ++c;
					str line (line_begin, c);
					
					logf_warning("load_shader_source:: expected '$include \"filename\"' syntax but got: '%s'!", line.c_str());
				};
				
				while (*c == ' ' || *c == '\t') ++c;
				
				auto cmd = c;
				
				while ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '_') ++c;
				
				if (str(cmd, c).compare("include") == 0) {
					
					while (*c == ' ' || *c == '\t') ++c;
					
					if (*c != '"') {
						syntax_error();
						return false;
					}
					++c;
					
					auto filename_str_begin = c;
					
					while (*c != '"') ++c;
					
					str inc_filename(filename_str_begin, c);
					
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
						
						str inc_text;
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
	
	static bool get_shader_compile_log (GLuint shad, str* log) {
		GLsizei log_len;
		{
			GLint temp = 0;
			glGetShaderiv(shad, GL_INFO_LOG_LENGTH, &temp);
			log_len = (GLsizei)temp;
		}
		
		if (log_len <= 1) {
			return false; // no log available
		} else {
			// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in str, so we have to allocate one additional char and then resize it away at the end
			
			log->resize(log_len);
			
			GLsizei written_len = 0;
			glGetShaderInfoLog(shad, log_len, &written_len, &(*log)[0]);
			dbg_assert(written_len == (log_len -1));
			
			log->resize(written_len);
			
			return true;
		}
	}
	static bool get_program_link_log (GLuint prog, str* log) {
		GLsizei log_len;
		{
			GLint temp = 0;
			glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &temp);
			log_len = (GLsizei)temp;
		}
		
		if (log_len <= 1) {
			return false; // no log available
		} else {
			// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in str, so we have to allocate one additional char and then resize it away at the end
			
			log->resize(log_len);
			
			GLsizei written_len = 0;
			glGetProgramInfoLog(prog, log_len, &written_len, &(*log)[0]);
			dbg_assert(written_len == (log_len -1));
			
			log->resize(written_len);
			
			return true;
		}
	}
	
	static bool load_shader (GLenum type, strcr filename, strcr source, GLuint* shad) {
		*shad = glCreateShader(type);
		
		{
			cstr ptr = source.c_str();
			glShaderSource(*shad, 1, &ptr, NULL);
		}
		
		glCompileShader(*shad);
		
		bool success;
		{
			GLint status;
			glGetShaderiv(*shad, GL_COMPILE_STATUS, &status);
			
			str log_str;
			bool log_avail = get_shader_compile_log(*shad, &log_str);
			
			success = status == GL_TRUE;
			if (!success) {
				// compilation failed
				logf_warning("OpenGL error in shader compilation \"%s\"!\n>>>\n%s\n<<<\n", filename.c_str(), log_avail ? log_str.c_str() : "<no log available>");
			} else {
				// compilation success
				if (log_avail) {
					logf_warning("OpenGL shader compilation log \"%s\":\n>>>\n%s\n<<<\n", filename.c_str(), log_str.c_str());
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
			
			str log_str;
			bool log_avail = get_program_link_log(prog, &log_str);
			
			success = status == GL_TRUE;
			if (!success) {
				// linking failed
				logf_warning("OpenGL error in shader linkage \"%s\"|\"%s\"!\n>>>\n%s\n<<<\n", vert_filename.c_str(), frag_filename.c_str(), log_avail ? log_str.c_str() : "<no log available>");
			} else {
				// linking success
				if (log_avail) {
					logf_warning("OpenGL shader linkage log \"%s\"|\"%s\":\n>>>\n%s\n<<<\n", vert_filename.c_str(), frag_filename.c_str(), log_str.c_str());
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

struct Vertex_Layout {
	struct Attribute {
		cstr		name;
		data_type	type;
		u64			stride;
		u64			offs;
	};
	
	std::vector<Attribute>	attribs;
	
	Vertex_Layout (std::initializer_list<Attribute> a): attribs{a} {}
	
	u32 bind_attrib_arrays (Shader const* shad) {
		u32 vertex_size = 0;
		
		for (auto& a : attribs) {
			
			GLint loc = glGetAttribLocation(shad->prog, a.name);
			//if (loc <= -1) logf_warning("Attribute %s is not used in the shader!", a.name);
			
			if (loc != -1) {
				dbg_assert(loc > -1);
				
				glEnableVertexAttribArray(loc);
				
				GLint comps = 1;
				GLenum type = GL_FLOAT;
				u32 size = sizeof(f32);
				switch (a.type) {
					case T_FLT:	comps = 1;	type = GL_FLOAT;	size = sizeof(f32);	break;
					case T_V2:	comps = 2;	type = GL_FLOAT;	size = sizeof(f32);	break;
					case T_V3:	comps = 3;	type = GL_FLOAT;	size = sizeof(f32);	break;
					case T_V4:	comps = 4;	type = GL_FLOAT;	size = sizeof(f32);	break;
					
					case T_INT:	comps = 1;	type = GL_INT;		size = sizeof(s32);	break;
					case T_IV2:	comps = 2;	type = GL_INT;		size = sizeof(s32);	break;
					case T_IV3:	comps = 3;	type = GL_INT;		size = sizeof(s32);	break;
					case T_IV4:	comps = 4;	type = GL_INT;		size = sizeof(s32);	break;
					
					default: dbg_assert(false);
				}
				
				vertex_size += size;
				
				glVertexAttribPointer(loc, comps, type, GL_FALSE, a.stride, (void*)a.offs);
				
			}
		}
		
		return vertex_size;
	}
};

struct Vbo {
	GLuint						vbo_vert;
	GLuint						vbo_indx;
	std::vector<byte>			vertecies;
	std::vector<vert_indx_t>	indices;
	
	Vertex_Layout*		layout;
	
	bool format_is_indexed () {
		return indices.size() > 0;
	}
	
	void init (Vertex_Layout* l) {
		layout = l;
		
		glGenBuffers(1, &vbo_vert);
		glGenBuffers(1, &vbo_indx);
		
	}
	~Vbo () {
		glDeleteBuffers(1, &vbo_vert);
		glDeleteBuffers(1, &vbo_indx);
	}
	
	void clear () {
		vertecies.clear();
		indices.clear();
	}
	
	void upload () {
		glBindBuffer(GL_ARRAY_BUFFER, vbo_vert);
		glBufferData(GL_ARRAY_BUFFER, vector_size_bytes(vertecies), NULL, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, vector_size_bytes(vertecies), vertecies.data(), GL_STATIC_DRAW);
		
		if (format_is_indexed()) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indx);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, vector_size_bytes(indices), NULL, GL_STATIC_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, vector_size_bytes(indices), indices.data(), GL_STATIC_DRAW);
		}
	}
	
	u32 bind (Shader const* shad) {
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo_vert);
		
		u32 vertex_size = layout->bind_attrib_arrays(shad);
		
		if (format_is_indexed()) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indx);
		} else {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
		
		return vertex_size;
	}
	
	void draw_entire (Shader const* shad) {
		u32 vertex_size = bind(shad);
		
		if (format_is_indexed()) {
			if (indices.size() > 0)
				glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, NULL);
		} else {
			if (vertecies.size() > 0) {
				dbg_assert(vertecies.size() % vertex_size == 0);
				glDrawArrays(GL_TRIANGLES, 0, vertecies.size() / vertex_size);
			}
		}
	}
};

static Shader* shad_equirectangular_to_cubemap;

void TextureCube_Equirectangular_File::gpu_convert_equirectangular_to_cubemap () {
	
	shad_equirectangular_to_cubemap->bind();
	bind_texture_unit(0, equirect);
	
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	
	glViewport(0,0, dim.x,dim.y);
	
	for (ui face_i=0; face_i<6; ++face_i) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X +face_i, tex, 0);
		
		{
			auto ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			dbg_assert(ret == GL_FRAMEBUFFER_COMPLETE);
		}
		
		glDrawArrays(GL_TRIANGLES, face_i * 6, 6);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
}
