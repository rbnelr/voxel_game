#define ARRLEN(arr) (sizeof(arr) / sizeof((arr)[0]))

static int texture_res = 16;

static constexpr int ATLAS_BLOCK_FACES_COUNT =	3;

static constexpr int UVZW_BLOCK_FACE_SIDE =		0;
static constexpr int UVZW_BLOCK_FACE_TOP =		1;
static constexpr int UVZW_BLOCK_FACE_BOTTOM =	2;

static bool unloaded_chunks_traversable = true;

enum block_type : uint8_t {
	BT_AIR				=0,
	BT_WATER			,
	BT_EARTH			,
	BT_GRASS			,
	BT_TREE_LOG			,
	BT_LEAVES			,
	
	BLOCK_TYPES_COUNT	,
	
	BT_OUT_OF_BOUNDS	=BLOCK_TYPES_COUNT,
	BT_NO_CHUNK			,
	
	PSEUDO_BLOCK_TYPES_COUNT,
};

enum transparency_mode : uint32_t {
	TM_OPAQUE		=0,
	TM_TRANSP_MASS	, // only surface of a group of transparent blocks of same type is rendered (like water where only the surface is visible)
	TM_TRANSP_BLOCK	, // all faces of these blocks are rendered (like leaves)
};

struct Block_Properties {
	bool				traversable			: 1;
	transparency_mode	transparency		: 2;
	bool				breakable			: 1;
	bool				replaceable			: 1;
	bool				does_autoheal		: 1;
};

static Block_Properties block_props[PSEUDO_BLOCK_TYPES_COUNT] = {
	/* BT_AIR				*/	{ 1, TM_TRANSP_MASS,	0, 1, 0 },
	/* BT_WATER				*/	{ 1, TM_TRANSP_MASS,	0, 1, 0 },
	/* BT_EARTH				*/	{ 0, TM_OPAQUE,			1, 0, 1 },
	/* BT_GRASS				*/	{ 0, TM_OPAQUE,			1, 0, 1 },
	/* BT_TREE_LOG			*/	{ 0, TM_OPAQUE,			1, 0, 1 },
	/* BT_LEAVES			*/	{ 0, graphics_settings.foliage_alpha ? TM_TRANSP_BLOCK : TM_OPAQUE,	1, 0, 1 },
								
	/* BT_OUT_OF_BOUNDS		*/	{ 1, TM_TRANSP_MASS,	0, 1, 0 },
	/* BT_NO_CHUNK			*/	{ 1, TM_TRANSP_MASS,	0, 1, 0 },
};

static bool bt_is_transparent (block_type t) {	return block_props[t].transparency != TM_OPAQUE; }

struct Block_Texture_Source {
	bool	optional_apha; // for leaves (alpha can be turned on and off (graphics setting), this requires a seperate source texture for alpha, because my image editor destroys the color info on transparent pixels, so i cannot just ignore alpha, when leaves alpha is turned off)
	
	const char*	texture_filename;
	const char*	alpha_texture_filename;
};
static Block_Texture_Source block_texture_sources[BLOCK_TYPES_COUNT] = {
	/* BT_AIR			*/	{ 0, "missing.png"	},
	/* BT_WATER			*/	{ 0, "water.png"	},
	/* BT_EARTH			*/	{ 0, "earth.png"	},
	/* BT_GRASS			*/	{ 0, "grass.png"	},
	/* BT_TREE_LOG		*/	{ 0, "tree_log.png"	},
	/* BT_LEAVES		*/	{ 1, "leaves.png", "leaves.alpha.png"	},
};
static int BLOCK_TEXTURE_INDEX_MISSING = 0;

static int atlas_textures_count = ARRLEN(block_texture_sources);

static int get_block_texture_index_from_block_type (block_type bt) {
	return bt;
}

struct Block {
	block_type	type;
	bool		dark; // any air block that only has air above it (is in sunlight)
	float		hp_ratio;
	srgba8		dbg_tint;
};

static Block B_OUT_OF_BOUNDS = { BT_OUT_OF_BOUNDS, false, 1, 255 };
static Block B_NO_CHUNK = { BT_NO_CHUNK, false, 1, 255 };

#undef BF_BOTTOM
#undef BF_TOP

#define BF_NEG_X		0
#define BF_POS_X		1
#define BF_NEG_Y		2
#define BF_POS_Y		3
#define BF_BOTTOM		4
#define BF_TOP			5

#define BF_NEG_Z		4
#define BF_POS_Z		5

static Texture2D* generate_and_upload_block_texture_atlas () { // texture atlasing
	// combine all textures into a texture atlas
	Texture2D* tex = new Texture2D("block_atlas");
	imgui_showable_textures.push_back(tex);
	
	int2 tex_atlas_res = (texture_res +0) * int2(ATLAS_BLOCK_FACES_COUNT,atlas_textures_count); // +2 for one pixel border
	
	tex->alloc_cpu_single_mip(PT_SRGB8_LA8, tex_atlas_res);
	
	assert(tex->get_pixel_size() == 4);
	uint32_t* src_pixels;
	uint32_t* aplha_src_pixels;
	uint32_t* dst_pixels = (uint32_t*)tex->data.data;
	
	int face_LUT[ATLAS_BLOCK_FACES_COUNT] = {
		/* UVZW_BLOCK_FACE_SIDE		*/	1,
		/* UVZW_BLOCK_FACE_TOP		*/	2,
		/* UVZW_BLOCK_FACE_BOTTOM	*/	0,
	};
	
	auto src = [&] (int x, int y, int face) -> uint32_t* {
		int w = texture_res;
		int h = texture_res;
		return &src_pixels[face_LUT[face]*h*w + y*w + x];
	};
	auto aplha_src = [&] (int x, int y, int face) -> uint32_t* {
		int w = texture_res;
		int h = texture_res;
		return &aplha_src_pixels[face_LUT[face]*h*w + y*w + x];
	};
	auto dst = [&] (int x, int y, int face, int tex_index) -> uint32_t* {
		int w = texture_res +0;
		int h = texture_res +0;
		return &dst_pixels[tex_index*h*ATLAS_BLOCK_FACES_COUNT*w + y*ATLAS_BLOCK_FACES_COUNT*w  + face*w + x];
	};
	
	for (int tex_index=0; tex_index<atlas_textures_count; ++tex_index) {
		
		auto& src_tex = block_texture_sources[tex_index];
		
		Texture2D_File col_tex (CS_AUTO, src_tex.texture_filename);
		Texture2D_File alpha_tex (CS_AUTO, src_tex.alpha_texture_filename ? src_tex.alpha_texture_filename : "");
		
		col_tex.load();
		assert(col_tex.type == PT_SRGB8_LA8);
		assert(all(col_tex.dim == int2(texture_res, texture_res*ATLAS_BLOCK_FACES_COUNT)));
		assert(col_tex.get_pixel_size() == 4);
		
		src_pixels = (uint32_t*)col_tex.data.data;
		
		if (src_tex.optional_apha) {
			alpha_tex.load();
			assert(alpha_tex.type == PT_SRGB8_LA8);
			assert(all(alpha_tex.dim == int2(texture_res, texture_res*ATLAS_BLOCK_FACES_COUNT)));
			assert(alpha_tex.get_pixel_size() == 4);
			
			aplha_src_pixels = (uint32_t*)alpha_tex.data.data;
		}
		
		for (int block_face_i=0; block_face_i<ATLAS_BLOCK_FACES_COUNT; ++block_face_i) {
			
			/*for (int x=0; x<texture_res +2; ++x) { // top border
				*dst(x,0, block_face_i, tex_index) = 0xff0000ff;
			}*/
			
			for (int y=0; y<texture_res; ++y) {
				
				//*dst(0,y, block_face_i, tex_index) = 0xff0000ff;
				
				for (int x=0; x<texture_res; ++x) {
					uint32_t col = *src(x,y, block_face_i);
					
					if (src_tex.optional_apha) {
						uint32_t alpha = *aplha_src(x,y, block_face_i);
						col &= ~0xff000000;
						col |= (alpha & 0xff) << 24;
					}
					
					*dst(x,y, block_face_i, tex_index) = col;
				}
				
				//*dst(texture_res+1,y, block_face_i, tex_index) = 0xff0000ff;
				
			}
			
			
			/*for (int x=0; x<texture_res +2; ++x) {
				*dst(x,texture_res+1, block_face_i, tex_index) = 0xff0000ff;
			}*/
		}
	}
	
	tex->upload();
	return tex;
}
