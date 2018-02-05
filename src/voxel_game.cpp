#include <cstdio>
#include <array>
#include <vector>
#include <string>

#include "types.hpp"
#include "lang_helpers.hpp"
#include "math.hpp"
#include "bit_twiddling.hpp"
#include "vector/vector.hpp"

typedef s32v2	iv2;
typedef s32v3	iv3;
typedef s32v4	iv4;
typedef fv2		v2;
typedef fv3		v3;
typedef fv4		v4;
typedef fm2		m2;
typedef fm3		m3;
typedef fm4		m4;
typedef fhm		hm;

typedef u8v3	lrgb8;
typedef u8v4	lrgba8;

static lrgb8 to_lrgb8 (v3 lrgbf) {
	lrgbf = lrgbf * 255.0f +0.5f;
	return lrgb8((u8)lrgbf.x, (u8)lrgbf.y, (u8)lrgbf.z);
}

struct Interpolator_Key {
	f32	range_begin;
	v3	col;
};
lrgb8 interpolate (f32 val, Interpolator_Key* keys, s32 keys_count) {
	dbg_assert(keys_count >= 1);
	
	s32 i=0;
	for (; i<keys_count; ++i) {
		if (val < keys[i].range_begin) break;
	}
	
	v3 col;
	if (i == 0) { // val is lower than the entire range
		col = keys[0].col;
	} else if (i == keys_count) { // val is higher than the entire range
		col = keys[i -1].col;
	} else {
		
		dbg_assert(keys_count >= 2 && i < keys_count);
		
		auto& a = keys[i -1];
		auto& b = keys[i];
		
		col = lerp(a.col, b.col, map(val, a.range_begin, b.range_begin));
	}
	return to_lrgb8(col);
}

static Interpolator_Key _incandescent_gradient_keys[] = {
	{ 0,		srgb(0)			},
	{ 0.3333f,	srgb(138,0,0)	},
	{ 0.6667f,	srgb(255,255,0)	},
	{ 1,		srgb(255)		},
};
static lrgb8 incandescent_gradient (f32 val) {
	return interpolate(val, _incandescent_gradient_keys, ARRLEN(_incandescent_gradient_keys));
}
static Interpolator_Key _spectrum_gradient_keys[] = {
	{ 0,		srgb(0,0,127)	},
	{ 0.25f,	srgb(0,0,248)	},
	{ 0.5f,		srgb(0,127,0)	},
	{ 0.75f,	srgb(255,255,0)	},
	{ 1,		srgb(255,0,0)	},
};
static lrgb8 spectrum_gradient (f32 val) {
	return interpolate(val, _spectrum_gradient_keys, ARRLEN(_spectrum_gradient_keys));
}

#define _USING_V110_SDK71_ 1
#include "glad.c"
#include "GLFW/glfw3.h"

#include "platform.hpp"

#include "Mmsystem.h"
static void dbg_play_sound () {
	PlaySound(TEXT("recycle.wav"), NULL, SND_FILENAME|SND_ASYNC);
}

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_BMP	1
#define STBI_ONLY_PNG	1
#define STBI_ONLY_TGA	1
//#define STBI_ONLY_JPEG	1
//#define STBI_ONLY_HDR	1

#include "stb_image.h"

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

static void logf (cstr format, ...) {
	std::string str;
	
	va_list vl;
	va_start(vl, format);
	
	_prints(&str, format, vl);
	
	va_end(vl);
	
	str.push_back('\n');
	printf(str.c_str());
}

static void logf_warning (cstr format, ...) {
	std::string str;
	
	va_list vl;
	va_start(vl, format);
	
	_prints(&str, format, vl);
	
	va_end(vl);
	
	printf(ANSI_COLOUR_CODE_YELLOW "%s\n" ANSI_COLOUR_CODE_NC, str.c_str());
}

namespace random {
	
	f32 range (f32 l, f32 h) {
		return (f32)rand() / (f32)RAND_MAX;
	}
	
}

static v2			mouse; // for quick debugging

//
struct Source_File {
	str			filepath;
	
	HANDLE		fh;
	FILETIME	last_change_t;
	
	void init (strcr f) {
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
			dbg_assert(ret != 0);
		}
	}
	
	bool poll_did_change () {
		if (fh == INVALID_HANDLE_VALUE) return open();
		
		FILETIME cur_last_change_t;
		
		GetFileTime(fh, NULL, NULL, &cur_last_change_t);
		
		auto result = CompareFileTime(&last_change_t, &cur_last_change_t);
		dbg_assert(result == 0 || result == -1);
		
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

typedef u32 vert_indx_t;

static cstr shaders_base_path =		"shaders/";
//static cstr meshes_base_path =		"assets";
static cstr textures_base_path =	"assets_src";

static cstr save_files =			"saves/%s.bin";

static bool load_data (strcr name, void* data, uptr size) {
	str file = prints(save_files, name.c_str());
	
	bool loaded = read_entire_file(file.c_str(), data, size);
	if (loaded) {
		logf(prints("%s loaded from \"%s\".", name, file.c_str()).c_str());
	} else {
		logf_warning(prints("%s could not be loaded from \"%s\".", name, file.c_str()).c_str());
	}
	
	return loaded;
};
static bool save_data (strcr name, void const* data, uptr size) {
	str file = prints(save_files, name.c_str());
	
	bool saved = overwrite_file(file.c_str(), data, size);
	if (saved) {
		logf(prints("%s saved to \"%s\".", name, file.c_str()).c_str());
	} else {
		logf_warning(prints("could not write \"%s\", %s wont be loaded on next launch.", file.c_str(), name).c_str());
	}
	
	return saved;
}

template <typename T> static bool load_struct (strcr name, T* data) {
	return load_data(name, data, sizeof(*data));
}
template <typename T> static bool save_struct (strcr name, T const& data) {
	return save_data(name, &data, sizeof(data));
}

#include "gl.hpp"
#include "font.hpp"

static font::Font*	overlay_font;
static f32			overlay_font_line_y;

static Shader*		shad_font;
static Vbo			vbo_overlay_font;

static void begin_overlay_text () {
	vbo_overlay_font.clear();
	
	overlay_font_line_y = overlay_font->ascent_plus_gap;
}
static void overlay_line (strcr s, v3 col=srgb(255,220,120)*0.8f, v3 outline_col=0, s32 cursor_pos=-1) {
	overlay_font->emit_line(&vbo_overlay_font.vertecies, &overlay_font_line_y, shad_font, utf8_to_utf32(s), v4(col,1), v4(outline_col,1), cursor_pos);
}

//
static v3 option_col =			255;
static v3 option_highl_col =	srgb(255,240,90)*0.95f;
static v3 option_edit_col =		srgb(255,255,90)*1.0f;

static enum { OPT_NOT_EDITING=0, OPT_SELECTING, OPT_EDITING } opt_mode = OPT_SELECTING;
static bool opt_value_edit_flag = false;
static bool opt_toggle_open = false;

static s32 selected_option = 0;
static s32 cur_option;

static str opt_val_str;
static s32 opt_cur_char = 0;

static void begin_options () {
	cur_option = 0;
}

static bool parse_s32 (strcr str, s32* val) {
	char* end = nullptr;
	*val = strtol(str.c_str(), &end, 10);
	return end;
}
static bool parse_f32 (strcr str, f32* val) {
	char* end = nullptr;
	*val = strtof(str.c_str(), &end);
	return end;
}
static bool parse_v2 (strcr str, v2* val) {
	using namespace parse_n;
	
	char* cur = (char*)str.c_str();
	
	whitespace(&cur);
	
	val->x = strtof(cur, &cur);
	if (!cur) return false;
	
	whitespace(&cur);
	char_(&cur, ',');
	
	whitespace(&cur);
	
	val->y = strtof(cur, &cur);
	if (!cur) return false;
	
	return true;
}
static bool parse_v3 (strcr str, v3* val) {
	using namespace parse_n;
	
	char* cur = (char*)str.c_str();
	
	whitespace(&cur);
	
	val->x = strtof(cur, &cur);
	if (!cur) return false;
	
	whitespace(&cur);
	char_(&cur, ',');
	
	whitespace(&cur);
	
	val->y = strtof(cur, &cur);
	if (!cur) return false;
	
	whitespace(&cur);
	char_(&cur, ',');
	
	whitespace(&cur);
	
	val->z = strtof(cur, &cur);
	if (!cur) return false;
	
	return true;
}

static bool option_group (strcr name, bool* open=nullptr) {
	bool option_highl = opt_mode && cur_option++ == selected_option;
	bool option_edit = option_highl && opt_mode == OPT_EDITING;
	if (option_highl && opt_toggle_open) {
		opt_toggle_open = false;
		if (open) *open = !(*open);
	}
	
	if (option_edit && opt_value_edit_flag) { // started editing
		opt_value_edit_flag = false;
	}
	
	v3 col = option_col;
	if (option_highl) {
		col = option_edit ? option_edit_col : option_highl_col;
	}
	
	overlay_line(prints("%s:%s", name.c_str(), open && !(*open) ? " {...}":""), col,0, option_edit ? name.size() +2 +opt_cur_char : -1);
	
	if (option_highl && !option_edit && opt_value_edit_flag) { // finished editing
		opt_value_edit_flag = false;
		return true;
	}
	return false;
}

static bool _option (strcr name, str& opt_str, bool* open) {
	bool option_highl = opt_mode && cur_option++ == selected_option;
	bool option_edit = option_highl && opt_mode == OPT_EDITING;
	if (option_highl && opt_toggle_open) {
		opt_toggle_open = false;
		if (open) *open = !(*open);
	}
	
	if (option_edit && opt_value_edit_flag) { // started editing
		opt_value_edit_flag = false;
		opt_val_str = opt_str;
	}
	
	if (option_edit) {
		opt_str = opt_val_str;
	}
	
	v3 col = option_col;
	if (option_highl) {
		col = option_edit ? option_edit_col : option_highl_col;
	}
	
	overlay_line(prints("%s: %s%s", name.c_str(), opt_str.c_str(), open && !(*open) ? " {...}":""), col,0, option_edit ? name.size() +2 +opt_cur_char : -1);
	
	if (option_highl && !option_edit && opt_value_edit_flag) { // finished editing
		opt_value_edit_flag = false;
		return true;
	}
	return false;
}

static bool option (strcr name, bool* val, bool* open=nullptr) {
	str opt_str = prints("%1d", *val ? 1 : 0);
	s32 tmp;
	if (_option(name, opt_str, open))	if (parse_s32(opt_val_str, &tmp)) {
		*val = tmp != 0;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, s32 (*get)(), void (*set)(s32)=nullptr, bool* open=nullptr) {
	str opt_str = prints("%4d", get());
	s32 tmp;
	if (_option(name, opt_str, open))	if (parse_s32(opt_val_str, &tmp) && set) {
		set(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, s32* val, bool* open=nullptr) {
	str opt_str = prints("%4d", *val);
	s32 tmp;
	if (_option(name, opt_str, open))	if (parse_s32(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, f32* val, bool* open=nullptr) {
	str opt_str = prints("%8.7g", *val);
	f32 tmp;
	if (_option(name, opt_str, open))	if (parse_f32(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, v2* val, bool* open=nullptr) {
	str opt_str = prints("%8.7g, %8.7g", val->x,val->y);
	v2 tmp;
	if (_option(name, opt_str, open))	if (parse_v2(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, v3* val, bool* open=nullptr) {
	str opt_str = prints("%8.7g, %8.7g, %8.7g", val->x,val->y,val->z);
	v3 tmp;
	if (_option(name, opt_str, open))	if (parse_v3(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}

static bool option_deg (strcr name, f32* val, bool* open=nullptr) {
	str opt_str = prints("%8.7g", to_deg(*val));
	f32 tmp;
	if (_option(name, opt_str, open))	if (parse_f32(opt_val_str, &tmp)) {
		*val = to_rad(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option_deg (strcr name, v2* val, bool* open=nullptr) {
	str opt_str = prints("%8.7g, %8.7g", to_deg(val->x),to_deg(val->y));
	v2 tmp;
	if (_option(name, opt_str, open))	if (parse_v2(opt_val_str, &tmp)) {
		*val = to_rad(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}

//
#define USI(name)	Uniform(T_INT, name)
#define UIV2(name)	Uniform(T_IV2, name)
#define UV2(name)	Uniform(T_V2, name)
#define UV3(name)	Uniform(T_V3, name)
#define UM4(name)	Uniform(T_M4, name)

static std::vector<Shader*>			shaders;

static Shader* new_shader (strcr v, strcr f, std::initializer_list<Uniform> u, std::initializer_list<Shader::Uniform_Texture> t={}) {
	Shader* s = new Shader(v,f,u,t);
	
	s->load(); // NOTE: Load shaders instantly on creation
	
	shaders.push_back(s);
	return s;
}

//
// TODO: document
struct Mesh_Vertex {
	v3		pos_chunk;
	v4		uvzw_atlas; // xy: [0,1] texture uv;  z: 0=side, 1=top, 2=bottom;  w: texture index
	lrgba8	dbg_tint;
};

static Vertex_Layout mesh_vert_layout = {
	{ "pos_chunk",	T_V3,	sizeof(Mesh_Vertex), offsetof(Mesh_Vertex, pos_chunk) },
	{ "uvzw_atlas",	T_V4,	sizeof(Mesh_Vertex), offsetof(Mesh_Vertex, uvzw_atlas) },
	{ "dbg_tint",	T_U8V4,	sizeof(Mesh_Vertex), offsetof(Mesh_Vertex, dbg_tint) },
};

static s32 texture_res = 16;

static constexpr s32 ATLAS_BLOCK_FACES_COUNT = 3;

static constexpr s32 UVZW_BLOCK_FACE_SIDE =		0;
static constexpr s32 UVZW_BLOCK_FACE_TOP =		1;
static constexpr s32 UVZW_BLOCK_FACE_BOTTOM =	2;

enum block_type : s32 {
	BT_AIR		=0,
	BT_EARTH	,
	BT_GRASS	,
	
	BT_OUT_OF_BOUNDS	=0xfe,
	BT_NO_CHUNK			=0xff,
	
	BLOCK_TYPES_COUNT
};
static bool bt_is_pervious (block_type t) { return t == BT_AIR || t == BT_OUT_OF_BOUNDS || t == BT_NO_CHUNK; }

static cstr block_texture_name[BLOCK_TYPES_COUNT] = {
	/* BT_AIR	*/	"missing.png",
	/* BT_EARTH	*/	"earth.png",
	/* BT_GRASS	*/	"grass.png",
};
static s32 BLOCK_TEXTURE_INDEX_MISSING = 0;

static s32 atlas_textures_count = 3;

static s32 get_block_texture_index_from_block_type (block_type bt) {
	return bt;
}

static constexpr f32 BLOCK_FULL_HP = 100;

struct Block {
	block_type	type;
	f32			hp;
	lrgba8		dbg_tint;
};

static constexpr Block B_OUT_OF_BOUNDS = { BT_OUT_OF_BOUNDS, 0, 255 };
static constexpr Block B_NO_CHUNK = { BT_NO_CHUNK, 0, 255 };

#include "noise.hpp"

typedef s64		bpos_t;
typedef s64v2	bpos2;
typedef s64v3	bpos;

typedef s64v2	chunk_pos_t;

struct s64v2_hashmap {
	chunk_pos_t v;
	
	NOINLINE bool operator== (s64v2_hashmap const& r) const { // for hash map
		return v.x == r.v.x && v.y == r.v.y;
	}
};

static_assert(sizeof(size_t) == 8, "");

namespace std {
	template<> struct hash<s64v2_hashmap> { // for hash map
		NOINLINE size_t operator() (s64v2_hashmap const& v) const {
			return 53 * (hash<s64>()(v.v.x) + 53) + hash<s64>()(v.v.y);
		}
	};
}

static constexpr bpos CHUNK_DIM = bpos(32,32,64);

struct Chunk {
	chunk_pos_t pos;
	
	bool		vbo_needs_update = false;
	
	Block	data[CHUNK_DIM.z][CHUNK_DIM.y][CHUNK_DIM.x];
	
	Vbo		vbo;
	
	void init () {
		
	}
	void init_gl () {
		vbo.init(&mesh_vert_layout);
	}
	
	Block* get_block (bpos pos) {
		return &data[pos.z][pos.y][pos.x];
	}
	
	void generate_blocks_mesh () {
		if (!vbo_needs_update) return;
		vbo_needs_update = false;
		
		bpos chunk_origin = bpos(pos * CHUNK_DIM.xy(), 0);
		
		vbo.vertecies.clear();
		
		auto cube = [&] (bpos const& pos_world, bpos const& pos_chunk, Block const* b) {
			
			f32 XL = (f32)pos_world.x;
			f32 YL = (f32)pos_world.y;
			f32 ZL = (f32)pos_world.z;
			f32 XH = (f32)(pos_world.x +1);
			f32 YH = (f32)(pos_world.y +1);
			f32 ZH = (f32)(pos_world.z +1);
			
			f32 w = get_block_texture_index_from_block_type(b->type);
			
			if (pos_chunk.x == CHUNK_DIM.x-1	|| get_block(pos_chunk +bpos(+1,0,0))->type == BT_AIR) {
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Mesh_Vertex)*6);
				
				*out++ = { v3(XH,YH,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YL,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
			}
			if (pos_chunk.x == 0				|| get_block(pos_chunk +bpos(-1,0,0))->type == BT_AIR) {
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Mesh_Vertex)*6);
				
				*out++ = { v3(XL,YL,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
			}
			if (pos_chunk.y == CHUNK_DIM.y-1	|| get_block(pos_chunk +bpos(0,+1,0))->type == BT_AIR) {
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Mesh_Vertex)*6);
				
				*out++ = { v3(XL,YH,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
			}
			if (pos_chunk.y == 0				|| get_block(pos_chunk +bpos(0,-1,0))->type == BT_AIR) {
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Mesh_Vertex)*6);
				
				*out++ = { v3(XH,YL,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XH,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b->dbg_tint };
			}
			if (pos_chunk.z == CHUNK_DIM.z-1	|| get_block(pos_chunk +bpos(0,0,+1))->type == BT_AIR) {
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Mesh_Vertex)*6);
				
				*out++ = { v3(XH,YL,ZH), v4(1,0, UVZW_BLOCK_FACE_TOP,w), b->dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_TOP,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(0,0, UVZW_BLOCK_FACE_TOP,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(0,0, UVZW_BLOCK_FACE_TOP,w), b->dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_TOP,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(0,1, UVZW_BLOCK_FACE_TOP,w), b->dbg_tint };
			}
			if (pos_chunk.z == 0				|| get_block(pos_chunk +bpos(0,0,-1))->type == BT_AIR) {
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Mesh_Vertex)*6);
				
				*out++ = { v3(XH,YH,ZL), v4(1,0, UVZW_BLOCK_FACE_BOTTOM,w), b->dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(1,1, UVZW_BLOCK_FACE_BOTTOM,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_BOTTOM,w), b->dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_BOTTOM,w), b->dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(1,1, UVZW_BLOCK_FACE_BOTTOM,w), b->dbg_tint };
				*out++ = { v3(XL,YL,ZL), v4(0,1, UVZW_BLOCK_FACE_BOTTOM,w), b->dbg_tint };
			}
		};
		
		{
			bpos i;
			for (i.z=0; i.z<CHUNK_DIM.z; ++i.z) {
				for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
					for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
						auto* block = get_block(i);
						
						if (block->type != BT_AIR) {
							
							cube(i +chunk_origin, i, block);
							
						}
					}
				}
			}
		}
		
		vbo.upload();
	}
	
};

static chunk_pos_t int_div_by_pot_floor (bpos pos_world, bpos* bpos_in_chunk) {
	
	chunk_pos_t chunk_pos;
	chunk_pos.x = pos_world.x >> GET_CONST_POT(CHUNK_DIM.x); // arithmetic shift right instead of divide because we want  -10 / 32  to be  -1 instead of 0
	chunk_pos.y = pos_world.y >> GET_CONST_POT(CHUNK_DIM.y);
	
	*bpos_in_chunk = pos_world;
	bpos_in_chunk->x = pos_world.x & ((1 << GET_CONST_POT(CHUNK_DIM.x)) -1);
	bpos_in_chunk->y = pos_world.y & ((1 << GET_CONST_POT(CHUNK_DIM.y)) -1);
	
	return chunk_pos;
}

#include <unordered_map>
std::unordered_map<s64v2_hashmap, Chunk*> chunks;

static Chunk* _prev_query_block_chunk = nullptr; // to avoid hash map lookup most of the time, since most query_block's are going to end up in the same chunk

static Block* query_block (bpos p) {
	if (p.z < 0 || p.z >= CHUNK_DIM.z) return (Block*)&B_OUT_OF_BOUNDS;
	
	bpos rel_p;
	chunk_pos_t chunk_p = int_div_by_pot_floor(p, &rel_p);
	
	Chunk* chunk;
	if (_prev_query_block_chunk && equal(_prev_query_block_chunk->pos, chunk_p)) {
		chunk = _prev_query_block_chunk;
	} else {
		
		auto k = chunks.find({chunk_p});
		if (k == chunks.end()) return (Block*)&B_NO_CHUNK;
		
		chunk = k->second;
		
		_prev_query_block_chunk = chunk;
	}
	return chunk->get_block(rel_p);
}

struct Perlin_Octave {
	f32	freq;
	f32	amp;
};
std::vector<Perlin_Octave> heightmap_perlin2d_octaves = {
	{ 0.03f,	30		},
	{ 0.1f,		10		},
	{ 0.4f,		3		},
	{ 2.0f,		0.1f	},
};
static s32 get_heightmap_perlin2d_octaves_count () {			return (s32)heightmap_perlin2d_octaves.size(); }
static void set_heightmap_perlin2d_octaves_count (s32 count) {	heightmap_perlin2d_octaves.resize(count); }
static bool heightmap_perlin2d_octaves_open = true;

#define _2D_TEST 1

f32 heightmap_perlin2d (v2 v) {
	using namespace perlin_noise_n;
	#if _2D_TEST
	return perlin_octave(v, 0.2f) > 0 ? 2 : 1;
	#else
	//v += v2(1,-1)*mouse * 20;
	
	//f32 fre = lerp(0.33f, 3.0f, mouse.x);
	//f32 amp = lerp(0.33f, 3.0f, mouse.y);
	
	f32 tot = 0;
	
	for (auto& o : heightmap_perlin2d_octaves) {
		tot += perlin_octave(v, o.freq) * o.amp;
	}
	
	tot *= 1.5f;
	
	//printf(">>>>>>>>> %.3f %.3f\n", fre, amp);
	
	return tot +32;
	#endif
}

void gen_chunk (Chunk* chunk) {
	bpos i; // position in chunk
	for (i.z=0; i.z<CHUNK_DIM.z; ++i.z) {
		for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
				auto* b = chunk->get_block(i);
				
				b->type = BT_AIR;
				b->hp = BLOCK_FULL_HP;
				b->dbg_tint = 255;
				
				//if (i.z == CHUNK_DIM.z-1) b->type = BT_EARTH;
			}
		}
	}
	
	for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
		for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
			
			f32 height = heightmap_perlin2d((v2)i.xy() +chunk->pos*CHUNK_DIM.xy());
			s32 highest_block = (s32)floor(height -1 +0.5f); // -1 because height 1 means the highest block is z=0
			
			for (i.z=0; i.z <= min(highest_block, (s32)CHUNK_DIM.z-1); ++i.z) {
				auto* b = chunk->get_block(i);
				if (i.z != highest_block) {
					b->type = BT_EARTH;
				} else {
					b->type = BT_GRASS;
				}
				#if _2D_TEST
				b->dbg_tint = lrgba8(spectrum_gradient(map(height +0.5f, 0, 3)), 255);
				#else
				b->dbg_tint = lrgba8(spectrum_gradient(map(height +0.5f, 0, 50)), 255);
				#endif
			}
		}
	}
	
	chunk->vbo_needs_update = true;
}

Chunk* new_chunk (v3 cam_pos_world) {
	dbg_assert(chunks.size() > 0);
	
	Chunk* c = new Chunk;
	c->init();
	c->init_gl();
	
	f32			nearest_free_spot_dist = +INF;
	chunk_pos_t	nearest_free_spot;
	
	for (auto& hash_pair : chunks) {
		auto& c = hash_pair.second;
		
		auto check_chunk_spot = [&] (chunk_pos_t pos) {
			if (chunks.find({pos}) != chunks.end()) return;
			// free spot found
			v2 chunk_center = (v2)(pos * CHUNK_DIM.xy()) +(v2)CHUNK_DIM.xy() / 2;
			
			f32 dist = length(cam_pos_world.xy() - chunk_center);
			if (dist < nearest_free_spot_dist
					&& all(pos >= 0)
					) {
				nearest_free_spot_dist = dist;
				nearest_free_spot = pos;
			}
		};
		
		check_chunk_spot(c->pos +chunk_pos_t(+1, 0));
		check_chunk_spot(c->pos +chunk_pos_t( 0,+1));
		check_chunk_spot(c->pos +chunk_pos_t(-1, 0));
		check_chunk_spot(c->pos +chunk_pos_t( 0,-1));
		
	}
	
	dbg_assert(nearest_free_spot_dist != +INF);
	
	c->pos = nearest_free_spot;
	
	chunks.insert({{c->pos}, c});
	gen_chunk(c);
	return c;
}
Chunk* inital_chunk () {
	Chunk* c = new Chunk;
	c->init();
	c->init_gl();
	
	c->pos = 0;
	
	chunks.insert({{c->pos}, c});
	gen_chunk(c);
	return c;
}

bool test_cylinder_collision (v3 pos_world, f32 r, f32 h) {
	
	bool any_colliding = false;
	
	bpos start =	(bpos)floor(pos_world -v3(r,r,0));
	bpos end =		(bpos)ceil(pos_world +v3(r,r,h));
	
	bpos i;
	for (i.z=start.z; i.z<end.z; ++i.z) {
		for (i.y=start.y; i.y<end.y; ++i.y) {
			for (i.x=start.x; i.x<end.x; ++i.x) {
				
				v2 p = pos_world.xy() -(v2)i.xy(); // circle position relative to block
				
				v2 closest_p = clamp(p, 0,1); // closest point on block
				
				bool could_collide = length_sqr(closest_p -p) <= r*r;
				
				if (!could_collide) continue;
				
				auto* b = query_block(i);
				bool colliding = !bt_is_pervious(b->type);
				
				any_colliding = any_colliding || colliding;
			}
		}
	}
	return any_colliding;
}
bool test_cylinder_collision (v3 pos_world, f32 r, f32 h, Vbo* vbo) {
	
	bool any_colliding = false;
	
	bpos start =	(bpos)floor(pos_world -v3(r,r,0));
	bpos end =		(bpos)ceil(pos_world +v3(r,r,h));
	
	f32 w = get_block_texture_index_from_block_type(BT_EARTH);
	
	bpos i;
	for (i.z=start.z; i.z<end.z; ++i.z) {
		for (i.y=start.y; i.y<end.y; ++i.y) {
			for (i.x=start.x; i.x<end.x; ++i.x) {
				
				v2 p = pos_world.xy() -(v2)i.xy(); // circle position relative to block
				
				v2 closest_p = clamp(p, 0,1); // closest point on block
				
				bool could_collide = length_sqr(closest_p -p) <= r*r;
				
				lrgba8 col = 255;
				
				if (!could_collide) {
					col = lrgba8(40,40,40,255);
				} else {
					
					auto* b = query_block(i);
					bool colliding = !bt_is_pervious(b->type);
					
					if (colliding) col = lrgba8(255,0,0,255);
					
					any_colliding = any_colliding || colliding;
				}
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&vbo->vertecies, sizeof(Mesh_Vertex)*6);
				
				*out++ = { (v3)i +v3(+1, 0,+1.01f), v4(1,0, UVZW_BLOCK_FACE_TOP,w), col };
				*out++ = { (v3)i +v3(+1,+1,+1.01f), v4(1,1, UVZW_BLOCK_FACE_TOP,w), col };
				*out++ = { (v3)i +v3( 0, 0,+1.01f), v4(0,0, UVZW_BLOCK_FACE_TOP,w), col };
				*out++ = { (v3)i +v3( 0, 0,+1.01f), v4(0,0, UVZW_BLOCK_FACE_TOP,w), col };
				*out++ = { (v3)i +v3(+1,+1,+1.01f), v4(1,1, UVZW_BLOCK_FACE_TOP,w), col };
				*out++ = { (v3)i +v3( 0,+1,+1.01f), v4(0,1, UVZW_BLOCK_FACE_TOP,w), col };
			}
		}
	}
	return any_colliding;
}

//
static f32			dt;

struct Input {
	iv2		wnd_dim;
	v2		wnd_dim_aspect;
	
	iv2		mcursor_pos_px;
	
	//
	v2		mouse_look_diff;
	
	iv3		move_dir =			0;
	bool	move_fast =			false;
	
	void get_non_callback_input () {
		{
			glfwGetFramebufferSize(wnd, &wnd_dim.x, &wnd_dim.y);
			
			v2 tmp = (v2)wnd_dim;
			wnd_dim_aspect = tmp / v2(tmp.y, tmp.x);
		}
		{
			f64 x, y;
			glfwGetCursorPos(wnd, &x, &y);
			mcursor_pos_px = iv2((int)x, (int)y);
			
			mouse = v2(x,y) / (v2)wnd_dim;
		}
	}
	
	v2 bottom_up_mcursor_pos () {
		return v2(mcursor_pos_px.x, wnd_dim.y -mcursor_pos_px.y);
	}
};
static Input		inp;

static v3 GRAV_ACCEL = v3(0,0,-10);

static bool viewing_flycam =		false;

struct Flycam {
	v3	pos_world =			v3(0, -5, 1);
	v2	ori_ae =			v2(deg(0), deg(+80)); // azimuth elevation
	
	f32	vfov =				deg(70);
	
	f32	speed =				4;
	f32	speed_fast_mul =	4;
	
	
	bool opt_open = true;
	void options () {
		option_group("flycam", &opt_open);
		if (opt_open) {
			option(		"  pos_world",		&pos_world);
			option_deg(	"  ori_ae",			&ori_ae);
			option_deg(	"  vfov",			&vfov);
			option(		"  speed",			&speed);
			option(		"  speed_fast_mul",	&speed_fast_mul);
		}
	}
};
static Flycam flycam;

static bool trigger_respawn_player =	true;
static bool trigger_regen_chunks =		false;
static bool trigger_save_game =			false;
static bool trigger_load_game =			false;

static void glfw_key_event (GLFWwindow* window, int key, int scancode, int action, int mods) {
	dbg_assert(action == GLFW_PRESS || action == GLFW_RELEASE || action == GLFW_REPEAT);
	
	bool went_down =	action == GLFW_PRESS;
	bool went_up =		action == GLFW_RELEASE;
	
	bool repeated =		!went_down && !went_up; // GLFW_REPEAT
	
	bool alt =			(mods & GLFW_MOD_ALT) != 0;
	
	if (alt) {
		switch (key) {
			case GLFW_KEY_ENTER:		if (went_down) toggle_fullscreen();	return;
		}
	} else {
		switch (key) {
			case GLFW_KEY_F11:			if (went_down) toggle_fullscreen();	return;
		}
	}
	
	if (opt_mode == OPT_SELECTING) { 
		switch (key) {
			case GLFW_KEY_F1:			if (went_down)	opt_mode = OPT_NOT_EDITING;		return;
			case GLFW_KEY_ENTER:		if (went_down) {
					opt_value_edit_flag = true;
					opt_mode = OPT_EDITING;
				}
				return;
			
			case GLFW_KEY_UP:			if (went_down || repeated)	selected_option = max(selected_option -1, 0);				return;
			case GLFW_KEY_DOWN:			if (went_down || repeated)	selected_option = min(selected_option +1, cur_option);		return;
			
			case GLFW_KEY_E:			if (went_down)	opt_toggle_open = true;		return;
			
		}
	} else if (opt_mode == OPT_EDITING) { 
		switch (key) {
			case GLFW_KEY_F1:			if (went_down)	opt_mode = OPT_NOT_EDITING;		return;
			case GLFW_KEY_ENTER:		if (went_down) {
					opt_value_edit_flag = true;
					opt_mode = OPT_SELECTING;
				}
				return;
			case GLFW_KEY_ESCAPE:		if (went_down) {
					opt_mode = OPT_SELECTING;
				}
				return;
			
			case GLFW_KEY_LEFT:			if (went_down || repeated)	opt_cur_char = max(opt_cur_char -1, 0);							return;
			case GLFW_KEY_RIGHT:		if (went_down || repeated)	opt_cur_char = min(opt_cur_char +1, (s32)opt_val_str.size());	return;
			
			case GLFW_KEY_BACKSPACE:	if (went_down || repeated) {
					if (opt_cur_char > 0) {
						opt_val_str.erase(opt_val_str.begin() +opt_cur_char -1);
						--opt_cur_char;
					}
				}
				return;
			case GLFW_KEY_DELETE:		if (went_down || repeated) {
					if (opt_cur_char < (s32)opt_val_str.size()) {
						opt_val_str.erase(opt_val_str.begin() +opt_cur_char);
					}
				}
				return;
		}
		return; // do not process input when editing options
	}
	
	if (!repeated) {
		if (alt) {
			switch (key) {
				//
				case GLFW_KEY_S:			if (went_down) trigger_save_game = true;	break;
				case GLFW_KEY_L:			if (went_down) trigger_load_game = true;	break;
			}
		} else {
			switch (key) {
				//
				case GLFW_KEY_A:			inp.move_dir.x -= went_down ? +1 : -1;		break;
				case GLFW_KEY_D:			inp.move_dir.x += went_down ? +1 : -1;		break;
				
				case GLFW_KEY_S:			inp.move_dir.z += went_down ? +1 : -1;		break;
				case GLFW_KEY_W:			inp.move_dir.z -= went_down ? +1 : -1;		break;
				
				case GLFW_KEY_LEFT_CONTROL:	inp.move_dir.y -= went_down ? +1 : -1;		break;
				case GLFW_KEY_SPACE:		inp.move_dir.y += went_down ? +1 : -1;		break;
				
				case GLFW_KEY_LEFT_SHIFT:	inp.move_fast = went_down;					break;
				
				case GLFW_KEY_R:			if (went_down) trigger_regen_chunks = true;		break;
				case GLFW_KEY_Q:			if (went_down) trigger_respawn_player = true;	break;
				
				case GLFW_KEY_F1:			if (went_down) opt_mode = OPT_SELECTING;	break;
			}
		}
	}
}
static void glfw_char_event (GLFWwindow* window, unsigned int codepoint, int mods) {
	if (opt_mode == OPT_EDITING) {
		opt_val_str.insert(opt_val_str.begin() +opt_cur_char++, (char)codepoint);
	}
}
static void glfw_mouse_button_event (GLFWwindow* window, int button, int action, int mods) {
    switch (button) {
		case GLFW_MOUSE_BUTTON_RIGHT:
			if (action == GLFW_PRESS) {
				start_mouse_look();
			} else {
				stop_mouse_look();
			}
			break;
	}
}
static void glfw_mouse_scroll (GLFWwindow* window, double xoffset, double yoffset) {
	if (viewing_flycam) {
		if (!inp.move_fast) {
			f32 delta_log = 0.1f * (f32)yoffset;
			flycam.speed = pow( 2, log2(flycam.speed) +delta_log );
			logf(">>> fly_vel: %f", flycam.speed);
		} else {
			f32 delta_log = -0.1f * (f32)yoffset;
			f32 vfov = pow( 2, log2(flycam.vfov) +delta_log );
			if (vfov >= deg(1.0f/10) && vfov <= deg(170)) flycam.vfov = vfov;
		}
	}
}
static void glfw_cursor_move_relative (GLFWwindow* window, double dx, double dy) {
	v2 diff = v2((f32)dx,(f32)dy);
	inp.mouse_look_diff += diff;
}

int main (int argc, char** argv) {
	cstr app_name = "Voxel Game";
	
	platform_setup_context_and_open_window(app_name, iv2(1280, 720));
	
	//
	set_vsync(1);
	
	{ // GL state
		glEnable(GL_FRAMEBUFFER_SRGB);
		
		glEnable(GL_DEPTH_TEST);
		glClearDepth(1.0f);
		glDepthFunc(GL_LEQUAL);
		glDepthRange(0.0f, 1.0f);
		glDepthMask(GL_TRUE);
		
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);
		
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}
	
	#define UCOM UV2("screen_dim"), UV2("mcursor_pos") // common uniforms
	#define UMAT UM4("world_to_cam"), UM4("cam_to_world"), UM4("cam_to_clip") // transformation uniforms
	
	//shad_equirectangular_to_cubemap = new_shader("equirectangular_to_cubemap.vert",	"equirectangular_to_cubemap.frag", {UCOM}, {{0,"equirectangular"}});
	
	{ // init game console overlay
		f32 sz =	0 ? 24 : 18; // 14 16 24
		f32 jpsz =	floor(sz * 1.75f);
		
		std::initializer_list<font::Glyph_Range> ranges = {
			{ "consola.ttf",	sz,		  U'\xfffd' }, // missing glyph placeholder, must be the zeroeth glyph
			{ "consola.ttf",	sz,		  U' ', U'~' },
			{ "consola.ttf",	sz,		{ U'ß',U'Ä',U'Ö',U'Ü',U'ä',U'ö',U'ü' } }, // german umlaute
			{ "meiryo.ttc",		jpsz,	  U'\x3040', U'\x30ff' }, // hiragana +katakana
			{ "meiryo.ttc",		jpsz,	{ U'　',U'、',U'。',U'”',U'「',U'」' } }, // some jp puncuation
		};
		
		overlay_font = new font::Font(sz, ranges);
		
		vbo_overlay_font.init(&font::mesh_vert_layout);
		shad_font = new_shader("font.vert", "font.frag", {UCOM}, {{0,"glyphs"}});
	}
	
	//
	
	static GLint OVERLAY_TEXTURE_UNIT = 7;
	
	GLuint tex_sampler_nearest;
	{
		glGenSamplers(1, &tex_sampler_nearest);
		glBindSampler(OVERLAY_TEXTURE_UNIT, tex_sampler_nearest);
		
		glSamplerParameteri(tex_sampler_nearest, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(tex_sampler_nearest, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	
	auto* shad_overlay_tex =		new_shader("overlay_tex.vert",	"overlay_tex.frag",		{UCOM, UV2("pos_clip"), UV2("size_clip")}, {{OVERLAY_TEXTURE_UNIT,"tex0"}});
	//auto* shad_overlay_cubemap =	new_shader("overlay_tex.vert",	"overlay_cubemap.frag",	{UCOM, UV2("pos_clip"), UV2("size_clip")}, {{0,"tex0"}});
	
	//shad_equirectangular_to_cubemap = new_shader("equirectangular_to_cubemap.vert",	"equirectangular_to_cubemap.frag", {UCOM}, {{0,"equirectangular"}});
	
	/*
	std::array<cstr, 6> HUMUS_CUBEMAP_FACE_CODES = {
		"posx",
		"negx",
		"negy", // opengl has the y faces in the wrong order for some reason
		"posy",
		"posz",
		"negz",
	};
	*/
	
	auto* shad_sky = new_shader("skybox.vert",	"skybox.frag",	{UCOM, UMAT});
	auto* shad_main = new_shader("main.vert",	"main.frag",	{UCOM, UMAT, USI("texture_res"), USI("atlas_textures_count")}, {{0,"atlas"}});
	
	Texture2D tex_block_atlas;
	{
		// combine all textures into a texture atlas
		
		iv2 tex_atlas_res = (texture_res +0) * iv2(ATLAS_BLOCK_FACES_COUNT,atlas_textures_count); // +2 for one pixel border
		
		tex_block_atlas.alloc_cpu_single_mip(PT_SRGB8_LA8, tex_atlas_res);
		
		dbg_assert(tex_block_atlas.get_pixel_size() == 4);
		u32* src_pixels;
		u32* dst_pixels = (u32*)tex_block_atlas.data.data;
		
		s32 face_LUT[ATLAS_BLOCK_FACES_COUNT] = {
			/* UVZW_BLOCK_FACE_SIDE		*/	1,
			/* UVZW_BLOCK_FACE_TOP		*/	2,
			/* UVZW_BLOCK_FACE_BOTTOM	*/	0,
		};

		auto src = [&] (s32 x, s32 y, s32 face) -> u32* {
			s32 w = texture_res;
			s32 h = texture_res;
			return &src_pixels[face_LUT[face]*h*w + y*w + x];
		};
		auto dst = [&] (s32 x, s32 y, s32 face, s32 tex_index) -> u32* {
			s32 w = texture_res +0;
			s32 h = texture_res +0;
			return &dst_pixels[tex_index*h*ATLAS_BLOCK_FACES_COUNT*w + y*ATLAS_BLOCK_FACES_COUNT*w  + face*w + x];
		};
		
		for (s32 tex_index=0; tex_index<atlas_textures_count; ++tex_index) {
			
			Texture2D_File earth_tex (CS_AUTO, block_texture_name[tex_index]);
			earth_tex.load();
			dbg_assert(earth_tex.type == PT_SRGB8_LA8);
			
			dbg_assert(all(earth_tex.dim == iv2(texture_res, texture_res*ATLAS_BLOCK_FACES_COUNT)));
			
			dbg_assert(earth_tex.get_pixel_size() == 4);
			src_pixels = (u32*)earth_tex.data.data;
			
			for (s32 block_face_i=0; block_face_i<ATLAS_BLOCK_FACES_COUNT; ++block_face_i) {
				
				/*for (s32 x=0; x<texture_res +2; ++x) { // top border
					*dst(x,0, block_face_i, tex_index) = 0xff0000ff;
				}*/
				
				for (s32 y=0; y<texture_res; ++y) {
					
					//*dst(0,y, block_face_i, tex_index) = 0xff0000ff;
					
					for (s32 x=0; x<texture_res; ++x) {
						u32 col = *src(x,y, block_face_i);
						*dst(x,y, block_face_i, tex_index) = col;
					}
					
					//*dst(texture_res+1,y, block_face_i, tex_index) = 0xff0000ff;
					
				}
				
				
				/*for (s32 x=0; x<texture_res +2; ++x) {
					*dst(x,texture_res+1, block_face_i, tex_index) = 0xff0000ff;
				}*/
			}
		}
		
		tex_block_atlas.upload();
	}
	
	#if 0
	Texture2D noise_test;
	auto gen_noise_test = [&] () {
		iv2 size = CHUNK_DIM.xy();
		
		noise_test.alloc_cpu_single_mip(PT_LRGB8, size);
		
		dbg_assert(noise_test.get_pixel_size() == 3);
		u8* dst_pixels = (u8*)noise_test.data.data;
		auto dst = [&] (s32 x, s32 y) -> u8* {
			return &dst_pixels[y*size.x*3 + x*3];
		};
		
		iv2 pos;
		for (pos.y=0; pos.y<size.y; ++pos.y) {
			for (pos.x=0; pos.x<size.x; ++pos.x) {
				
				f32 val = heightmap_perlin2d((v2)pos);
				heightmap[pos.y][pos.x] = val;
				
				val = (val +0.5f) / 8;
				
				u32 tmp = (u32)(clamp(val, 0.0f,1.0f) * 255.0f);
				dst(pos.x,pos.y)[0] = tmp;
				dst(pos.x,pos.y)[1] = tmp;
				dst(pos.x,pos.y)[2] = tmp;
			}
		}
		
		noise_test.upload();
		
		gen_chunks();
	};
	#endif
	
	//
	struct Camera_View {
		v3	pos_world;
		v2	ori_ae;
		
		f32	vfov;
		f32	hfov;
		
		f32 clip_near =		1.0f/256;
		f32 clip_far =		512;
		
		v2 frust_scale;
		
		hm	world_to_cam;
		hm	cam_to_world;
		m4	cam_to_clip;
		
		void calc_final_matricies (m3 world_to_cam_rot, m3 cam_to_world_rot) {
			world_to_cam = world_to_cam_rot * translateH(-pos_world);
			cam_to_world = translateH(pos_world) * cam_to_world_rot;
			
			{
				frust_scale.y = tan(vfov / 2);
				frust_scale.x = frust_scale.y * inp.wnd_dim_aspect.x;
				
				hfov = atan(frust_scale.x) * 2;
				
				v2 frust_scale_inv = 1.0f / frust_scale;
				
				f32 x = frust_scale_inv.x;
				f32 y = frust_scale_inv.y;
				f32 a = (clip_far +clip_near) / (clip_near -clip_far);
				f32 b = (2.0f * clip_far * clip_near) / (clip_near -clip_far);
				
				cam_to_clip = m4::row(
								x, 0, 0, 0,
								0, y, 0, 0,
								0, 0, a, b,
								0, 0, -1, 0 );
			}
		}
	};
	
	struct Player {
		#if _2D_TEST
		//v3	pos_world =		v3(4,32,3);
		v3	pos_world =		v3(12,32,3);
		#else
		v3	pos_world =		v3(4,32,43);
		#endif
		v3	vel_world =		0;
		
		v2	ori_ae =		v2(deg(0), deg(+80)); // azimuth elevation
		f32	vfov =			deg(80);
		
		v3	camera_offset_cam = v3(0,0.6f,3);
		
		f32 collision_r =	0.4f;
		f32 collision_h =	1.7f;
		
		
		bool opt_open = true;
		void options () {
			option_group("player", &opt_open);
			if (opt_open) {
				option(		"  pos_world",			&pos_world);
				option(		"  vel_world",			&vel_world);
				option_deg(	"  ori_ae",				&ori_ae);
				option_deg(	"  vfov",				&vfov);
				option(		"  camera_offset_cam",	&camera_offset_cam);
				option(		"  collision_r",		&collision_r);
				option(		"  collision_h",		&collision_h);
			}
		}
	};
	Player player;
	
	auto load_game = [&] () {
		trigger_load_game = false;
		load_struct("flycam", &flycam);
	};
	auto save_game = [&] () {
		trigger_save_game = false;
		save_struct("flycam", flycam);
	};
	
	load_game();
	
	inital_chunk();
	while (chunks.size() < 32) new_chunk(player.pos_world);
	
	bool one_chunk_every_frame = false;
	bool one_chunk_every_frame_open = false;
	s32 one_chunk_every_frame_period = 60;
	
	Vbo test_vbo;
	test_vbo.init(&mesh_vert_layout);
	
	// 
	f64 prev_t = glfwGetTime();
	f32 avg_dt = 1.0f / 60;
	f32 avg_dt_alpha = 0.025f;
	dt = 0;
	
	for (s32 frame_i=0;; ++frame_i) {
		
		dt = min(dt, 1.0f / 20);
		
		begin_overlay_text();
		
		{ //
			f32 fps = 1.0f / dt;
			f32 dt_ms = dt * 1000;
			
			f32 avg_fps = 1.0f / avg_dt;
			f32 avdt_ms = avg_dt * 1000;
			
			//printf("frame #%5d %6.1f fps %6.2f ms  avg: %6.1f fps %6.2f ms\n", frame_i, fps, dt_ms, avg_fps, avdt_ms);
			glfwSetWindowTitle(wnd, prints("%s %6d  %6.1f fps avg %6.2f ms avg  %6.2f ms", app_name, frame_i, avg_fps, avdt_ms, dt_ms).c_str());
			
			overlay_line(prints("%s %6d  %6.1f fps avg %6.2f ms avg  %6.2f ms", app_name, frame_i, avg_fps, avdt_ms, dt_ms), srgb(255,40,0)*0.85f);
		}
		
		inp.mouse_look_diff = 0;
		
		glfwPollEvents();
		
		inp.get_non_callback_input();
		
		begin_options();
		
		overlay_line(prints("mouse:   %4d %4d -> %.2f %.2f", inp.mcursor_pos_px.x,inp.mcursor_pos_px.y, mouse.x,mouse.y));
		
		option("viewing_flycam", &viewing_flycam);
		flycam.options();
		player.options();
		
		if (glfwWindowShouldClose(wnd)) break;
		
		for (auto* s : shaders)			s->reload_if_needed();
		
		if (trigger_save_game) save_game();
		if (trigger_load_game) load_game();
		
		Camera_View view;
		
		m3 world_to_cam_rot;
		m3 cam_to_world_rot;
		{ // view/player rotation
			auto clamped_cam_ae = [] (v2 cam_ae, v2 mouse_look_sens) -> v2 {
				cam_ae -= inp.mouse_look_diff * mouse_look_sens;
				cam_ae.x = mymod(cam_ae.x, deg(360));
				cam_ae.y = clamp(cam_ae.y, deg(2), deg(180.0f -2));
				
				return cam_ae;
			};
			
			if (viewing_flycam) {
				
				view.vfov = flycam.vfov;
				v2 mouse_look_sens = v2(deg(1.0f / 8)) * (view.vfov / deg(70));
				
				view.ori_ae = flycam.ori_ae = clamped_cam_ae(flycam.ori_ae, mouse_look_sens);
			} else {
				
				view.vfov = player.vfov;
				v2 mouse_look_sens = v2(deg(1.0f / 8)) * (view.vfov / deg(70));
				
				view.ori_ae = player.ori_ae = clamped_cam_ae(player.ori_ae, mouse_look_sens);
			}
			world_to_cam_rot = rotate3_X(-view.ori_ae.y) * rotate3_Z(-view.ori_ae.x);
			cam_to_world_rot = rotate3_Z(view.ori_ae.x) * rotate3_X(view.ori_ae.y);
		}
		
		option("heightmap_perlin2d_octaves", get_heightmap_perlin2d_octaves_count, set_heightmap_perlin2d_octaves_count, &heightmap_perlin2d_octaves_open);
		if (heightmap_perlin2d_octaves_open) {
			for (s32 i=0; i<(s32)heightmap_perlin2d_octaves.size(); ++i) {
				auto& o = heightmap_perlin2d_octaves[i];
				
				v2 tmp = v2(o.freq, o.amp);
				
				if (option(prints("  [%2d]", i), &tmp)) trigger_regen_chunks = true;
				
				o.freq = tmp.x;	o.amp = tmp.y;
			}
		}
		
		if (trigger_regen_chunks) {
			trigger_regen_chunks = false;
			for (auto& c : chunks) gen_chunk(c.second);
		}
		
		option("one_chunk_every_frame", &one_chunk_every_frame, &one_chunk_every_frame_open);
		if (one_chunk_every_frame_open) option("  period", &one_chunk_every_frame_period);
		
		if (one_chunk_every_frame && frame_i % one_chunk_every_frame_period == 0 && frame_i != 0) new_chunk(player.pos_world);
		
		overlay_line(prints("chunks:  %4d", (s32)chunks.size()));
		
		if (viewing_flycam) { // view/player position
			f32 cam_speed_forw = flycam.speed;
			if (inp.move_fast) cam_speed_forw *= flycam.speed_fast_mul;
			
			v3 cam_vel = cam_speed_forw * v3(1,1,1);
			
			v3 cam_vel_cam = normalize_or_zero( (v3)inp.move_dir ) * cam_vel;
			flycam.pos_world += (cam_to_world_rot * cam_vel_cam) * dt;
			
			//printf(">>> %f %f %f\n", cam_vel_cam.x, cam_vel_cam.y, cam_vel_cam.z);
			
			view.pos_world = flycam.pos_world;
		} else {
			#if _2D_TEST
			v3 pos_world = player.pos_world;
			v3 vel_world = player.vel_world;
			
			pos_world.z = 1;
			
			// 
			v2 player_walk_speed = 3 * (inp.move_fast ? 3 : 1);
			
			v2 walk_vel_world = rotate2(player.ori_ae.x) * (normalize_or_zero( (v2)iv2(inp.move_dir.x, -inp.move_dir.z) ) * player_walk_speed);
			
			pos_world += v3(walk_vel_world, 0) * dt;
			
			{
				f32 r = player.collision_r;
				f32 h = player.collision_h;
				
				bpos start =	(bpos)floor(pos_world -v3(r,r,0));
				bpos end =		(bpos)ceil(pos_world +v3(r,r,h));
				
				bpos i;
				for (i.z=start.z; i.z<end.z; ++i.z) {
					for (i.y=start.y; i.y<end.y; ++i.y) {
						for (i.x=start.x; i.x<end.x; ++i.x) {
							
							v2 p = pos_world.xy() -(v2)i.xy(); // circle position relative to block
							
							v2 closest_p = clamp(p, 0,1); // closest point on block
							
							v2 pen = closest_p -p; // potential penetration vector
							bool could_collide = length_sqr(pen) <= r*r;
							
							if (!could_collide) continue;
							
							auto* b = query_block(i);
							bool colliding = !bt_is_pervious(b->type);
							
							if (colliding) {
								if (abs(pen.x) >= abs(pen.y)) {
									vel_world.x = 0;
									pos_world.x = i.x +closest_p.x +(pen.x < 0 ? +r : -r);
								} else {
									vel_world.y = 0;
									pos_world.y = i.y +closest_p.y +(pen.y < 0 ? +r : -r);
								}
							}
						}
					}
				}
			}
			
			player.vel_world = vel_world;
			player.pos_world = pos_world;
			
			view.pos_world = player.pos_world +(cam_to_world_rot * player.camera_offset_cam);
			#else
			v3 pos_world = player.pos_world;
			v3 vel_world = player.vel_world;
			
			// 
			bool grounded = false;
			if (test_cylinder_collision(pos_world, player.collision_r, player.collision_h)) {
				vel_world.z = 0;
				grounded = true;
			}
			
			v3 accel_world = 0;
			if (!grounded) accel_world += GRAV_ACCEL;
			
			//
			v2 walk_vel_world = 0;
			if (grounded) {
				
				v2 player_walk_speed = 3 * (inp.move_fast ? 3 : 1);
				
				walk_vel_world = rotate2(player.ori_ae.x) * (normalize_or_zero( (v2)iv2(inp.move_dir.x, -inp.move_dir.z) ) * player_walk_speed);
			}
			
			vel_world += accel_world * dt;
			pos_world += (vel_world +v3(walk_vel_world, 0)) * dt;
			
			player.vel_world = vel_world;
			player.pos_world = pos_world;
			view.pos_world = player.pos_world +(cam_to_world_rot * player.camera_offset_cam);
			#endif
		}
		
		view.calc_final_matricies(world_to_cam_rot, cam_to_world_rot);
		
		//// Draw
		for (auto* s : shaders) { // set common uniforms
			if (s->valid()) {
				s->bind();
				s->set_unif("screen_dim", (v2)inp.wnd_dim);
				s->set_unif("mcursor_pos", inp.bottom_up_mcursor_pos());
			}
		}
		
		glViewport(0,0, inp.wnd_dim.x,inp.wnd_dim.y);
		
		if (shad_sky->valid()) { // draw skybox
			glDisable(GL_DEPTH_TEST);
			
			shad_sky->bind();
			shad_sky->set_unif("world_to_cam",	view.world_to_cam.m4());
			shad_sky->set_unif("cam_to_world",	view.cam_to_world.m4());
			shad_sky->set_unif("cam_to_clip",	view.cam_to_clip);
			
			glDrawArrays(GL_TRIANGLES, 0, 6*6);
			
			glEnable(GL_DEPTH_TEST);
		}
		glClear(GL_DEPTH_BUFFER_BIT);
		
		#if 0
		for (auto* m : meshes_opaque) {
			if (m->shad->valid()) {
				m->bind_textures();
				
				hm model_to_world = m->get_transform();
				
				m->shad->bind();
				m->shad->set_unif("model_to_world",	model_to_world.m4());
				m->shad->set_unif("world_to_cam",	world_to_cam.m4());
				m->shad->set_unif("cam_to_clip",	cam_to_clip);
				m->shad->set_unif("cam_to_world",	cam_to_world.m4());
				
				m->vbo.draw_entire(m->shad);
			}
		}
		
		for (auto* m : meshes_translucent) {
			
			m->bind_textures();
			
			hm model_to_world = m->get_transform();
			
			if (m->shad->valid()) {
				m->shad->bind();
				m->shad->set_unif("model_to_world",	model_to_world.m4());
				m->shad->set_unif("world_to_cam",	world_to_cam.m4());
				m->shad->set_unif("cam_to_clip",	cam_to_clip);
				m->shad->set_unif("cam_to_world",	cam_to_world.m4());
				
				m->vbo.draw_entire(m->shad);
			}
			if (m->shad_transp_pass2->valid()) { 
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				glDepthFunc(GL_LESS);
			
				m->shad_transp_pass2->bind();
				m->shad_transp_pass2->set_unif("model_to_world",	model_to_world.m4());
				m->shad_transp_pass2->set_unif("world_to_cam",	world_to_cam.m4());
				m->shad_transp_pass2->set_unif("cam_to_clip",	cam_to_clip);
				m->shad_transp_pass2->set_unif("cam_to_world",	cam_to_world.m4());
				
				m->vbo.draw_entire(m->shad_transp_pass2);
				
				glDepthFunc(GL_LEQUAL);
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
			}
		}
		#endif
		
		if (shad_main->valid()) {
			bind_texture_unit(0, &tex_block_atlas);
			
			shad_main->bind();
			shad_main->set_unif("world_to_cam",	view.world_to_cam.m4());
			shad_main->set_unif("cam_to_world",	view.cam_to_world.m4());
			shad_main->set_unif("cam_to_clip",	view.cam_to_clip);
			
			shad_main->set_unif("texture_res", texture_res);
			shad_main->set_unif("atlas_textures_count", atlas_textures_count);
			
			for (auto& chunk_hash_pair : chunks) {
				auto& chunk = chunk_hash_pair.second;
				
				chunk->generate_blocks_mesh();
				
				chunk->vbo.draw_entire(shad_main);
			}
		}
		
		if (shad_main->valid()) {
			glDisable(GL_CULL_FACE);
			
			shad_main->bind();
			// uniforms still bound
			
			test_vbo.clear();
			
			v3 pos_world = player.pos_world;
			f32 r = player.collision_r;
			f32 h = player.collision_h;
			
			bool colliding = test_cylinder_collision(pos_world, r, h, &test_vbo);
			
			{
				s32 cylinder_sides = 32;
				
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&test_vbo.vertecies, sizeof(Mesh_Vertex)*(3+6+3)*cylinder_sides);
				
				f32 w = BLOCK_TEXTURE_INDEX_MISSING;
				
				lrgba8 col = 255;
				if (colliding) col = lrgba8(255,0,0,255);
				
				v2 rv = v2(r,0);
				
				for (s32 i=0; i<cylinder_sides; ++i) {
					f32 rot_a = (f32)(i +0) / (f32)cylinder_sides * deg(360);
					f32 rot_b = (f32)(i +1) / (f32)cylinder_sides * deg(360);
					
					m2 ma = rotate2(rot_a);
					m2 mb = rotate2(rot_b);
					
					*out++ = { pos_world +v3(0,0,     h), v4(v2(0.5f),					UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(ma * rv, h), v4(ma * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(mb * rv, h), v4(mb * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					
					*out++ = { pos_world +v3(mb * rv, 0), v4(mb * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(mb * rv, h), v4(mb * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(ma * rv, 0), v4(ma * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(ma * rv, 0), v4(ma * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(mb * rv, h), v4(mb * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(ma * rv, h), v4(ma * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					
					*out++ = { pos_world +v3(0,0,     0), v4(v2(0.5f),					UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(mb * rv, 0), v4(mb * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
					*out++ = { pos_world +v3(ma * rv, 0), v4(ma * v2(+0.5f,0) +0.5f,	UVZW_BLOCK_FACE_TOP,w), col };
				}
			}
			
			test_vbo.upload();
			test_vbo.draw_entire(shad_main);
			
			glEnable(GL_CULL_FACE);
		}
		
		{
			v2 LL = v2(0,0);
			v2 LR = v2(1,0);
			v2 UL = v2(0,1);
			v2 UR = v2(1,1);
			
			auto draw_overlay_tex2d = [&] (Texture2D* tex, v2 pos01, v2 size_multiplier=1) {
				if (!shad_overlay_tex->valid()) {
					dbg_assert(false);
					return;
				}
				
				v2 size_screen = (v2)tex->dim * size_multiplier;
				v2 size_clip = size_screen / ((v2)inp.wnd_dim / 2);
				
				// pos is the lower left corner of the quad
				v2 pos_screeen = ((v2)inp.wnd_dim -size_screen) * pos01; // [0,1] => [touches ll corner of screen, touches ur corner of screen]
				
				v2 pos_clip = (pos_screeen / (v2)inp.wnd_dim) * 2 -1;
				
				glEnable(GL_BLEND);
				glDisable(GL_DEPTH_TEST);
				glDisable(GL_CULL_FACE);
				
				shad_overlay_tex->bind();
				shad_overlay_tex->set_unif("pos_clip", pos_clip);
				shad_overlay_tex->set_unif("size_clip", size_clip);
				bind_texture_unit(OVERLAY_TEXTURE_UNIT, tex);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
			};
			#if 0
			auto draw_overlay_texCube = [&] (Texture* tex, v2 pos01, v2 size_px) {
				if (!shad_overlay_cubemap->valid()) {
					dbg_assert(false);
					return;
				}
				
				v2 size_screen = size_px;
				v2 size_clip = size_screen / ((v2)inp.wnd_dim / 2);
				
				// pos is the lower left corner of the quad
				v2 pos_screeen = ((v2)inp.wnd_dim -size_screen) * pos01; // [0,1] => [touches ll corner of screen, touches ur corner of screen]
				
				v2 pos_clip = (pos_screeen / (v2)inp.wnd_dim) * 2 -1;
				
				glEnable(GL_BLEND);
				glDisable(GL_DEPTH_TEST);
				glDisable(GL_CULL_FACE);
				
				shad_overlay_cubemap->bind();
				shad_overlay_cubemap->set_unif("pos_clip", pos_clip);
				shad_overlay_cubemap->set_unif("size_clip", size_clip);
				bind_texture_unit(0, tex);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
			};
			#endif
			
			if (1 && shad_overlay_tex->valid()) {
				//draw_overlay_tex2d(&tex_block_atlas, LR, 8);
				//draw_overlay_tex2d(&noise_test, LL, 8);
			}
			#if 0
			if (0 && shad_overlay_cubemap->valid()) {
				//draw_overlay_texCube(tex_test_cubemap1, UR, (v2)min(inp.wnd_dim.x, inp.wnd_dim.y) / 2);
				//draw_overlay_texCube(tex_test_cubemap2, UL, (v2)min(inp.wnd_dim.x, inp.wnd_dim.y) / 2);
			}
			#endif
			
		}
		
		if (shad_font->valid()) {
			glEnable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			
			shad_font->bind();
			shad_font->set_unif("screen_dim", (v2)inp.wnd_dim);
			bind_texture_unit(0, &overlay_font->tex);
			
			vbo_overlay_font.upload();
			vbo_overlay_font.draw_entire(shad_font);
			
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
		}
		
		glfwSwapBuffers(wnd);
		
		{
			f64 now = glfwGetTime();
			dt = now -prev_t;
			prev_t = now;
			
			avg_dt = lerp(avg_dt, dt, avg_dt_alpha);
		}
	}
	
	platform_terminate();
	
	return 0;
}
