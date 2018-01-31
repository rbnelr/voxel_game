#include <cstdio>
#include <array>
#include <vector>
#include <string>

#include "types.hpp"
#include "lang_helpers.hpp"
#include "math.hpp"
#include "vector/vector.hpp"

typedef s32v2	iv2;
typedef s32v3	iv3;
typedef s32v4	iv4;
typedef u32v2	uv2;
typedef u32v3	uv3;
typedef u32v4	uv4;
typedef fv2		v2;
typedef fv3		v3;
typedef fv4		v4;
typedef fm2		m2;
typedef fm3		m3;
typedef fm4		m4;
typedef fhm		hm;

struct Interpolator_Key {
	f32	range_begin;
	v3	col;
};
v3 interpolate (f32 val, Interpolator_Key* keys, s32 keys_count) {
	dbg_assert(keys_count >= 1);
	
	s32 i=0;
	for (; i<keys_count; ++i) {
		if (val < keys[i].range_begin) break;
	}
	
	if (i == 0) { // val is lower than the entire range
		return keys[0].col;
	} else if (i == keys_count) { // val is higher than the entire range
		return keys[i -1].col;
	}
	
	dbg_assert(keys_count >= 2 && i < keys_count);
	
	auto& a = keys[i -1];
	auto& b = keys[i];
	return lerp(a.col, b.col, map(val, a.range_begin, b.range_begin));
}

static Interpolator_Key _incandescent_gradient_keys[] = {
	{ 0,		srgb(0)			},
	{ 0.3333f,	srgb(138,0,0)	},
	{ 0.6667f,	srgb(255,255,0)	},
	{ 1,		srgb(255)		},
};
static v3 incandescent_gradient (f32 val) {
	return interpolate(val, _incandescent_gradient_keys, ARRLEN(_incandescent_gradient_keys));
}
static Interpolator_Key _spectrum_gradient_keys[] = {
	{ 0,		srgb(0,0,127)	},
	{ 0.25f,	srgb(0,0,248)	},
	{ 0.5f,		srgb(0,127,0)	},
	{ 0.75f,	srgb(255,255,0)	},
	{ 1,		srgb(255,0,0)	},
};
static v3 spectrum_gradient (f32 val) {
	return interpolate(val, _spectrum_gradient_keys, ARRLEN(_spectrum_gradient_keys));
}

#define _USING_V110_SDK71_ 1
#include "glad.c"
#include "GLFW/glfw3.h"

#include "platform.hpp"


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

#include "gl.hpp"
#include "font.hpp"

static font::Font* console_font;

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
	v3	pos_chunk;
	v4	uvzw_atlas; // xy: [0,1] texture uv;  z: 0=side, 1=top, 2=bottom;  w: texture index
	v4	dbg_tint;
};

static Vertex_Layout mesh_vert_layout = {
	{ "pos_chunk",	T_V3, sizeof(Mesh_Vertex), offsetof(Mesh_Vertex, pos_chunk) },
	{ "uvzw_atlas",	T_V4, sizeof(Mesh_Vertex), offsetof(Mesh_Vertex, uvzw_atlas) },
	{ "dbg_tint",	T_V4, sizeof(Mesh_Vertex), offsetof(Mesh_Vertex, dbg_tint) },
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
	
	BLOCK_TYPES_COUNT
};

static cstr block_texture_name[BLOCK_TYPES_COUNT] = {
	/* BT_AIR	*/	"missing.png",
	/* BT_EARTH	*/	"earth.png",
	/* BT_GRASS	*/	"grass.png",
};

static s32 atlas_textures_count = 3;

static s32 get_block_texture_index_from_block_type (block_type bt) {
	return bt;
}

static constexpr f32 BLOCK_FULL_HP = 100;

struct Block {
	block_type	type;
	f32			hp;
	v4			dbg_tint;
};

#include "noise.hpp"

static constexpr iv3 CHUNK_DIM = iv3(32,32,16);

struct Chunk {
	Block	data[CHUNK_DIM.z][CHUNK_DIM.y][CHUNK_DIM.x];
	
	Block& get_block (iv3 pos) {
		return data[pos.z][pos.y][pos.x];
	}
};

f32 heightmap_perlin2d (v2 v) {
	using namespace perlin_noise_n;
	
	//v += v2(1,-1)*mouse * 20;
	
	//f32 fre = lerp(0.33f, 3.0f, mouse.x);
	//f32 amp = lerp(0.33f, 3.0f, mouse.y);
	
	f32 tot = 0;
	
	tot += perlin_octave(v, 0.053f) * 6.2f;
	tot += perlin_octave(v, 0.175f) * 4;
	tot += perlin_octave(v, 0.597f) * 0.79f;
	
	tot *= 1.5f;
	
	//printf(">>>>>>>>> %.3f %.3f\n", fre, amp);
	
	return tot +3;
}

Chunk chunk = {};

f32 heightmap[CHUNK_DIM.y][CHUNK_DIM.x];

void gen_chunks () {
	iv3 i;
	for (i.z=0; i.z<CHUNK_DIM.z; ++i.z) {
		for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
				auto& b = chunk.get_block(i);
				
				b.type = BT_AIR;
				b.hp = BLOCK_FULL_HP;
				b.dbg_tint = 1;
			}
		}
	}
	
	for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
		for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
			
			f32 height = heightmap[i.y][i.x];
			s32 highest_block = (s32)floor(height +0.5f);
			
			for (i.z=0; i.z <= min(highest_block, 16); ++i.z) {
				auto& b = chunk.get_block(i);
				if (i.z != highest_block) {
					b.type = BT_EARTH;
				} else {
					b.type = BT_GRASS;
				}
				b.dbg_tint = v4(spectrum_gradient((height +0.5f) / 5), 1);
			}
		}
	}
}

//
static f32			dt = 0;

static v2			mouse; // for quick debugging

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

struct Camera {
	v3	pos_world;
	v2	ori_ae; // azimuth elevation
	
	f32	fly_vel;
	f32	fly_vel_fast_mul;
	f32	vfov;
};

static Camera default_camera = {	v3(0, -5, 1),
									v2(deg(0), deg(+80)),
									4,
									4,
									deg(70) };

static Camera		cam;

#define SAVE_FILE	"saves/camera_view.bin"

static void load_game () {
	bool loaded = read_entire_file(SAVE_FILE, &cam, sizeof(cam));
	if (loaded) {
		logf("camera_view loaded from \"" SAVE_FILE "\".");
	} else {
		cam = default_camera;
		logf_warning("camera_view could not be loaded from \"" SAVE_FILE "\", using defaults.");
	}
}
static void save_game () {
	bool saved = overwrite_file(SAVE_FILE, &cam, sizeof(cam));
	if (saved) {
		logf("camera_view saved to \"" SAVE_FILE "\".");
	} else {
		logf_warning("could not write \"" SAVE_FILE "\", camera_view wont be loaded on next launch.");
	}
}

static void glfw_key_event (GLFWwindow* window, int key, int scancode, int action, int mods) {
	dbg_assert(action == GLFW_PRESS || action == GLFW_RELEASE || action == GLFW_REPEAT);
	
	bool went_down =	action == GLFW_PRESS;
	bool went_up =		action == GLFW_RELEASE;
	
	bool repeated =		!went_down && !went_up; // GLFW_REPEAT
	
	bool alt =			(mods & GLFW_MOD_ALT) != 0;
	
	if (repeated) {
		
	} else {
		if (!alt) {
			switch (key) {
				case GLFW_KEY_F11:			if (went_down) {		toggle_fullscreen(); }	break;
				
				//
				case GLFW_KEY_A:			inp.move_dir.x -= went_down ? +1 : -1;		break;
				case GLFW_KEY_D:			inp.move_dir.x += went_down ? +1 : -1;		break;
				
				case GLFW_KEY_S:			inp.move_dir.z += went_down ? +1 : -1;		break;
				case GLFW_KEY_W:			inp.move_dir.z -= went_down ? +1 : -1;		break;
				
				case GLFW_KEY_LEFT_CONTROL:	inp.move_dir.y -= went_down ? +1 : -1;		break;
				case GLFW_KEY_SPACE:		inp.move_dir.y += went_down ? +1 : -1;		break;
				
				case GLFW_KEY_LEFT_SHIFT:	inp.move_fast = went_down;					break;
			}
		} else {
			switch (key) {
				
				case GLFW_KEY_ENTER:		if (alt && went_down) {	toggle_fullscreen(); }	break;
				
				//
				case GLFW_KEY_S:			if (alt && went_down) {	save_game(); }			break;
				case GLFW_KEY_L:			if (alt && went_down) {	load_game(); }			break;
			}
		}
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
	if (!inp.move_fast) {
		f32 delta_log = 0.1f * (f32)yoffset;
		cam.fly_vel = pow( 2, log2(cam.fly_vel) +delta_log );
		logf(">>> fly_vel: %f", cam.fly_vel);
	} else {
		f32 delta_log = -0.1f * (f32)yoffset;
		f32 vfov = pow( 2, log2(cam.vfov) +delta_log );
		if (vfov >= deg(1.0f/10) && vfov <= deg(170)) cam.vfov = vfov;
	}
}
static void glfw_cursor_move_relative (GLFWwindow* window, double dx, double dy) {
	v2 diff = v2((f32)dx,(f32)dy);
	inp.mouse_look_diff += diff;
}

static Vbo		vbo_console_font;
static Shader*	shad_font;

int main (int argc, char** argv) {
	
	cstr app_name = "Voxel Game";
	
	platform_setup_context_and_open_window(app_name, iv2(1280, 720));
	
	//
	load_game();
	
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
	#define UMAT UM4("world_to_cam"), UM4("cam_to_clip"), UM4("cam_to_world") // transformation uniforms
	
	//shad_equirectangular_to_cubemap = new_shader("equirectangular_to_cubemap.vert",	"equirectangular_to_cubemap.frag", {UCOM}, {{0,"equirectangular"}});
	
	{ // init game console overlay
		f32 sz =	16; // 14 16 24
		f32 jpsz =	floor(sz * 1.75f);
		
		std::initializer_list<font::Glyph_Range> ranges = {
			{ "consola.ttf",	sz,		  U'\xfffd' }, // missing glyph placeholder, must be the zeroeth glyph
			{ "consola.ttf",	sz,		  U' ', U'~' },
			{ "consola.ttf",	sz,		{ U'ß',U'Ä',U'Ö',U'Ü',U'ä',U'ö',U'ü' } }, // german umlaute
			{ "meiryo.ttc",		jpsz,	  U'\x3040', U'\x30ff' }, // hiragana +katakana
			{ "meiryo.ttc",		jpsz,	{ U'　',U'、',U'。',U'”',U'「',U'」' } }, // some jp puncuation
		};
		
		console_font = new font::Font(sz, ranges);
		
		vbo_console_font.init(&font::mesh_vert_layout);
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
	
	auto* shad_sky = new_shader("skybox.vert",	"skybox.frag",	{UCOM, UM4("skybox_to_clip")});
	auto* shad_main = new_shader("main.vert",	"main.frag",	{UCOM, USI("texture_res"), USI("atlas_textures_count"), UMAT}, {{0,"atlas"}});
	
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
	
	Vbo chunk_vbo;
	chunk_vbo.init(&mesh_vert_layout);
	
	// 
	f64 prev_t = glfwGetTime();
	f32 avg_dt = 1.0f / 60;
	f32 avg_dt_alpha = 0.025f;
	
	for (u32 frame_i=0;; ++frame_i) {
		
		{ //
			f32 fps = 1.0f / dt;
			f32 dt_ms = dt * 1000;
			
			f32 avg_fps = 1.0f / avg_dt;
			f32 avdt_ms = avg_dt * 1000;
			
			//printf("frame #%5d %6.1f fps %6.2f ms  avg: %6.1f fps %6.2f ms\n", frame_i, fps, dt_ms, avg_fps, avdt_ms);
			glfwSetWindowTitle(wnd, prints("%s %6d  %6.1f fps avg %6.2f ms avg", app_name, frame_i, avg_fps, avdt_ms).c_str());
		}
		
		inp.mouse_look_diff = 0;
		
		glfwPollEvents();
		
		inp.get_non_callback_input();
		
		//printf(">>> mouse: %.2f %.2f\n", mouse.x,mouse.y);
		
		if (glfwWindowShouldClose(wnd)) break;
		
		for (auto* s : shaders)			s->reload_if_needed();
		
		hm world_to_cam;
		hm cam_to_world;
		m4 cam_to_clip;
		m4 skybox_to_clip;
		{
			{
				v2 mouse_look_sens = v2(deg(1.0f / 8)) * (cam.vfov / deg(70));
				cam.ori_ae -= inp.mouse_look_diff * mouse_look_sens;
				cam.ori_ae.x = mymod(cam.ori_ae.x, deg(360));
				cam.ori_ae.y = clamp(cam.ori_ae.y, deg(2), deg(180.0f -2));
				
				//printf(">>> %f %f\n", to_deg(camera_ae.x), to_deg(camera_ae.y));
			}
			m3 world_to_cam_rot = rotate3_X(-cam.ori_ae.y) * rotate3_Z(-cam.ori_ae.x);
			m3 cam_to_world_rot = rotate3_Z(cam.ori_ae.x) * rotate3_X(cam.ori_ae.y);
			
			{
				f32 cam_vel_forw = cam.fly_vel;
				if (inp.move_fast) cam_vel_forw *= cam.fly_vel_fast_mul;
				
				v3 cam_vel = cam_vel_forw * v3(1,2.0f/3,1);
				
				v3 cam_vel_cam = normalize_or_zero( (v3)inp.move_dir ) * cam_vel;
				cam.pos_world += (cam_to_world_rot * cam_vel_cam) * dt;
				
				//printf(">>> %f %f %f\n", cam_vel_cam.x, cam_vel_cam.y, cam_vel_cam.z);
			}
			world_to_cam = world_to_cam_rot * translateH(-cam.pos_world);
			cam_to_world =translateH(cam.pos_world) * cam_to_world_rot;
			
			{
				f32 vfov =			cam.vfov;
				f32 clip_near =		1.0f/256;
				f32 clip_far =		512;
				
				v2 frust_scale;
				frust_scale.y = tan(vfov / 2);
				frust_scale.x = frust_scale.y * inp.wnd_dim_aspect.x;
				
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
			
			skybox_to_clip = cam_to_clip * world_to_cam_rot;
		}
		
		for (auto* s : shaders) {
			if (s->valid()) {
				s->bind();
				s->set_unif("screen_dim", (v2)inp.wnd_dim);
				s->set_unif("mcursor_pos", inp.bottom_up_mcursor_pos());
			}
		}
		
		gen_noise_test();
		
		glViewport(0,0, inp.wnd_dim.x,inp.wnd_dim.y);
		
		if (shad_sky->valid()) { // draw skybox
			glDisable(GL_DEPTH_TEST);
			
			shad_sky->bind();
			shad_sky->set_unif("skybox_to_clip", skybox_to_clip);
			
			glDrawArrays(GL_TRIANGLES, 0, 6*6);
			
			glEnable(GL_DEPTH_TEST);
		}
		glClear(GL_DEPTH_BUFFER_BIT);
		
		glEnable(GL_BLEND);
		
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
		
		{
			chunk_vbo.vertecies.clear();
			
			auto cube = [&] (iv3 block_pos, Block const& b) {
				
				Mesh_Vertex* out = (Mesh_Vertex*)&*vector_append(&chunk_vbo.vertecies, sizeof(Mesh_Vertex)*6*6);
				
				f32 XL = (f32)block_pos.x;
				f32 YL = (f32)block_pos.y;
				f32 ZL = (f32)block_pos.z;
				f32 XH = (f32)(block_pos.x +1);
				f32 YH = (f32)(block_pos.y +1);
				f32 ZH = (f32)(block_pos.z +1);
				
				f32 w = get_block_texture_index_from_block_type(b.type);
				
				// Sides
				*out++ = { v3(XH,YL,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				
				*out++ = { v3(XH,YH,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YL,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				
				*out++ = { v3(XL,YH,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				
				*out++ = { v3(XL,YL,ZL), v4(1,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(1,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(0,1, UVZW_BLOCK_FACE_SIDE,w), b.dbg_tint };
				
				// Top
				*out++ = { v3(XH,YL,ZH), v4(1,0, UVZW_BLOCK_FACE_TOP,w), b.dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_TOP,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(0,0, UVZW_BLOCK_FACE_TOP,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZH), v4(0,0, UVZW_BLOCK_FACE_TOP,w), b.dbg_tint };
				*out++ = { v3(XH,YH,ZH), v4(1,1, UVZW_BLOCK_FACE_TOP,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZH), v4(0,1, UVZW_BLOCK_FACE_TOP,w), b.dbg_tint };
				// Bottom
				*out++ = { v3(XH,YH,ZL), v4(1,0, UVZW_BLOCK_FACE_BOTTOM,w), b.dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(1,1, UVZW_BLOCK_FACE_BOTTOM,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_BOTTOM,w), b.dbg_tint };
				*out++ = { v3(XL,YH,ZL), v4(0,0, UVZW_BLOCK_FACE_BOTTOM,w), b.dbg_tint };
				*out++ = { v3(XH,YL,ZL), v4(1,1, UVZW_BLOCK_FACE_BOTTOM,w), b.dbg_tint };
				*out++ = { v3(XL,YL,ZL), v4(0,1, UVZW_BLOCK_FACE_BOTTOM,w), b.dbg_tint };
				
			};
			
			{
				iv3 i;
				for (i.z=0; i.z<CHUNK_DIM.z; ++i.z) {
					for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
						for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
							auto block = chunk.get_block(i);
							
							if (block.type != BT_AIR) {
								
								cube(i, block);
								
							}
						}
					}
				}
			}
			
			chunk_vbo.upload();
			
			if (shad_main->valid()) {
				bind_texture_unit(0, &tex_block_atlas);
				
				shad_main->bind();
				shad_main->set_unif("world_to_cam",	world_to_cam.m4());
				shad_main->set_unif("cam_to_clip",	cam_to_clip);
				shad_main->set_unif("cam_to_world",	cam_to_world.m4());
				
				shad_main->set_unif("texture_res", texture_res);
				shad_main->set_unif("atlas_textures_count", atlas_textures_count);
				
				chunk_vbo.draw_entire(shad_main);
			}
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
				draw_overlay_tex2d(&noise_test, LL, 8);
			}
			#if 0
			if (0 && shad_overlay_cubemap->valid()) {
				//draw_overlay_texCube(tex_test_cubemap1, UR, (v2)min(inp.wnd_dim.x, inp.wnd_dim.y) / 2);
				//draw_overlay_texCube(tex_test_cubemap2, UL, (v2)min(inp.wnd_dim.x, inp.wnd_dim.y) / 2);
			}
			#endif
			
		}
		
		#if 0
		if (shad_font->valid()) {
			glEnable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			
			shad_font->bind();
			shad_font->set_unif("screen_dim", (v2)inp.wnd_dim);
			bind_texture_unit(0, &console_font->tex);
			
			vbo_console_font.upload();
			vbo_console_font.draw_entire(shad_font);
			
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
		}
		#endif
		
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
