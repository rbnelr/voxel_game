#define _CRT_SECURE_NO_WARNINGS 1

#include <cstdio>
#include <array>
#include <vector>
#include <string>

#include "types.hpp"
#include "lang_helpers.hpp"
#include "math.hpp"
#include "bit_twiddling.hpp"
#include "vector/vector.hpp"
#include "intersection_test.hpp"
#include "open_simplex_noise.hpp"

#define _USING_V110_SDK71_ 1
#include "glad.c"
#include "GLFW/glfw3.h"

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

typedef f32		flt;

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

#include "platform.hpp"

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

#define PROFILE_BEGIN(name)	f64 __profile_begin_##name = glfwGetTime()
#define PROFILE_END_PRINT(name, format, ...)	printf(">> PROFILE: %s took %8.3f ms  " format "\n", STRINGIFY(name), (glfwGetTime() -__profile_begin_##name) * 1000, __VA_ARGS__)

#define PROFILE_END_ACCUM(name)	name += (glfwGetTime() -__profile_begin_##name)
#define PROFILE_PRINT(name, format, ...)	printf(">> PROFILE: %s took %8.3f ms  " format "\n", STRINGIFY(name), name * 1000, __VA_ARGS__)

namespace random {
	
	f32 flt (f32 l=0, f32 h=1) {
		return (f32)rand() / (f32)RAND_MAX;
	}
	
}

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

#define LLL	v3(-1,-1,-1)
#define HLL	v3(+1,-1,-1)
#define LHL	v3(-1,+1,-1)
#define HHL	v3(+1,+1,-1)
#define LLH	v3(-1,-1,+1)
#define HLH	v3(+1,-1,+1)
#define LHH	v3(-1,+1,+1)
#define HHH	v3(+1,+1,+1)

#define QUAD(a,b,c,d) a,d,b, b,d,c // facing inward

const v3 cube_faces[6*6] = {
	QUAD(	LHL,
			LLL,
			LLH,
			LHH ),
	
	QUAD(	HLL,
			HHL,
			HHH,
			HLH ),
	
	QUAD(	LLL,
			HLL,
			HLH,
			LLH ),
	
	QUAD(	HHL,
			LHL,
			LHH,
			HHH ),
	
	QUAD(	HLL,
			LLL,
			LHL,
			HHL ),
	
	QUAD(	LLH,
			HLH,
			HHH,
			LHH )
};

#undef QUAD
#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward

const v3 cube_faces_inward[6*6] = {
	QUAD(	LHL,
			LLL,
			LLH,
			LHH ),
	
	QUAD(	HLL,
			HHL,
			HHH,
			HLH ),
	
	QUAD(	LLL,
			HLL,
			HLH,
			LLH ),
	
	QUAD(	HHL,
			LHL,
			LHH,
			HHH ),
	
	QUAD(	HLL,
			LLL,
			LHL,
			HHL ),
	
	QUAD(	LLH,
			HLH,
			HHH,
			LHH )
};

#undef QUAD

static v2			mouse; // for quick debugging
static s32			frame_i; // should only be used for debugging

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
static cstr textures_base_path =	"assets_src";

static cstr save_files =			"saves/%s.bin";

static bool load_data (strcr name, void* data, uptr size) {
	str file = prints(save_files, name.c_str());
	
	bool loaded = read_entire_file(file.c_str(), data, size);
	if (loaded) {
		logf(prints("%s loaded from \"%s\".", name.c_str(), file.c_str()).c_str());
	} else {
		logf_warning(prints("%s could not be loaded from \"%s\".", name.c_str(), file.c_str()).c_str());
	}
	
	return loaded;
};
static bool save_data (strcr name, void const* data, uptr size) {
	str file = prints(save_files, name.c_str());
	
	bool saved = overwrite_file(file.c_str(), data, size);
	if (saved) {
		logf(prints("%s saved to \"%s\".", name.c_str(), file.c_str()).c_str());
	} else {
		logf_warning(prints("could not write \"%s\", %s wont be loaded on next launch.", file.c_str(), name.c_str()).c_str());
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

#include "options_overlay.hpp"

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
typedef s64		bpos_t;
typedef s64v2	bpos2;
typedef s64v3	bpos;

typedef s64v2	chunk_pos_t;

//
// TODO: document
struct Chunk_Vbo_Vertex {
	v3		pos_world;
	v4		uvzw_atlas; // xy: [0,1] texture uv;  z: 0=side, 1=top, 2=bottom;  w: texture index
	flt		hp_ratio; // [0,1]
	flt		brightness;
	lrgba8	dbg_tint;
};

static Vertex_Layout chunk_vbo_vert_layout = {
	{ "pos_world",	T_V3,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, pos_world) },
	{ "uvzw_atlas",	T_V4,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, uvzw_atlas) },
	{ "hp_ratio",	T_FLT,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, hp_ratio) },
	{ "brightness",	T_FLT,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, brightness) },
	{ "dbg_tint",	T_U8V4,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, dbg_tint) },
};

#include "blocks.hpp"
#include "chunks.hpp"

static size_t hash (chunk_pos_t v) {
	return 53 * (std::hash<s64>()(v.x) + 53) + std::hash<s64>()(v.y);
};

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
			return ::hash(v.v);
		}
	};
}

struct Noise_Octave {
	f32	period;
	f32	amp;
};
std::vector<Noise_Octave> heightmap_noise_octaves = {
	{ 300,		12		},
	{ 120,		12		},
	{ 50,		4.5f	},
	{ 20,		3		},
	{ 3,		0.225f	},
};
static s32 get_heightmap_noise_octaves_count () {			return (s32)heightmap_noise_octaves.size(); }
static void set_heightmap_noise_octaves_count (s32 count) {	heightmap_noise_octaves.resize(count); }
static bool heightmap_noise_octaves_open = true;


std::vector<Noise_Octave> heightmap_multiplier_noise_octaves = {
	{ 500,		0.2f		},
	{ 1000,		0.4f		},
	{ 200,		0.2f		},
};
static s32 get_heightmap_multiplier_noise_octaves_count () {			return (s32)heightmap_multiplier_noise_octaves.size(); }
static void set_heightmap_multiplier_noise_octaves_count (s32 count) {	heightmap_multiplier_noise_octaves.resize(count); }
static bool heightmap_multiplier_noise_octaves_open = true;

static f32 heightmap_multiplier (OSN::Noise<2> const& osn_noise, v2 pos_world) {
	auto noise = [&] (v2 pos, flt period, flt ang_offs, v2 offs) {
		pos = rotate2(ang_offs) * pos;
		pos /= period; // period is inverse frequency
		pos += offs;
		return osn_noise.eval<flt>(pos.x, pos.y);
	};
	
	f32 tot = 0;
	
	s32 i = get_heightmap_noise_octaves_count();
	for (auto& o : heightmap_multiplier_noise_octaves) {
		v2 offs = (i % 3 ? +1 : -1) * 12.34f * (f32)i;
		offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (f32)i;
		tot += noise(pos_world, o.period, deg(37.17f) * (f32)i, offs) * o.amp;
		
		++i;
	}
	
	tot = pow(1.6f, 2.8f * (tot +0.1f));
	
	return tot;
}

static f32 heightmap (OSN::Noise<2> const& osn_noise, v2 pos_world) {
	auto noise = [&] (v2 pos, flt period, flt ang_offs, v2 offs) {
		pos = rotate2(ang_offs) * pos;
		pos /= period; // period is inverse frequency
		pos += offs;
		return osn_noise.eval<flt>(pos.x, pos.y);
	};
	
	f32 tot = 0;
	
	s32 i = 0;
	for (auto& o : heightmap_noise_octaves) {
		v2 offs = (i % 3 ? +1 : -1) * 12.34f * (f32)i;
		offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (f32)i;
		flt val = noise(pos_world, o.period, deg(37.17f) * (f32)i, offs) * o.amp;
		tot += val;
		
		++i;
	}
	
	tot *= heightmap_multiplier(osn_noise, pos_world);
	
	return tot +32;
}

static flt noise_tree_freq = 0.1f;
static flt noise_tree_amp = 5;

#if 0
static lrgb8 noise_tree (v2 pos_world) {
	//using namespace perlin_noise_n;
	pos_world = abs(pos_world);
	
	f32 val = perlin_octave(pos_world, noise_tree_freq) * noise_tree_amp;
	
	return spectrum_gradient(val);
}
#endif

static u64 world_seed = 0;

static void gen_chunk_blocks (Chunk* chunk) {
	bpos_t water_level = 21;
	
	bpos i; // position in chunk
	for (i.z=0; i.z<CHUNK_DIM.z; ++i.z) {
		for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
				auto* b = chunk->get_block(i);
				
				b->type = i.z <= water_level ? BT_WATER : BT_AIR;
				b->hp_ratio = 1;
				b->dbg_tint = 255;
			}
		}
	}
	
	OSN::Noise<2> noise( world_seed );
	//OSN::Noise<2> noise( hash(chunk->pos) );
	
	for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
		for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
			
			v2 pos_world = (v2)(i.xy() +chunk->pos*CHUNK_DIM.xy());
			
			f32 height = heightmap(noise, pos_world);
			s32 highest_block = (s32)floor(height -1 +0.5f); // -1 because height 1 means the highest block is z=0
			
			for (i.z=0; i.z <= min(highest_block, (s32)CHUNK_DIM.z-1); ++i.z) {
				auto* b = chunk->get_block(i);
				if (i.z == highest_block) {
					b->type = BT_GRASS;
				} else {
					b->type = BT_EARTH;
				}
				b->dbg_tint = lrgba8(spectrum_gradient(map(height, 0, 45)), 255);
				//b->dbg_tint = lrgba8( noise_tree(pos_world), 255);
			}
		}
	}
	
	chunk->update_whole_chunk_changed();
}

#include <unordered_map>
std::unordered_map<s64v2_hashmap, Chunk*> chunks;

static Chunk* _prev_query_chunk = nullptr; // avoid hash map lookup most of the time, since a lot of query_chunk's are going to end up in the same chunk (in query_block of clustered blocks)

static Chunk* query_chunk (chunk_pos_t pos) {
	if (_prev_query_chunk && equal(_prev_query_chunk->pos, pos)) {
		return _prev_query_chunk;
	} else {
		
		auto k = chunks.find({pos});
		if (k == chunks.end()) return nullptr;
		
		Chunk* chunk = k->second;
		
		_prev_query_chunk = chunk;
		
		return chunk;
	}
}
static Block* query_block (bpos p, Chunk** out_chunk) {
	if (out_chunk) *out_chunk = nullptr;
	
	if (p.z < 0 || p.z >= CHUNK_DIM.z) return (Block*)&B_OUT_OF_BOUNDS;
	
	bpos block_pos_chunk;
	chunk_pos_t chunk_pos = get_chunk_from_block_pos(p, &block_pos_chunk);
	
	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk) return (Block*)&B_NO_CHUNK;
	
	if (out_chunk) *out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
}

static void generate_new_chunk (chunk_pos_t chunk_pos) {
	
	Chunk* c = new Chunk();
	
	c->pos = chunk_pos;
	c->init_gl();
	gen_chunk_blocks(c);
	
	chunks.insert({{c->pos}, c});
}

static void inital_chunk (v3 player_pos_world) {
	generate_new_chunk( get_chunk_from_block_pos( (bpos)floor(player_pos_world) ) );
}

//
static f32			dt;

enum fps_mouse_mode_e { DEV_MODE=0, FPS_MODE };

static fps_mouse_mode_e fps_mouse_mode =	FPS_MODE;
static bool mouselook_enabled =				fps_mouse_mode == FPS_MODE;

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
			iv2 tmp = iv2((int)x, (int)y);
			
			if (mouselook_enabled) mouse_look_diff = (v2)(tmp -mcursor_pos_px);
			
			mcursor_pos_px = tmp;
			
			mouse = v2(x,y) / (v2)wnd_dim;
		}
		
		move_dir = clamp(move_dir, -1,+1);
	}
	
	v2 bottom_up_mcursor_pos () {
		return v2(mcursor_pos_px.x, wnd_dim.y -mcursor_pos_px.y);
	}
};
static Input		inp;

static bool controling_flycam =		true;
static bool viewing_flycam =		true;

struct Flycam {
	v3	pos_world =			v3(-15, -27, 50);
	v2	ori_ae =			v2(deg(350), deg(+75)); // azimuth elevation
	
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

static f32 grav_accel_down = 20;

static f32 jump_height_from_jump_impulse (f32 jump_impulse_up, f32 grav_accel_down) {
	return jump_impulse_up*jump_impulse_up / grav_accel_down * 0.5f;
}
static f32 jump_impulse_from_jump_height (f32 jump_height, f32 grav_accel_down) {
	return sqrt( 2.0f * jump_height * grav_accel_down );
}

//
static bool trigger_respawn_player =	true;
static bool trigger_regen_chunks =		false;
static bool trigger_dbg_heightmap_visualize =	false;
static bool trigger_save_game =			false;
static bool trigger_load_game =			false;
static bool jump_held =					false;
static bool hold_break_block =			false;
static bool trigger_place_block =		false;

static void glfw_key_event (GLFWwindow* window, int key, int scancode, int action, int mods) {
	dbg_assert(action == GLFW_PRESS || action == GLFW_RELEASE || action == GLFW_REPEAT);
	
	bool went_down =	action == GLFW_PRESS;
	bool went_up =		action == GLFW_RELEASE;
	
	bool repeated =		!went_down && !went_up; // GLFW_REPEAT
	
	bool alt =			(mods & GLFW_MOD_ALT) != 0;
	
	if (key == GLFW_KEY_F11 || (alt && key == GLFW_KEY_ENTER)) {
		if (went_down) toggle_fullscreen();
		return;
	}
	
	if (key == GLFW_KEY_F1) {
		if (went_down) {
			if (opt_mode == OPT_OVERLAY_DISABLED)	opt_mode = OPT_SELECTING;
			else									opt_mode = OPT_OVERLAY_DISABLED;
		}
		return;
	}
	if (key == GLFW_KEY_F2) {
		if (went_down) fps_mouse_mode = (fps_mouse_mode_e)!fps_mouse_mode;
		if (fps_mouse_mode == DEV_MODE)	stop_mouse_look();
		else							start_mouse_look();
		mouselook_enabled = fps_mouse_mode == FPS_MODE;
		return;
	}
	
	if (opt_mode == OPT_SELECTING) { 
		switch (key) {
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
				
				case GLFW_KEY_S:			inp.move_dir.y -= went_down ? +1 : -1;		break;
				case GLFW_KEY_W:			inp.move_dir.y += went_down ? +1 : -1;		break;
				
				case GLFW_KEY_LEFT_CONTROL:	inp.move_dir.z -= went_down ? +1 : -1;		break;
				case GLFW_KEY_SPACE:		inp.move_dir.z += went_down ? +1 : -1;
											jump_held = went_down;						break;
				
				case GLFW_KEY_LEFT_SHIFT:	inp.move_fast = went_down;					break;
				
				case GLFW_KEY_R:			if (went_down) trigger_regen_chunks = true;		break;
				case GLFW_KEY_Q:			if (went_down) trigger_respawn_player = true;	break;
				
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
	bool went_down = action == GLFW_PRESS;
	
	switch (button) {
		case GLFW_MOUSE_BUTTON_RIGHT:
			if (fps_mouse_mode == DEV_MODE) {
				if (went_down) {
					start_mouse_look();
					mouselook_enabled = true;
				} else {
					stop_mouse_look();
					mouselook_enabled = false;
				}
			} else {
				trigger_place_block = went_down;
			}
			break;
		case GLFW_MOUSE_BUTTON_LEFT: hold_break_block = went_down; break;
	}
}
static void glfw_mouse_scroll (GLFWwindow* window, double xoffset, double yoffset) {
	if (controling_flycam) {
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

int main (int argc, char** argv) {
	cstr app_name = "Voxel Game";
	
	platform_setup_context_and_open_window(app_name, iv2(1280, 720));
	
	if (fps_mouse_mode == DEV_MODE)	stop_mouse_look();
	else							start_mouse_look();
	
	//
	set_vsync(-1);
	
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
			#if 0
			{ "consola.ttf",	sz,		{ U'ß',U'Ä',U'Ö',U'Ü',U'ä',U'ö',U'ü' } }, // german umlaute
			{ "meiryo.ttc",		jpsz,	  U'\x3040', U'\x30ff' }, // hiragana +katakana
			{ "meiryo.ttc",		jpsz,	{ U'　',U'、',U'。',U'”',U'「',U'」' } }, // some jp puncuation
			#endif
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
	auto* shad_main = new_shader("main.vert",	"main.frag",	{UCOM, UMAT, USI("texture_res"), USI("atlas_textures_count"), USI("breaking_frames_count")}, {{0,"atlas"}, {1,"breaking"}});
	auto* shad_overlay = new_shader("overlay.vert",	"overlay.frag",	{UCOM, UMAT});
	
	Texture2D tex_block_atlas;
	{ // texture atlasing
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
	
	Texture2D_File tex_breaking (CS_LINEAR, "breaking.png");
	s32 breaking_frames_count;
	
	tex_breaking.load();
	tex_breaking.upload();
	
	breaking_frames_count = tex_breaking.dim.y / tex_breaking.dim.x;
	
	//
	struct Camera_View {
		v3	pos_world;
		v2	ori_ae;
		
		f32	vfov;
		f32	hfov;
		
		f32 clip_near =		1.0f/32;
		f32 clip_far =		8192;
		
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
	
	v3	initial_player_pos_world =		v3(4,32,43);
	v3	initial_player_vel_world =		0;
	
	struct Player {
		v3	pos_world;
		v3	vel_world;
		
		v2	ori_ae =		v2(deg(0), deg(+80)); // azimuth elevation
		f32	vfov =			deg(80);
		
		bool third_person = true;
		
		f32	eye_height =	1.65f;
		v3	third_person_camera_offset_cam =		v3(0.5f, -0.4f, 3);
		
		f32 collision_r =	0.4f;
		f32 collision_h =	1.7f;
		
		f32 walking_friction_alpha =	0.15f;
		
		f32 falling_ground_friction =	0.0f;
		f32 falling_bounciness =		0.25f;
		f32 falling_min_bounce_speed =	6;
		
		f32 wall_friction =				0.2f;
		f32 wall_bounciness =			0.55f;
		f32 wall_min_bounce_speed =		8;
		
		f32	jumping_up_impulse = jump_impulse_from_jump_height(1.2f, grav_accel_down);
		
		bool opt_open = true;
		void options () {
			option_group("player", &opt_open);
			if (opt_open) {
				option(		"  pos_world",				&pos_world);
				option(		"  vel_world",				&vel_world);
				option_deg(	"  ori_ae",					&ori_ae);
				option_deg(	"  vfov",					&vfov);
				
				option(		"  third_person",			&third_person);
				
				option(		"  eye_height",				&eye_height);
				option(		"  third_person_camera_offset_cam",		&third_person_camera_offset_cam);
				
				option(		"  collision_r",			&collision_r);
				option(		"  collision_h",			&collision_h);
				
				option(		"  walking_friction_alpha",	&walking_friction_alpha);
				
				option(		"  falling_ground_friction",&falling_ground_friction);
				option(		"  falling_bounciness",		&falling_bounciness);
				option(		"  falling_min_bounce_speed",	&falling_min_bounce_speed);
				
				option(		"  wall_bounciness",		&wall_bounciness);
				option(		"  wall_friction",			&wall_friction);
				option(		"  wall_min_bounce_speed",	&wall_min_bounce_speed);
				
				option(		"  jumping_up_impulse",		&jumping_up_impulse);
			}
		}
	};
	Player player;
	
	auto respawn_player = [&] () {
		player.pos_world = initial_player_pos_world;
		player.vel_world = initial_player_vel_world;
	};
	respawn_player();
	
	bool draw_debug_overlay = false;
	
	auto load_game = [&] () {
		trigger_load_game = false;
		load_struct("flycam", &flycam);
	};
	auto save_game = [&] () {
		trigger_save_game = false;
		save_struct("flycam", flycam);
	};
	
	load_game();
	
	inital_chunk( player.pos_world );
	
	flt chunk_drawing_radius =		INF;
	flt chunk_generation_radius =	500;
	
	Texture2D dbg_heightmap_visualize;
	Texture2D dbg_heightmap_multiplier_visualize;
	s32 dbg_heightmap_visualize_radius = 1000;
	
	auto regen_dbg_heightmap_visualize = [&] () {
		lrgba8* pixels = (lrgba8*)dbg_heightmap_visualize.mips[0].data;
		iv2 dim = dbg_heightmap_visualize.mips[0].dim;
		
		auto dst = [&] (iv2 pos) -> lrgba8* {
			return &pixels[pos.y*dim.x + pos.x];
		};
		
		OSN::Noise<2> noise( world_seed );
		
		iv2 i;
		for (i.y=0; i.y<dim.y; ++i.y) {
			for (i.x=0; i.x<dim.x; ++i.x) {
				v2 pos_world = map((v2)i, 0,(v2)(dim-1), -dbg_heightmap_visualize_radius / 2, +dbg_heightmap_visualize_radius / 2);
				*dst(i) = lrgba8(spectrum_gradient(map( heightmap(noise, pos_world) , 0, 45)), 255);
			}
		}
		
		dbg_heightmap_visualize.upload();
	};
	auto regen_dbg_heightmap_multiplier_visualize = [&] () {
		lrgba8* pixels = (lrgba8*)dbg_heightmap_multiplier_visualize.mips[0].data;
		iv2 dim = dbg_heightmap_multiplier_visualize.mips[0].dim;
		
		auto dst = [&] (iv2 pos) -> lrgba8* {
			return &pixels[pos.y*dim.x + pos.x];
		};
		
		OSN::Noise<2> noise( world_seed );
		
		iv2 i;
		for (i.y=0; i.y<dim.y; ++i.y) {
			for (i.x=0; i.x<dim.x; ++i.x) {
				v2 pos_world = map((v2)i, 0,(v2)(dim-1), -dbg_heightmap_visualize_radius / 2, +dbg_heightmap_visualize_radius / 2);
				*dst(i) = lrgba8(spectrum_gradient(map( heightmap_multiplier(noise, pos_world) , 0, 2)), 255);
			}
		}
		
		dbg_heightmap_multiplier_visualize.upload();
	};
	
	{
		dbg_heightmap_visualize.alloc_cpu_single_mip(PT_LRGBA8, 512);
		dbg_heightmap_multiplier_visualize.alloc_cpu_single_mip(PT_LRGBA8, 512);
		
		regen_dbg_heightmap_visualize();
		regen_dbg_heightmap_multiplier_visualize();
	}
	
	s32 max_chunks_generated_per_frame = 1;
	
	struct Overlay_Vertex {
		v3		pos_world;
		lrgba8	color;
	};
	
	Vertex_Layout overlay_vertex_layout = {
		{ "pos_world",	T_V3,	sizeof(Overlay_Vertex), offsetof(Overlay_Vertex, pos_world) },
		{ "color",		T_U8V4,	sizeof(Overlay_Vertex), offsetof(Overlay_Vertex, color) },
	};
	
	Vbo overlay_vbo;
	overlay_vbo.init(&overlay_vertex_layout);
	
	flt feet_vel_world_multiplier = 1;
	
	// 
	f64 prev_t = glfwGetTime();
	f32 avg_dt = 1.0f / 60;
	f32 avg_dt_alpha = 0.025f;
	dt = 0;
	
	bool fixed_dt = 1 || IS_DEBUGGER_PRESENT(); 
	f32 max_variable_dt = 1.0f / 20; 
	f32 fixed_dt_dt = 1.0f / 60; 
	
	inp.get_non_callback_input(); // get inital mouse pos so that we dont get a invalid mouse delta on the first frame (glfw does not support relative mouse input for some reason)
	
	for (frame_i=0;; ++frame_i) {
		//printf("frame: %d\n", frame_i);
		
		if (frame_i == 0) { // Timestep
			dbg_assert(dt == 0);
		} else {
			if (fixed_dt) {
				dt = fixed_dt_dt;
			} else {
				dt = min(dt, max_variable_dt);
			}
		}
		
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
		opt_toggle_open = false;
		opt_value_edit_flag = false;
		
		glfwPollEvents();
		
		inp.get_non_callback_input();
		
		begin_options();
		
		overlay_line(prints("mouse:   %4d %4d -> %.2f %.2f", inp.mcursor_pos_px.x,inp.mcursor_pos_px.y, mouse.x,mouse.y));
		
		{ // various options
			option("fixed_dt",			&fixed_dt);
			option("max_variable_dt",	&max_variable_dt);
			option("fixed_dt_dt",		&fixed_dt_dt);
			
			option("viewing_flycam",	&viewing_flycam);
			option("controling_flycam",	&controling_flycam);
			
			flycam.options();
			
			option("initial_player_pos_world",		&initial_player_pos_world);
			option("initial_player_vel_world",		&initial_player_vel_world);
			
			player.options();
			
			option("unloaded_chunks_traversable",	&unloaded_chunks_traversable);
			if (option("unloaded_chunks_dark",		&B_NO_CHUNK.dark)) for (auto& c : chunks) c.second->needs_remesh = true;
		}
		
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
			
			if (controling_flycam) {
				v2 mouse_look_sens = v2(deg(1.0f / 8)) * (flycam.vfov / deg(70));
				
				flycam.ori_ae = clamped_cam_ae(flycam.ori_ae, mouse_look_sens);
			} else {
				v2 mouse_look_sens = v2(deg(1.0f / 8)) * (player.vfov / deg(70));
				
				player.ori_ae = clamped_cam_ae(player.ori_ae, mouse_look_sens);
			}
			
			if (viewing_flycam) {
				view.vfov = flycam.vfov;
				view.ori_ae = flycam.ori_ae;
			} else {
				view.vfov = player.vfov;
				view.ori_ae = player.ori_ae;
			}
			
			world_to_cam_rot = rotate3_X(-view.ori_ae.y) * rotate3_Z(-view.ori_ae.x);
			cam_to_world_rot = rotate3_Z(view.ori_ae.x) * rotate3_X(view.ori_ae.y);
		}
		
		option("heightmap_noise_octaves", get_heightmap_noise_octaves_count, set_heightmap_noise_octaves_count, &heightmap_noise_octaves_open);
		if (heightmap_noise_octaves_open) {
			for (s32 i=0; i<(s32)heightmap_noise_octaves.size(); ++i) {
				auto& o = heightmap_noise_octaves[i];
				
				v2 tmp = v2(o.period, o.amp);
				
				if (option(prints("  [%2d]", i), &tmp)) trigger_regen_chunks = true;
				
				o.period = tmp.x;	o.amp = tmp.y;
			}
		}
		
		option("heightmap_multiplier_noise_octaves", get_heightmap_multiplier_noise_octaves_count, set_heightmap_multiplier_noise_octaves_count, &heightmap_multiplier_noise_octaves_open);
		if (heightmap_multiplier_noise_octaves_open) {
			for (s32 i=0; i<(s32)heightmap_multiplier_noise_octaves.size(); ++i) {
				auto& o = heightmap_multiplier_noise_octaves[i];
				
				v2 tmp = v2(o.period, o.amp);
				
				if (option(prints("  [%2d]", i), &tmp)) trigger_regen_chunks = true;
				
				o.period = tmp.x;	o.amp = tmp.y;
			}
		}
		
		if (option("noise_tree_freq", &noise_tree_freq))	trigger_regen_chunks = true;
		if (option("noise_tree_amp", &noise_tree_amp))		trigger_regen_chunks = true;
		
		if (trigger_regen_chunks) { // chunks currently being processed by thread pool will not be regenerated
			trigger_regen_chunks = false;
			chunks.clear();
			inital_chunk(player.pos_world);
			trigger_dbg_heightmap_visualize = true;
		}
		
		option("chunk_drawing_radius", &chunk_drawing_radius);
		option("chunk_generation_radius", &chunk_generation_radius);
		
		if (option("dbg_heightmap_visualize_radius", &dbg_heightmap_visualize_radius)) trigger_dbg_heightmap_visualize = true;
		
		if (trigger_dbg_heightmap_visualize) {
			regen_dbg_heightmap_visualize();
			regen_dbg_heightmap_multiplier_visualize();
			trigger_dbg_heightmap_visualize = false;
		}
		
		if (trigger_respawn_player) {
			trigger_respawn_player = false;
			respawn_player();
		}
		
		{ // chunk generation
			// check all chunk positions within a square of chunk_generation_radius
			chunk_pos_t start =	(chunk_pos_t)floor(	(player.pos_world.xy() -chunk_generation_radius) / (v2)CHUNK_DIM.xy() );
			chunk_pos_t end =	(chunk_pos_t)ceil(	(player.pos_world.xy() +chunk_generation_radius) / (v2)CHUNK_DIM.xy() );
			
			// check their actual distance to determine if they should be generated or not
			auto chunk_dist_to_player = [&] (chunk_pos_t pos) {
				bpos2 chunk_origin = pos * CHUNK_DIM.xy();
				return point_square_nearest_dist(chunk_origin, CHUNK_DIM.xy(), player.pos_world.xy());
			};
			auto chunk_is_in_generation_radius = [&] (chunk_pos_t pos) {
				return chunk_dist_to_player(pos) <= chunk_generation_radius;
			};
			
			std::vector<chunk_pos_t> chunks_to_generate;
			
			chunk_pos_t cp;
			for (cp.x = start.x; cp.x<end.x; ++cp.x) {
				for (cp.y = start.y; cp.y<end.y; ++cp.y) {
					if (chunk_is_in_generation_radius(cp) && !query_chunk(cp)) {
						// chunk is within chunk_generation_radius and not yet generated
						chunks_to_generate.push_back(cp);
					}
				}
			}
			
			std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
				[&] (chunk_pos_t l, chunk_pos_t r) { return chunk_dist_to_player(l) < chunk_dist_to_player(r); }
			);
			
			PROFILE_BEGIN(generate_new_chunk);
			
			int count = 0;
			for (auto& cp : chunks_to_generate) {
				generate_new_chunk(cp);
				if (++count == max_chunks_generated_per_frame) break;
			}
			if (count != 0) PROFILE_END_PRINT(generate_new_chunk, "frame: %3d generated %d chunks", frame_i, count);
			
		}
		
		overlay_line(prints("chunks in ram:   %4d %6d MB (%d KB per chunk) chunk is %dx%dx%d blocks", (s32)chunks.size(), (s32)(sizeof(Chunk)*chunks.size()/1024/1024), (s32)(sizeof(Chunk)/1024), (s32)CHUNK_DIM.x,(s32)CHUNK_DIM.y,(s32)CHUNK_DIM.z));
		
		overlay_vbo.clear();
		
		if (controling_flycam) { // camera view position
			f32 cam_speed_forw = flycam.speed;
			if (inp.move_fast) cam_speed_forw *= flycam.speed_fast_mul;
			
			v3 cam_vel = cam_speed_forw * v3(1,1,1);
			
			v3 cam_vel_cam = normalize_or_zero( (v3)iv3(inp.move_dir.x, inp.move_dir.z, -inp.move_dir.y) ) * cam_vel;
			flycam.pos_world += (cam_to_world_rot * cam_vel_cam) * dt;
			
			//printf(">>> %f %f %f\n", cam_vel_cam.x, cam_vel_cam.y, cam_vel_cam.z);
		}
		
		{ // player position (collision and movement dynamics)
			constexpr f32 COLLISION_SEPERATION_EPSILON = 0.001f;
			
			v3 pos_world = player.pos_world;
			v3 vel_world = player.vel_world;
			
			// 
			bool player_stuck_in_solid_block;
			bool player_on_ground;
			
			auto check_blocks_around_player = [&] () {
				{ // for all blocks we could be touching
					bpos start =	(bpos)floor(pos_world -v3(player.collision_r,player.collision_r,0));
					bpos end =		(bpos)ceil(pos_world +v3(player.collision_r,player.collision_r,player.collision_h));
					
					bool any_intersecting = false;
					
					bpos bp;
					for (bp.z=start.z; bp.z<end.z; ++bp.z) {
						for (bp.y=start.y; bp.y<end.y; ++bp.y) {
							for (bp.x=start.x; bp.x<end.x; ++bp.x) {
								
								auto* b = query_block(bp);
								bool block_solid = !bt_is_traversable(b->type);
								
								bool intersecting = cylinder_cube_intersect(pos_world -(v3)bp, player.collision_r,player.collision_h);
								
								if (0) {
									lrgba8 col;
									
									if (!block_solid) {
										col = lrgba8(40,40,40,100);
									} else {
										col = intersecting ? lrgba8(255,40,40,200) : lrgba8(255,255,255,150);
									}
									
									Overlay_Vertex* out = (Overlay_Vertex*)&*vector_append(&overlay_vbo.vertecies, sizeof(Overlay_Vertex)*6);
									
									*out++ = { (v3)bp +v3(+1, 0,+1.01f), col };
									*out++ = { (v3)bp +v3(+1,+1,+1.01f), col };
									*out++ = { (v3)bp +v3( 0, 0,+1.01f), col };
									*out++ = { (v3)bp +v3( 0, 0,+1.01f), col };
									*out++ = { (v3)bp +v3(+1,+1,+1.01f), col };
									*out++ = { (v3)bp +v3( 0,+1,+1.01f), col };
								}
								
								any_intersecting = any_intersecting || (intersecting && block_solid);
							}
						}
					}
					
					player_stuck_in_solid_block = any_intersecting; // player somehow ended up inside a block
				}
				
				{ // for all blocks we could be standing on
					
					bpos_t pos_z = floor(pos_world.z);
					
					player_on_ground = false;
					
					if ((pos_world.z -pos_z) <= COLLISION_SEPERATION_EPSILON*1.05f && vel_world.z == 0) {
						
						bpos2 start =	(bpos2)floor(pos_world.xy() -player.collision_r);
						bpos2 end =		(bpos2)ceil(pos_world.xy() +player.collision_r);
						
						bpos bp;
						bp.z = pos_z -1;
						
						for (bp.y=start.y; bp.y<end.y; ++bp.y) {
							for (bp.x=start.x; bp.x<end.x; ++bp.x) {
								
								auto* b = query_block(bp);
								bool block_solid = !bt_is_traversable(b->type);
								
								if (block_solid && circle_square_intersect(pos_world.xy() -(v2)bp.xy(), player.collision_r)) player_on_ground = true;
							}
						}
					}
				}
				
			};
			check_blocks_around_player();
			
			overlay_line(prints(">>> player_on_ground: %d\n", player_on_ground));
			
			if (player_stuck_in_solid_block) {
				vel_world = 0;
				printf(">>>>>>>>>>>>>> stuck!\n");
			} else {
				
				if (!controling_flycam) {
					{ // player walking dynamics
						v2 player_walk_speed = 3 * (inp.move_fast ? 3 : 1);
						
						v2 feet_vel_world = rotate2(player.ori_ae.x) * (normalize_or_zero( (v2)inp.move_dir.xy() ) * player_walk_speed);
						
						option("feet_vel_world_multiplier", &feet_vel_world_multiplier);
						
						feet_vel_world *= feet_vel_world_multiplier;
						
						// need some proper way of doing walking dynamics
						
						if (player_on_ground) {
							vel_world = v3( lerp(vel_world.xy(), feet_vel_world, player.walking_friction_alpha), vel_world.z );
						} else {
							v3 tmp = v3( lerp(vel_world.xy(), feet_vel_world, player.walking_friction_alpha), vel_world.z );
							
							if (length(vel_world.xy()) < length(player_walk_speed)*0.5f) vel_world = tmp; // only allow speeding up to slow speed with air control
						}
						
						if (length(vel_world) < 0.01f) vel_world = 0;
					}
					
					if (jump_held && player_on_ground) vel_world += v3(0,0, player.jumping_up_impulse);
				}
				
				vel_world += v3(0,0, -grav_accel_down) * dt;
			}
			
			option("draw_debug_overlay", &draw_debug_overlay);
			
			auto trace_player_collision_path = [&] () {
				f32 player_r = player.collision_r;
				f32 player_h = player.collision_h;
				
				f32 t_remain = dt;
				
				bool draw_dbg = draw_debug_overlay; // so that i only draw the debug block overlay once

				while (t_remain > 0) {
					
					struct {
						f32 dist = +INF;
						v3 hit_pos;
						v3 normal; // normal of surface/edge we collided with, the player always collides with the outside of the block since we assume the player can never be inside a block if we're doing this raycast
					} earliest_collision;
					
					auto find_earliest_collision_with_block_by_raycast_minkowski_sum = [&] (bpos bp) {
						bool hit = false;
						
						auto* b = query_block(bp);
						bool block_solid = !bt_is_traversable(b->type);
						
						if (block_solid) {
							
							v3 local_origin = (v3)bp;
							
							v3 pos_local = pos_world -local_origin;
							v3 vel = vel_world;
							
							auto collision_found = [&] (f32 hit_dist, v3 hit_pos_local, v3 normal_world) {
								if (hit_dist < earliest_collision.dist) {
									v3 hit_pos_world = hit_pos_local +local_origin;
									
									earliest_collision.dist =		hit_dist;
									earliest_collision.hit_pos =	hit_pos_world;
									earliest_collision.normal =		normal_world;
								}
							};
							
							// this geometry we are raycasting our player position onto represents the minowski sum of the block and our players cylinder
							
							auto raycast_x_side = [&] (v3 ray_pos, v3 ray_dir, f32 plane_x, f32 normal_x) { // side forming yz plane
								if (ray_dir.x == 0 || (ray_dir.x * (plane_x -ray_pos.x)) < 0) return false; // ray parallel to plane or ray points away from plane
								
								f32 delta_x = plane_x -ray_pos.x;
								v2 delta_yz = delta_x * (v2(ray_dir.y,ray_dir.z) / ray_dir.x);

								v2 hit_pos_yz = v2(ray_pos.y,ray_pos.z) + delta_yz;

								f32 hit_dist = length(v3(delta_x, delta_yz[0], delta_yz[1]));
								if (!all(hit_pos_yz > v2(0,-player_h) && hit_pos_yz < 1)) return false;
								
								collision_found(hit_dist, v3(plane_x, hit_pos_yz[0], hit_pos_yz[1]), v3(normal_x,0,0));
								return true;
							};
							auto raycast_y_side = [&] (v3 ray_pos, v3 ray_dir, f32 plane_y, f32 normal_y) { // side forming xz plane
								if (ray_dir.y == 0 || (ray_dir.y * (plane_y -ray_pos.y)) < 0) return false; // ray parallel to plane or ray points away from plane
								
								f32 delta_y = plane_y -ray_pos.y;
								v2 delta_xz = delta_y * (v2(ray_dir.x,ray_dir.z) / ray_dir.y);
								
								v2 hit_pos_xz = v2(ray_pos.x,ray_pos.z) +delta_xz;
								
								f32 hit_dist = length(v3(delta_xz[0], delta_y, delta_xz[1]));
								if (!all(hit_pos_xz > v2(0,-player_h) && hit_pos_xz < 1)) return false;
								
								collision_found(hit_dist, v3(hit_pos_xz[0], plane_y, hit_pos_xz[1]), v3(0,normal_y,0));
								return true;
							};
							
							auto raycast_sides_edge = [&] (v3 ray_pos, v3 ray_dir, v2 cyl_pos2d, f32 cyl_r, f32 cyl_z_l,f32 cyl_z_h) { // edge between block sides which are cylinders in our minowski sum
								// do 2d circle raycase using on xy plane
								f32 ray_dir2d_len = length(ray_dir.xy());
								if (ray_dir2d_len == 0) return false; // ray parallel to cylinder
								v2 unit_ray_dir2d = ray_dir.xy() / ray_dir2d_len;
								
								v2 circ_rel_p = cyl_pos2d -ray_pos.xy();
								
								f32 closest_p_dist = dot(unit_ray_dir2d, circ_rel_p);
								v2 closest_p = unit_ray_dir2d * closest_p_dist;
								
								v2 circ_to_closest = closest_p -circ_rel_p;
								
								f32 r_sqr = cyl_r*cyl_r;
								f32 dist_sqr = length_sqr(circ_to_closest);
								
								if (dist_sqr >= r_sqr) return false; // ray does not cross cylinder
								
								f32 chord_half_length = sqrt( r_sqr -dist_sqr );
								f32 closest_hit_dist2d = closest_p_dist -chord_half_length;
								f32 furthest_hit_dist2d = closest_p_dist +chord_half_length;
								
								f32 hit_dist2d;
								if (closest_hit_dist2d >= 0)		hit_dist2d = closest_hit_dist2d;
								else if (furthest_hit_dist2d >= 0)	hit_dist2d = furthest_hit_dist2d;
								else								return false; // circle hit is on backwards direction of ray, ie. no hit
								
								v2 rel_hit_xy = hit_dist2d * unit_ray_dir2d;
								
								// calc hit z
								f32 rel_hit_z = length(rel_hit_xy) * (ray_dir.z / ray_dir2d_len);
								
								v3 rel_hit_pos = v3(rel_hit_xy, rel_hit_z);
								v3 hit_pos = ray_pos +rel_hit_pos;
								
								if (!(hit_pos.z > cyl_z_l && hit_pos.z < cyl_z_h)) return false;
								
								collision_found(length(rel_hit_pos), hit_pos, v3(normalize(rel_hit_xy -circ_rel_p), 0));
								return true;
							};
							
							auto raycast_cap = [&] (v3 ray_pos, v3 ray_dir, f32 plane_z, f32 normal_z) {
								// normal axis aligned plane raycast
								f32 delta_z = plane_z -ray_pos.z;
								
								if (ray_dir.z == 0 || (ray_dir.z * delta_z) < 0) return false; // if ray parallel to plane or ray points away from plane
								
								v2 delta_xy = delta_z * (ray_dir.xy() / ray_dir.z);
								
								v2 plane_hit_xy = ray_pos.xy() +delta_xy;
								
								// check if cylinder base/top circle cap intersects with block top/bottom square
								v2 closest_p = clamp(plane_hit_xy, 0,1);
								
								f32 dist_sqr = length_sqr(closest_p -plane_hit_xy);
								if (dist_sqr >= player_r*player_r) return false; // hit outside
								
								f32 hit_dist = length(v3(delta_xy, delta_z));
								collision_found(hit_dist, v3(plane_hit_xy, plane_z), v3(0,0, normal_z));
								return true;
							};
							
							hit = raycast_cap(			pos_local, vel, 1,			+1) || hit; // block top
							hit = raycast_cap(			pos_local, vel, -player_h,	-1) || hit; // block bottom
							
							hit = raycast_x_side(		pos_local, vel, 0 -player_r, -1 ) || hit;
							hit = raycast_x_side(		pos_local, vel, 1 +player_r, +1 ) || hit;
							hit = raycast_y_side(		pos_local, vel, 0 -player_r, -1 ) || hit;
							hit = raycast_y_side(		pos_local, vel, 1 +player_r, +1 ) || hit;
							
							hit = raycast_sides_edge(	pos_local, vel, v2( 0, 0), player_r, -player_h, 1 ) || hit;
							hit = raycast_sides_edge(	pos_local, vel, v2( 0,+1), player_r, -player_h, 1 ) || hit;
							hit = raycast_sides_edge(	pos_local, vel, v2(+1, 0), player_r, -player_h, 1 ) || hit;
							hit = raycast_sides_edge(	pos_local, vel, v2(+1,+1), player_r, -player_h, 1 ) || hit;
							
						}
						
						lrgba8 col;
						
						if (!block_solid) {
							col = lrgba8(40,40,40,100);
						} else {
							col = hit ? lrgba8(255,40,40,200) : lrgba8(255,255,255,150);
						}
						
						if (draw_dbg) {
							Overlay_Vertex* out = (Overlay_Vertex*)&*vector_append(&overlay_vbo.vertecies, sizeof(Overlay_Vertex)*6);
							
							*out++ = { (v3)bp +v3(+1, 0,+1.01f), col };
							*out++ = { (v3)bp +v3(+1,+1,+1.01f), col };
							*out++ = { (v3)bp +v3( 0, 0,+1.01f), col };
							*out++ = { (v3)bp +v3( 0, 0,+1.01f), col };
							*out++ = { (v3)bp +v3(+1,+1,+1.01f), col };
							*out++ = { (v3)bp +v3( 0,+1,+1.01f), col };
						}
					};
					
					if (length_sqr(vel_world) != 0) {
						// for all blocks we could be touching during movement by at most one block on each axis
						bpos start =	(bpos)floor(pos_world -v3(player_r,player_r,0)) -1;
						bpos end =		(bpos)ceil(pos_world +v3(player_r,player_r,player_h)) +1;
						
						bpos bp;
						for (bp.z=start.z; bp.z<end.z; ++bp.z) {
							for (bp.y=start.y; bp.y<end.y; ++bp.y) {
								for (bp.x=start.x; bp.x<end.x; ++bp.x) {
									find_earliest_collision_with_block_by_raycast_minkowski_sum(bp);
								}
							}
						}
					}
					
					//printf(">>> %f %f,%f %f,%f\n", earliest_collision.dist, pos_world.x,pos_world.y, earliest_collision.hit_pos.x,earliest_collision.hit_pos.y);
					
					f32 max_dt = min(t_remain, 1.0f / max_component(abs(vel_world))); // if we are moving so fast that we would move by more than one block on any one axis we will do sub steps of exactly one block
					
					f32 earliest_collision_t = earliest_collision.dist / length(vel_world); // inf if there is no collision
					
					if (earliest_collision_t >= max_dt) {
						pos_world += vel_world * max_dt;
						t_remain -= max_dt;
					} else {
						
						// handle block collision
						f32 friction;
						f32 bounciness;
						f32 min_bounce_speed;
						
						if (earliest_collision.normal.z == +1) {
							// hit top of block ie. ground
							friction = player.falling_ground_friction;
							bounciness = player.falling_bounciness;
							min_bounce_speed = player.falling_min_bounce_speed;
						} else {
							// hit side of block or bottom of block ie. wall or ceiling
							friction = player.wall_friction;
							bounciness = player.wall_bounciness;
							min_bounce_speed = player.wall_min_bounce_speed;
						}
						
						v3 normal = earliest_collision.normal;
						f32 norm_speed = dot(normal, vel_world); // normal points out of the wall
						v3 norm_vel = normal * norm_speed;
						
						v3 frict_vel = vel_world -norm_vel;
						frict_vel.z = 0; // do not apply friction on vertical movement
						f32 frict_speed = length(frict_vel);
						
						v3 remain_vel = vel_world -norm_vel -frict_vel;
						
						if (frict_speed != 0) {
							v3 frict_dir = frict_vel / frict_speed;
							
							f32 friction_dv = friction * max(-norm_speed, 0.0f); // change in speed due to kinetic friction (unbounded ie. can be larger than our actual velocity)
							frict_vel -= frict_dir * min(friction_dv, frict_speed);
						}
						
						norm_vel = bounciness * -norm_vel;
						
						if (length(norm_vel) <= min_bounce_speed) norm_vel = 0;
						
						vel_world = v3(norm_vel +frict_vel +remain_vel);
						
						pos_world = earliest_collision.hit_pos;
						
						pos_world += v3(earliest_collision.normal * COLLISION_SEPERATION_EPSILON); // move player a epsilon distance away from the wall to prevent problems, having this value be 1/1000 instead of sothing very samll prevents small intersection problems that i don't want to debug now
						
						t_remain -= earliest_collision_t;
					}

					draw_dbg = false;
				}
			};
			trace_player_collision_path();
			
			player.vel_world = vel_world;
			player.pos_world = pos_world;
		}
		
		if (viewing_flycam) {
			view.pos_world = flycam.pos_world;
		} else {
			view.pos_world = player.pos_world +v3(0,0,player.eye_height);
			if (player.third_person) view.pos_world += cam_to_world_rot * player.third_person_camera_offset_cam;
		}
		
		view.calc_final_matricies(world_to_cam_rot, cam_to_world_rot);
		
		Block*	highlighted_block;
		bpos	highlighted_block_pos;
		block_face_e highlighted_block_face;
		auto raycast_highlighted_block = [&] () {
			auto raycast_block = [&] (v3 ray_pos, v3 ray_dir, flt max_dist, bpos* hit_block, block_face_e* hit_face) -> Block* {
				
				bpos step_delta = bpos(	(bpos_t)normalize(ray_dir.x),
										(bpos_t)normalize(ray_dir.y),
										(bpos_t)normalize(ray_dir.z) );
									
									
				v3 step = v3(		length(ray_dir / abs(ray_dir.x)),
									length(ray_dir / abs(ray_dir.y)),
									length(ray_dir / abs(ray_dir.z)) );
				
				v3 ray_pos_floor = floor(ray_pos);
				
				v3 pos_in_block = ray_pos -ray_pos_floor;
				
				v3 next = step * select(ray_dir > 0, 1 -pos_in_block, pos_in_block);
				next = select(ray_dir != 0, next, INF);
				
				auto find_next_axis = [&] (v3 next) {
					if (		next.x < next.y && next.x < next.z )	return 0;
					else if (	next.y < next.z )						return 1;
					else												return 2;
				};
				
				bpos cur_block = (bpos)ray_pos_floor;
				
				int first_axis = find_next_axis(next);
				block_face_e face = (block_face_e)(first_axis*2 +(step_delta[first_axis] > 0 ? 1 : 0));
				
				for (;;) {
					
					//highlight_block(cur_block);
					Block* b = query_block(cur_block);
					if (bt_is_breakable(b->type)) {
						*hit_block = cur_block;
						*hit_face = face;
						return b;
					}
					
					int axis = find_next_axis(next);
					
					face = (block_face_e)(axis*2 +(step_delta[axis] < 0 ? 1 : 0));
					
					if (next[axis] > max_dist) return nullptr;
					
					next[axis] += step[axis];
					cur_block[axis] += step_delta[axis];
				}
				
			};
			
			v3 dir = rotate3_Z(player.ori_ae.x) * rotate3_X(player.ori_ae.y) * v3(0,0,-1);
			
			highlighted_block = raycast_block(player.pos_world +v3(0,0,player.eye_height), dir, 3.5f, &highlighted_block_pos, &highlighted_block_face);
		};
		raycast_highlighted_block();
		
		if (highlighted_block && hold_break_block) { // block breaking
			
			Chunk* chunk;
			Block* b = query_block(highlighted_block_pos, &chunk);
			dbg_assert( bt_is_breakable(b->type) && chunk );
			
			b->hp_ratio -= 1.0f / 0.3f * dt;
			
			if (b->hp_ratio > 0) {
				chunk->block_only_hp_changed(highlighted_block_pos);
			} else {
				
				b->hp_ratio = 0;
				b->type = BT_AIR;
				
				chunk->block_changed(highlighted_block_pos);
				
				dbg_play_sound();
			}
		}
		{ // block placing
			bool block_place_is_inside_player = false;
			
			if (highlighted_block && trigger_place_block) {
				
				bpos dir = 0;
				dir[highlighted_block_face / 2] = highlighted_block_face % 2 ? +1 : -1;
				
				bpos block_place_pos = highlighted_block_pos +dir;
				
				Chunk* chunk;
				Block* b = query_block(block_place_pos, &chunk);
				
				block_place_is_inside_player = cylinder_cube_intersect(player.pos_world -(v3)block_place_pos, player.collision_r,player.collision_h);
				
				if (bt_is_replaceable(b->type) && !block_place_is_inside_player) { // could be BT_NO_CHUNK or BT_OUT_OF_BOUNDS or BT_AIR 
					
					b->type = BT_EARTH;
					b->hp_ratio = 1;
					b->dbg_tint = 255;
					
					dbg_play_sound();
					
					chunk->block_changed(block_place_pos);
					
					raycast_highlighted_block(); // make highlighted_block seem more responsive
				}
			}
			
			trigger_place_block = trigger_place_block && block_place_is_inside_player; // if we tried to place a block inside the player try again next frame as long as RMB is held down (releasing RMB will set trigger_place_block to false anyway)
		}
		
		//// Draw
		for (auto* s : shaders) { // set common uniforms
			if (s->valid()) {
				s->bind();
				s->set_unif("screen_dim", (v2)inp.wnd_dim);
				s->set_unif("mcursor_pos", inp.bottom_up_mcursor_pos());
			}
		}
		
		glViewport(0,0, inp.wnd_dim.x,inp.wnd_dim.y);
		
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
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
		
		if (shad_main->valid()) {
			
			glEnable(GL_BLEND);
			
			bind_texture_unit(0, &tex_block_atlas);
			bind_texture_unit(1, &tex_breaking);
			
			shad_main->bind();
			shad_main->set_unif("world_to_cam",	view.world_to_cam.m4());
			shad_main->set_unif("cam_to_world",	view.cam_to_world.m4());
			shad_main->set_unif("cam_to_clip",	view.cam_to_clip);
			
			shad_main->set_unif("texture_res", texture_res);
			shad_main->set_unif("atlas_textures_count", atlas_textures_count);
			shad_main->set_unif("breaking_frames_count", breaking_frames_count);
			
			s32 count = 0;
			
			PROFILE_BEGIN(update_block_brighness);
			// update_block_brighness first because remesh accesses surrounding chunks for blocks on the edge
			for (auto& chunk_hash_pair : chunks) {
				auto& chunk = chunk_hash_pair.second;
				
				if (chunk->needs_block_brighness_update) {
					chunk->update_block_brighness();
					++count;
				}
			}
			if (count != 0) PROFILE_END_PRINT(update_block_brighness, "frame: %3d count: %d", frame_i, count);
			
			count = 0;
			
			PROFILE_BEGIN(remesh_upload);
			// remesh and draw
			for (auto& chunk_hash_pair : chunks) {
				auto& chunk = chunk_hash_pair.second;
				
				if (chunk->needs_remesh) {
					chunk->remesh();
					chunk->vbo.upload();
					chunk->vbo_transperant.upload();
					++count;
				}
			}
			if (count != 0) PROFILE_END_PRINT(remesh_upload, "frame: %3d count: %d", frame_i, count);
			
			// draw opaque
			for (auto& chunk_hash_pair : chunks) {
				auto& chunk = chunk_hash_pair.second;
				
				chunk->vbo.draw_entire(shad_main);
			}
			
			// draw transperant
			for (auto& chunk_hash_pair : chunks) {
				auto& chunk = chunk_hash_pair.second;
				
				chunk->vbo_transperant.draw_entire(shad_main);
			}
			
			glDisable(GL_BLEND);
		}
		
		if (shad_overlay->valid()) {
			shad_overlay->bind();
			
			shad_overlay->set_unif("world_to_cam",	view.world_to_cam.m4());
			shad_overlay->set_unif("cam_to_world",	view.cam_to_world.m4());
			shad_overlay->set_unif("cam_to_clip",	view.cam_to_clip);
			
			//bool checker = false;
			
			auto highlight_block = [&] (bpos block) {
				s32 cylinder_sides = 32;
				
				#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
				
				#if 1
				lrgba8 col = lrgba8(255,255,255,60);
				lrgba8 side_col = lrgba8(255,255,255,120);
				
				f32 r = 1.01f;
				f32 inset = 1.0f / 50;
				
				f32 side_r = r * 0.06f;
				#else
				lrgba8 col = lrgba8(255,255,255,140);
				lrgba8 side_col = lrgba8(255,255,255,60);
				
				f32 r = 1.01f;
				f32 inset = 1.0f / 17;
				
				f32 side_r = r * 0.35f;
				#endif
				
				s32 face_quads = 4;
				s32 quad_vertecies = 6;
				
				for (block_face_e face=(block_face_e)0; face<(block_face_e)6; ++face) {
					
					f32 up;
					f32 horiz;
					switch (face) {
						case BF_NEG_X:	horiz = 0;	up = -1;	break;
						case BF_NEG_Y:	horiz = 1;	up = -1;	break;
						case BF_POS_X:	horiz = 2;	up = -1;	break;
						case BF_POS_Y:	horiz = 3;	up = -1;	break;
						case BF_NEG_Z:	horiz = 0;	up = -2;	break;
						case BF_POS_Z:	horiz = 0;	up = 0;		break;
					}
					
					m3 rot_up =		rotate3_Y( deg(90) * up );
					m3 rot_horiz =	rotate3_Z( deg(90) * horiz );
					
					{
						Overlay_Vertex* out = (Overlay_Vertex*)&*vector_append(&overlay_vbo.vertecies,
							sizeof(Overlay_Vertex)*face_quads*quad_vertecies);
						
						for (s32 edge=0; edge<face_quads; ++edge) {
							
							m2 rot_edge = rotate2(deg(90) * -edge);
							
							auto vert = [&] (v3 v, lrgba8 col) {
								*out++ = { (v3)block +((rot_horiz * rot_up * rot_edge * v) * 0.5f +0.5f), col };
							};
							auto quad = [&] (v3 a, v3 b, v3 c, v3 d, lrgba8 col) {
								vert(a, col);	vert(b, col);	vert(d, col);
								vert(d, col);	vert(b, col);	vert(c, col);
							};
							
							// emit block highlight
							quad(	v3(-r,-r,+r),
									v3(+r,-r,+r),
									v3(+r,-r,+r) +v3(-inset,+inset,0)*2,
									v3(-r,-r,+r) +v3(+inset,+inset,0)*2,
									col);
							
						}
					}
					
					{
						Overlay_Vertex* out = (Overlay_Vertex*)&*vector_append(&overlay_vbo.vertecies,
							sizeof(Overlay_Vertex)*quad_vertecies);
						
						auto vert = [&] (v3 v, lrgba8 col) {
							*out++ = { (v3)block +((rot_horiz * rot_up * v) * 0.5f +0.5f), col };
						};
						auto quad = [&] (v3 a, v3 b, v3 c, v3 d, lrgba8 col) {
							vert(a, col);	vert(b, col);	vert(d, col);
							vert(d, col);	vert(b, col);	vert(c, col);
						};
						
						if (face == highlighted_block_face) { // emit face highlight
							quad(	v3(-side_r,-side_r,+r),
									v3(+side_r,-side_r,+r),
									v3(+side_r,+side_r,+r),
									v3(-side_r,+side_r,+r),
									side_col );
						}
					}
				}
				
				#undef QUAD
			};
			
			if (highlighted_block) highlight_block(highlighted_block_pos);
			
			{ // block visualize
				glDisable(GL_CULL_FACE);
				glEnable(GL_BLEND);
				//glDisable(GL_DEPTH_TEST);
				
				overlay_vbo.upload();
				overlay_vbo.draw_entire(shad_overlay);
				
				//glEnable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
				glEnable(GL_CULL_FACE);
			}
			
			overlay_vbo.clear();
			
			{ // player collision cylinder
				v3 pos_world = player.pos_world;
				f32 r = player.collision_r;
				f32 h = player.collision_h;
				
				s32 cylinder_sides = 32;
				
				Overlay_Vertex* out = (Overlay_Vertex*)&*vector_append(&overlay_vbo.vertecies, sizeof(Overlay_Vertex)*(3+6+3)*cylinder_sides);
				
				lrgba8 col = lrgba8(255, 40, 255, 230);
				
				v2 rv = v2(r,0);
				
				for (s32 i=0; i<cylinder_sides; ++i) {
					f32 rot_a = (f32)(i +0) / (f32)cylinder_sides * deg(360);
					f32 rot_b = (f32)(i +1) / (f32)cylinder_sides * deg(360);
					
					m2 ma = rotate2(rot_a);
					m2 mb = rotate2(rot_b);
					
					*out++ = { pos_world +v3(0,0,     h), col };
					*out++ = { pos_world +v3(ma * rv, h), col };
					*out++ = { pos_world +v3(mb * rv, h), col };
					
					*out++ = { pos_world +v3(mb * rv, 0), col };
					*out++ = { pos_world +v3(mb * rv, h), col };
					*out++ = { pos_world +v3(ma * rv, 0), col };
					*out++ = { pos_world +v3(ma * rv, 0), col };
					*out++ = { pos_world +v3(mb * rv, h), col };
					*out++ = { pos_world +v3(ma * rv, h), col };
					
					*out++ = { pos_world +v3(0,0,     0), col };
					*out++ = { pos_world +v3(mb * rv, 0), col };
					*out++ = { pos_world +v3(ma * rv, 0), col };
				}
			}
			
			{
				glEnable(GL_BLEND);
				//glDisable(GL_DEPTH_TEST);
				
				overlay_vbo.upload();
				overlay_vbo.draw_entire(shad_overlay);
				
				//glEnable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
			}
		}
		
		{ // texture debug overlays
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
			
			if (shad_overlay_tex->valid()) {
				//draw_overlay_tex2d(&tex_block_atlas, LR, 8);
				//draw_overlay_tex2d(&tex_breaking, UL, 8);
				//draw_overlay_tex2d(&dbg_heightmap_multiplier_visualize, UR, 1);
				//draw_overlay_tex2d(&dbg_heightmap_visualize, LR, 1.5f);
			}
			
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
