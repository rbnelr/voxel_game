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

#include <algorithm> // for min max

#include "open_simplex_noise/open_simplex_noise.hpp"

#include "assert.h"
#include <string>
#include <vector>
#include <unordered_map>

extern float elev_freq, elev_amp;
extern float rough_freq;
extern float detail0_freq, detail0_amp;
extern float detail1_freq, detail1_amp;
extern float detail2_freq, detail2_amp;

#include "gl.hpp"
#include "blocks.hpp"
#include "input.hpp"
#include "graphics/camera.hpp"
#include "player.hpp"
#include "running_average.hpp"

//
#define UBOOL(name)	Uniform(T_BOOL, name)
#define USI(name)	Uniform(T_INT, name)
#define UIV2(name)	Uniform(T_IV2, name)
#define UV2(name)	Uniform(T_V2, name)
#define UV3(name)	Uniform(T_V3, name)
#define UM4(name)	Uniform(T_M4, name)

#define UCOM UV2("screen_dim"), UV2("mcursor_pos") // common uniforms
#define UMAT UM4("world_to_cam"), UM4("cam_to_world"), UM4("cam_to_clip") // transformation uniforms

extern std::vector<Shader*>			shaders;

Shader* new_shader (std::string const& v, std::string const& f, std::initializer_list<Uniform> u, std::initializer_list<Shader::Uniform_Texture> t={});

#include "chunks.hpp"

static float heightmap (OSN::Noise<2> const& osn_noise, float2 pos_world) {
	auto noise = [&] (float2 pos, float period, float ang_offs, float2 offs, float range_l, float range_h) {
		pos = rotate2(ang_offs) * pos;
		pos /= period; // period is inverse frequency
		pos += offs;

		float val = osn_noise.eval<float>(pos.x, pos.y);
		val = map(val, -0.865773f, 0.865772f, range_l,range_h); // normalize into [range_l,range_h] range
		return val;
	};

	float elevation;
	float roughness;
	float detail;

	int i = 0;
	{
		float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
		offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
		elevation = noise(pos_world, elev_freq, deg(37.17f) * (float)i, offs, -1,+1) * elev_amp;

		++i;
	}
	{
		float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
		offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
		roughness = noise(pos_world, rough_freq, deg(37.17f) * (float)i, offs, 0,+1);

		++i;
	}

	detail = 0;
	{
		float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
		offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
		detail += noise(pos_world, detail0_freq, deg(37.17f) * (float)i, offs, -1,+1) * detail0_amp;

		++i;
	}
	{
		float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
		offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
		detail += noise(pos_world, detail1_freq, deg(37.17f) * (float)i, offs, -1,+1) * detail1_amp;

		++i;
	}
	{
		float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
		offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
		detail += noise(pos_world, detail2_freq, deg(37.17f) * (float)i, offs, -1,+1) * detail2_amp;

		++i;
	}

	return (elevation +32) +(roughness * detail);
}

extern bool _use_potatomode;

struct Graphics_Settings {
	bool	foliage_alpha = !_use_potatomode;

	void imgui () {
		//if (ImGui::Checkbox("Foliage Alpha", &foliage_alpha)) foliage_alpha_changed();
	}
};


namespace random {

	inline float flt (float l=0, float h=1) {
		return (float)rand() / (float)RAND_MAX;
	}

}

template<typename T> struct Gradient_KV {
	float	key;
	T	val;
};

template<typename T> static T gradient (float key, Gradient_KV<T> const* kvs, size_t kvs_count) {
	if (kvs_count == 0) return T(0);

	size_t i=0;
	for (; i<kvs_count; ++i) {
		if (key < kvs[i].key) break;
	}

	if (i == 0) { // val is lower than the entire range
		return kvs[0].val;
	} else if (i == kvs_count) { // val is higher than the entire range
		return kvs[i -1].val;
	} else {
		assert(kvs_count >= 2 && i < kvs_count);

		auto& a = kvs[i -1];
		auto& b = kvs[i];
		return map(key, a.key, b.key, a.val, b.val);
	}
}
template<typename T> static T gradient (float key, std::initializer_list<Gradient_KV<T>> const& kvs) {
	return gradient<T>(key, &*kvs.begin(), kvs.size());
}

static lrgba incandescent_gradient (float key) {
	return lrgba(gradient<float3>(key, {
		{ 0,		srgb(0)			},
		{ 0.3333f,	srgb(138,0,0)	},
		{ 0.6667f,	srgb(255,255,0)	},
		{ 1,		srgb(255)		},
		}), 1);
}
static lrgba spectrum_gradient (float key) {
	return lrgba(gradient<float3>(key, {
		{ 0,		srgb(0,0,127)	},
		{ 0.25f,	srgb(0,0,248)	},
		{ 0.5f,		srgb(0,127,0)	},
		{ 0.75f,	srgb(255,255,0)	},
		{ 1,		srgb(255,0,0)	},
		}), 1);
}

struct FPS_Display {
	RunningAverage<float> dt_avg = RunningAverage<float>(64);
	float latest_avg_dt;

	float update_period = .5f; // sec
	float update_timer = 0;

	int histogram_height = 40;

	void display_fps () {
		dt_avg.push(input.real_dt);

		if (update_timer <= 0) {
			latest_avg_dt = dt_avg.calc_avg();
			update_timer += update_period;
		}
		update_timer -= input.real_dt;

		float avg_fps = 1.0f / latest_avg_dt;
		ImGui::Text("avg fps: %5.1f (%6.3f ms)  ----  timestep: %6.3f ms", avg_fps, latest_avg_dt * 1000, input.dt * 1000);

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotHistogram("###frametimes_histogram", dt_avg.values.get(), dt_avg.count, 0, "frametimes:", 0, 0.033f, ImVec2(0, (float)histogram_height));

		if (ImGui::BeginPopupContextItem()) {
			ImGui::SliderInt("histogram_height", &histogram_height, 20, 120);
			ImGui::EndPopup();
		}
	}
};

class Game {
	FPS_Display fps_display;

	bool activate_flycam = false;

	Player player = Player("player");

	Flycam flycam = Flycam("flycam", float3(-5, -10, 50), float3(0, deg(80), 0), 12);

	Shader* shad_skybox = new_shader("skybox.vert",	"skybox.frag",	{UCOM, UMAT});
	Shader* shad_blocks = new_shader("blocks.vert",	"blocks.frag",	{UCOM, UMAT, UBOOL("draw_wireframe"), UBOOL("show_dbg_tint"), USI("texture_res"), USI("atlas_textures_count"), USI("breaking_frames_count"), UBOOL("alpha_test")}, {{0,"atlas"}, {1,"breaking"}});
	Shader* shad_overlay = new_shader("overlay.vert",	"overlay.frag",	{UCOM, UMAT});

	Texture2D* tex_block_atlas = generate_and_upload_block_texture_atlas();

	Texture2D_File tex_breaking = Texture2D_File(CS_LINEAR, "breaking.png");

	float chunk_drawing_radius =	_use_potatomode ? 20.0f : INF;
	float chunk_generation_radius =	_use_potatomode ? 20.0f : 140.0f;

	Texture2D dbg_heightmap_visualize = Texture2D("dbg_heightmap_visualize");
	int dbg_heightmap_visualize_radius = 1000;

	int64_t world_seed = 0;

	float noise_tree_desity_period = 200;
	float noise_tree_density_amp = 1;

	Graphics_Settings graphics_settings;

	void foliage_alpha_changed () {
		block_props[BT_LEAVES].transparency = graphics_settings.foliage_alpha ? TM_TRANSP_BLOCK : TM_OPAQUE;
		trigger_regen_chunks = true;
	}

	bool trigger_regen_chunks =		false;

	void regen_dbg_heightmap_visualize () {
		srgba8* pixels = (srgba8*)dbg_heightmap_visualize.mips[0].data;
		int2 dim = dbg_heightmap_visualize.mips[0].dim;

		auto dst = [&] (int2 pos) -> srgba8* {
			return &pixels[pos.y*dim.x + pos.x];
		};

		OSN::Noise<2> noise( world_seed );

		int2 i;
		for (i.y=0; i.y<dim.y; ++i.y) {
			for (i.x=0; i.x<dim.x; ++i.x) {
				float2 pos_world = map((float2)i, 0,(float2)(dim-1), -dbg_heightmap_visualize_radius / 2.0f, +dbg_heightmap_visualize_radius / 2.0f);
				*dst(i) = to_srgb(spectrum_gradient(map( heightmap(noise, pos_world) , 0, 45)));
			}
		}

		dbg_heightmap_visualize.upload();
	};

	
	float noise_tree_density (OSN::Noise<2> const& osn_noise, float2 pos_world) {
		auto noise = [&] (float2 pos, float period, float ang_offs, float2 offs) {
			pos = rotate2(ang_offs) * pos;
			pos /= period; // period is inverse frequency
			pos += offs;
		
			float val = osn_noise.eval<float>(pos.x, pos.y);
			val = map(val, -0.865773f, 0.865772f); // normalize into [0,1] range
			return val;
		};
	
		float val = noise(pos_world, noise_tree_desity_period, 0,0) * noise_tree_density_amp;
	
		val = gradient<float>(val, {
			{ 0.00f,  0						},
			{ 0.05f,  1.0f / (5*5 * 32*32)	}, // avg one tree in 5x5 chunks
			{ 0.25f,  1.0f / (32*32)		}, // avg one tree in 1 chunk
			{ 0.50f,  4.0f / (32*32)		}, // avg 5 tree in 1 chunk
			{ 0.75f, 10.0f / (32*32)		}, // avg 15 tree in 1 chunk
			{ 1.00f, 25.0f / (32*32)		}, // avg 40 tree in 1 chunk
		});
	
		#if 0
		// TODO: use height of block to alter tree density
		val = gradient<float>(val, {
			{ 0.00f,  0						},
			{ 0.05f,  1.0f / (5*5 * 32*32)	}, // avg one tree in 5x5 chunks
			{ 0.25f,  1.0f / (32*32)		}, // avg one tree in 1 chunk
			{ 0.50f,  5.0f / (32*32)		}, // avg 5 tree in 1 chunk
			{ 0.75f, 15.0f / (32*32)		}, // avg 15 tree in 1 chunk
			{ 1.00f, 40.0f / (32*32)		}, // avg 40 tree in 1 chunk
		});
		#endif
	
		return val;
	}

	Vbo overlay_vbo;

	int max_chunks_generated_per_frame = 1;

	//float block_update_frequency = 1.0f;
	float block_update_frequency = 1.0f / 25;
	bpos_t cur_chunk_update_block_i = 0;

	struct Overlay_Vertex {
		float3	pos_world;
		srgba8	color;
	};

	Vertex_Layout overlay_vertex_layout = {
		{ "pos_world",	T_V3,	sizeof(Overlay_Vertex), offsetof(Overlay_Vertex, pos_world) },
		{ "color",		T_U8V4,	sizeof(Overlay_Vertex), offsetof(Overlay_Vertex, color) },
	};
	
	void gen_chunk_blocks (Chunk* chunk) {
		bpos_t water_level = 21;
	
		bpos i; // position in chunk
		for (i.z=0; i.z<CHUNK_DIM_Z; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
					auto* b = chunk->get_block(i);
				
					if (i.z <= water_level) {
						b->type = BT_WATER;
						b->hp_ratio = 1;
						b->dbg_tint = 255;
					} else {
						b->type = BT_AIR;
						b->hp_ratio = 0;
						b->dbg_tint = 0;
					}
				}
			}
		}
	
		OSN::Noise<2> noise( world_seed );
	
		srand( (unsigned int)hash(chunk->coord) );
	
		std::vector<bpos> tree_poss;
	
		auto find_min_tree_dist = [&] (bpos2 new_tree_pos) {
			float min_dist = +INF;
			for (bpos p : tree_poss)
				min_dist = min(min_dist, length((float2)(bpos2)p -(float2)new_tree_pos));
			return min_dist;
		};
	
		for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
			
				float2 pos_world = (float2)((bpos2)i +chunk->coord * CHUNK_DIM_2D);
			
				float height = heightmap(noise, pos_world);
				int highest_block = (int)floor(height -1 +0.5f); // -1 because height 1 means the highest block is z=0
			
				float tree_density = noise_tree_density(noise, pos_world);
			
				float tree_prox_prob = gradient<float>( find_min_tree_dist((bpos2)i), {
					{ SQRT_2,	0 },		// length(float2(1,1)) -> zero blocks free diagonally
					{ 2.236f,	0.02f },	// length(float2(1,2)) -> one block free
					{ 2.828f,	0.15f },	// length(float2(2,2)) -> one block free diagonally
					{ 4,		0.75f },
					{ 6,		1 },
				});
				float effective_tree_prob = tree_density * tree_prox_prob;
				//float effective_tree_prob = tree_density;
			
				float tree_chance = random::flt();
				if (tree_chance < effective_tree_prob)
					tree_poss.push_back( bpos((bpos2)i, highest_block +1) );
			
				for (i.z=0; i.z <= min(highest_block, (int)CHUNK_DIM_Z-1); ++i.z) {
					auto* b = chunk->get_block(i);
				
					if (i.z == highest_block && i.z >= water_level
							//&& equal(chunk->coord, 0)
							//&& (chunk->coord.x % 2 == 0 && chunk->coord.y % 2 == 0)
							) {
						b->type = BT_GRASS;
					} else {
						b->type = BT_EARTH;
					}
				
					b->hp_ratio = 1;
				
					//b->dbg_tint = lrgba8(spectrum_gradient(map(height, 0, 45)), 255);
					b->dbg_tint = spectrum_gradient(tree_density / (25.0f/(32*32)));
				}
			}
		}
	
		auto place_tree = [&] (bpos pos_chunk) {
			auto place_block = [&] (bpos pos_chunk, block_type bt) {
				if (any(pos_chunk < 0 || pos_chunk >= CHUNK_DIM)) return;
				Block* b = chunk->get_block(pos_chunk);
				if (b->type == BT_AIR || b->type == BT_WATER || (bt == BT_TREE_LOG && b->type == BT_LEAVES)) {
					b->type = bt;
					b->hp_ratio = 1;
					b->dbg_tint = 255;
				}
			};
			auto place_block_sphere = [&] (bpos pos_chunk, float3 r, block_type bt) {
				bpos start = (bpos)floor((float3)pos_chunk +0.5f -r);
				bpos end = (bpos)ceil((float3)pos_chunk +0.5f +r);
			
				bpos i; // position in chunk
				for (i.z=start.z; i.z<end.z; ++i.z) {
					for (i.y=start.y; i.y<end.y; ++i.y) {
						for (i.x=start.x; i.x<end.x; ++i.x) {
							if (length_sqr((float3)(i -pos_chunk) / r) <= 1) place_block(i, bt);
						}
					}
				}
			};
		
			bpos_t tree_height = 6;
		
			for (bpos_t i=0; i<tree_height; ++i) place_block(pos_chunk +bpos(0,0,i), BT_TREE_LOG);
		
			place_block_sphere(pos_chunk +bpos(0,0,tree_height-1), float3(float2(3.2f),tree_height/2.5f), BT_LEAVES);
		};
	
		for (bpos p : tree_poss) place_tree(p);
	
		chunk->update_whole_chunk_changed();
	}

	void generate_new_chunk (chunk_pos_t chunk_pos) {

		Chunk* c = new Chunk();

		c->coord = chunk_pos;
		c->init_gl();
		gen_chunk_blocks(c);

		chunks.insert({{c->coord}, c});
	}

	void inital_chunk (float3 player_pos_world) {
		generate_new_chunk( get_chunk_from_block_pos( (bpos)floor(player_pos_world) ) );
	}

	bool draw_debug_overlay = 0;
	bool draw_wireframe = 0;
	bool show_dbg_tint = 0;
	int breaking_frames_count;


public:
	Game ();

	void frame ();

};
