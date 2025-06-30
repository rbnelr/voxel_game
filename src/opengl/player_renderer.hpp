#pragma once
#include "common.hpp"
#include "opengl_renderer.hpp"

namespace gl {

struct BlockHighlight {
	Shader*					shad;
	BlockHighlightSubmeshes	block_highl;

	BlockHighlight (Shaders& shaders) {
		shad = shaders.compile("block_highlight");
	}
	void draw (OpenglRenderer& r, SelectedBlock& block) {
		OGL_TRACE("block_highlight");

		// rotate from -Y to facing in a block face direction
		static constexpr float3x3 face_rotation[] = {
			// BF_NEG_X
			//rotate3_Z(deg(-90)),
			float3x3(0,1,0,  -1,0,0,  0,0,1),
			// BF_POS_X
			//rotate3_Z(deg(+90)),
			float3x3(0,-1,0,  1,0,0,  0,0,1),
			// BF_NEG_Y
			//float3x3::identity(),
			float3x3(1,0,0,  0,1,0,  0,0,1),
			// BF_POS_Y
			//rotate3_Z(deg(180)),
			float3x3(-1,0,0,  0,-1,0,  0,0,1),
			// BF_NEG_Z
			//rotate3_X(deg(+90)),
			float3x3(1,0,0,  0,0,-1,  0,1,0),
			// BF_POS_Z
			//rotate3_X(deg(-90)),
			float3x3(1,0,0,  0,0,1,  0,-1,0),
		};

		PipelineState s;
		s.depth_test = true;
		s.blend_enable = true;
		r.state.set(s);

		glUseProgram(shad->prog);
		r.state.bind_textures(shad, {});

		shad->set_uniform("block_pos", (float3)block.hit.pos);
		shad->set_uniform("face_rotation", face_rotation[0]);
		shad->set_uniform("tint", srgba(40,40,40,240));

		glBindVertexArray(r.mesh_data.vao);
		draw_submesh(block_highl.block_highlight);
	
		if (block.hit.face >= 0) {
			shad->set_uniform("face_rotation", face_rotation[ (BlockFace)(block.hit.face >= 0 ? block.hit.face : 0) ]);
			draw_submesh(block_highl.face_highlight);
		}
	}
};

struct GuiRenderer {
	Shader*			gui_shad;
	Sampler			gui_sampler = {"gui_sampler"};

	BlockHighlight  block_highl;

	struct GUIVertex {
		float2 pos; // 2d because we display 2d gui
		float3 normal; // but still 3d normals for a bit of lighting on the pseudo 3d cubes for blocks
		float3 uvi; // z is tile texture index, -1 means gui texture
		
		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(GUIVertex));
			a.template add<AttribMode::FLOAT, decltype(pos   )>(loc++, "pos"   , offsetof(GUIVertex, pos   ));
			a.template add<AttribMode::FLOAT, decltype(normal)>(loc++, "normal", offsetof(GUIVertex, normal));
			a.template add<AttribMode::FLOAT, decltype(uvi   )>(loc++, "uvi"   , offsetof(GUIVertex, uvi   ));
		}
	};

	IndexedBuffer	gui_ib	= indexed_buffer<GUIVertex>("GUIRenderer.gui_vbo");
	
	std::array<GuiRenderer::GUIVertex, 12> build_gui_block_mesh () {
		static constexpr float scale = 0.41f;
		const float3x3 rot = rotate3_X(deg(-82.0f)) * rotate3_Z(deg(8.0f));
		std::array<GuiRenderer::GUIVertex, 12> verts;

		static constexpr BlockFace faces[] = { BF_NEG_X, BF_NEG_Y, BF_POS_Z };

		int i = 0;
		for (int face=0; face<3; ++face) {
			for (int vert=0; vert<4; ++vert) {
				auto& v = verts[i++];
				v.pos    = (float2)(rot * CUBE_CORNERS[faces[face]][vert]*scale);
				v.normal = rot * CUBE_NORMALS[faces[face]];
				v.uvi    = float3(QUAD_UV[vert], 0);
			}
		}
		return verts;
	}
	std::array<GuiRenderer::GUIVertex, 12> _gui_block_mesh = build_gui_block_mesh();

	std::vector<GUIVertex> gui_vertex_data;
	std::vector<uint16_t> gui_index_data;

	// allocate vertices for N quads (4 each) and already fill indices for them
	GUIVertex* push_quads (int count) {
		size_t base_idx = gui_vertex_data.size();
		gui_vertex_data.resize(base_idx + 4*count);
		auto* verts = &gui_vertex_data[base_idx];

		size_t idx_offs = gui_index_data.size();
		gui_index_data.resize(idx_offs + 6*count);
		auto* indices = &gui_index_data[idx_offs];

		for (int j=0; j<count; ++j) {
			for (int i=0; i<6; ++i) {
				*indices++ = (uint16_t)(base_idx + j*4 + QUAD_INDICES[i]);
			}
		}

		return verts;
	}

	struct AtlasUVs {
		float2 pos;
		float2 size;
	};
	static constexpr AtlasUVs crosshair_uv			= { float2(   0,    0)   ,    32    };
	static constexpr AtlasUVs frame_uv				= { float2(32*1,    0) +6,    16 +4 };
	static constexpr AtlasUVs frame_highl_uv		= { float2(32*2,    0) +6,    16 +4 };
	static constexpr AtlasUVs frame_grabbed_uv		= { float2(32*2, 32*1) +6,    16 +4 };
	static constexpr AtlasUVs frame_selected_uv		= { float2(32*3,    0) +4,    16 +8 };

	int gui_scale = 4;
	bool crosshair = false;

	// render quad with pixel coords
	void draw_gui_quad (float2 const& pos, float2 const& size, AtlasUVs const& uv) {
		GUIVertex* verts = push_quads(1);
		for (int i=0; i<4; ++i) {
			verts[i].pos    = pos + QUAD_CORNERS[i] * size;
			verts[i].normal = float3(0, 0, 1);
			verts[i].uvi    = float3((uv.pos + QUAD_UV[i] * uv.size) * (1.0f/256), -1);
		}
	}
	void draw_item_quad (float2 const& pos, float2 const& size, item_id item) {
		if (item < MAX_BLOCK_ID) {
			GUIVertex* verts = push_quads(3);
		
			float tile_idxs[] = {
				(float)g->assets->block_tiles[item].sides[BF_NEG_X],
				(float)g->assets->block_tiles[item].sides[BF_NEG_Y],
				(float)g->assets->block_tiles[item].sides[BF_POS_Z],
			};
		
			for (int i=0; i<12; ++i) {
				verts[i].pos    = pos +  _gui_block_mesh[i].pos * size;
				verts[i].normal =        _gui_block_mesh[i].normal;
				verts[i].uvi    = float3(_gui_block_mesh[i].uvi.x, _gui_block_mesh[i].uvi.y, tile_idxs[i/4]);
			}

		} else {
			GUIVertex* verts = push_quads(1);
		
			float tile_idx = (float)ITEM_TILES[item - MAX_BLOCK_ID];
			for (int i=0; i<4; ++i) {
				verts[i].pos    = pos + QUAD_CORNERS[i] * size;
				verts[i].normal = float3(0, 0, 1);
				verts[i].uvi    = float3(QUAD_UV[i], tile_idx);
			}
		}
	}

	GuiRenderer (Shaders& shaders): block_highl{shaders} {
		gui_shad = shaders.compile("gui");

		glSamplerParameteri(gui_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(gui_sampler, GL_TEXTURE_MAX_ANISOTROPY, 1);
	}
	void draw_gui (OpenglRenderer& r, Input& I) {
		ZoneScoped;
		OGL_TRACE("gui");
	
		update_gui(I);

		glBindVertexArray(gui_ib.vao);
		stream_buffer(gui_ib, gui_vertex_data, gui_index_data);

		if (gui_vertex_data.size() > 0) {
			PipelineState s;
			s.depth_test = false;
			s.blend_enable = true;
			r.state.set(s);
			glUseProgram(gui_shad->prog);

			r.state.bind_textures(gui_shad, {
				{"tex", r.gui_atlas, gui_sampler},
				{"tile_textures", r.tile_textures, r.pixelated_sampler},
			});

			glDrawElements(GL_TRIANGLES, (GLsizei)gui_index_data.size(), GL_UNSIGNED_SHORT, (void*)0);
		}
	}

	void update_gui (Input& I) {
		gui_vertex_data.clear();
		gui_index_data.clear();

		float sz = (float)gui_scale;

		float2 screen_center = round((float2)I.window_size /2);
	
		// calc pixel coords

		if (crosshair) { // crosshair
			draw_gui_quad(screen_center, 32*sz, crosshair_uv);
		}

		float frame_sz     = frame_uv.size.x * sz;
		float sel_frame_sz = frame_selected_uv.size.x * sz;
		float item_sz      = 16 * sz;

		bool clicked = I.cursor_enabled && I.buttons[MOUSE_BUTTON_LEFT].went_down;

		auto draw_items_grid = [&] (Item* items, int count, int w, int h, float2 const& anchor, int selected=-1) {
			auto& backpack = g->player->inventory.backpack;

			float2 start = anchor -(float2)int2(w-1,h-1)/2 * frame_sz;

			int2 hovered_idx = roundi((I.cursor_pos_bottom_up() - start) / frame_sz);

			for (int i=0; i<count; ++i) {
				int2 idx2 = int2(i%w, h-1 -i/w);

				bool hovered = I.cursor_enabled && idx2 == hovered_idx;
				if (hovered && clicked) {
					std::swap(items[i], g->player->inventory.hand);
					clicked = false;
				}
				auto& tex = hovered ?
					//(g->player->inventory.hand.id != I_NULL ? frame_grabbed_uv : frame_highl_uv) :
					frame_highl_uv :
					frame_uv;

				float2 pos = start + (float2)idx2 * frame_sz;
			
				draw_gui_quad(pos, frame_sz, tex);

				if (items[i].id != I_NULL)
					draw_item_quad(pos, item_sz, items[i].id);
			}

			if (selected >= 0) {
				int2 idx2 = int2(selected % w, h-1 - selected / w);
				float2 pos = start + (float2)idx2 * frame_sz;
				draw_gui_quad(pos, sel_frame_sz, frame_selected_uv);
			}
		};

		{ // toolbar
			auto& toolbar = g->player->inventory.toolbar;

			float2 anchor = float2(screen_center.x, frame_sz/2 +1*sz);
			draw_items_grid(toolbar.slots, ARRLEN(toolbar.slots), ARRLEN(toolbar.slots), 1, anchor, toolbar.selected);
		}

		if (g->player->inventory.is_open) { // backpack
			auto& backpack = g->player->inventory.backpack;

			static int w = 10;
			ImGui::DragInt("backpack gui w", &w, 0.05f);

			float2 anchor = screen_center;
			draw_items_grid(&backpack.slots[0][0], 10*10, w, 10, anchor);

			if (I.cursor_enabled && g->player->inventory.hand.id != I_NULL)
				draw_item_quad(I.cursor_pos_bottom_up() +(item_sz/2), item_sz, g->player->inventory.hand.id);
		}
	}
};

// Drawing of meshes that are displayed in first and third person for now
struct PlayerRenderer {
	Shader*			held_block_shad;
	Shader*			held_item_shad;

	PlayerRenderer (Shaders& shaders) {
		held_block_shad    = shaders.compile("held_block");
		held_item_shad     = shaders.compile("held_item");
	}
	void draw (OpenglRenderer& r) {
		ZoneScoped;
		OGL_TRACE("player");

		auto& item = g->player->inventory.toolbar.get_selected();
	
		if (item.id != I_NULL) {
			auto& assets = *g->assets;
	
			float anim_t = g->player->break_block.anim_t != 0 ? g->player->break_block.anim_t : g->player->block_place.anim_t;
			auto a = assets.player.animation.calc(anim_t);

			PipelineState s;
			s.depth_test = true;
			s.blend_enable = true;
			r.state.set(s);

			float3x4 mat = g->player->body_to_world() * translate(a.pos) * a.rot;

			if (item.is_block()) {
				glUseProgram(held_block_shad->prog);

				r.state.bind_textures(held_block_shad, {
					{"tile_textures", r.tile_textures, r.pixelated_sampler}
				});

				held_block_shad->set_uniform("model_to_world", (float4x4)(mat * assets.player.block_mat));

				//
				auto bid = (block_id)item.id;

				auto& tile = assets.block_tiles[bid];
				int meshid = assets.block_meshes.block_meshes[(block_id)item.id];

				static constexpr int MAX_MESH_SLICES = 64;
				float texids[MAX_MESH_SLICES] = {};

				int count;

				if (meshid < 0) {
					held_block_shad->set_uniform("meshid", 0);

					count = 6;
					for (int i=0; i<6; ++i) {
						texids[i] = (float)tile.calc_tex_index((BlockFace)i, 0);
					}
				} else {
					auto& bm_info = assets.block_meshes.meshes[meshid];

					held_block_shad->set_uniform("meshid", bm_info.index);

					count = bm_info.length;
					for (int i=0; i<bm_info.length; ++i) {
						texids[i] = (float)tile.calc_tex_index((BlockFace)0, 0);
					}
				}

				glUniform1fv(held_block_shad->get_uniform_location("texids[0]"), ARRLEN(texids), texids);

				glBindVertexArray(r.dummy_vao);
				glDrawArrays(GL_TRIANGLES, 0, count*6);
			} else {
				glUseProgram(held_item_shad->prog);
			
				r.state.bind_textures(held_item_shad, {
					{"tile_textures", r.tile_textures, r.pixelated_sampler}
				});

				auto id = (item_id)item.id;

				held_item_shad->set_uniform("model_to_world", (float4x4)(mat * assets.player.tool_mat));
				held_item_shad->set_uniform("texid", (float)ITEM_TILES[id - MAX_BLOCK_ID]);

				glBindVertexArray(r.mesh_data.vao);
				draw_submesh(r.item_meshes[id - MAX_BLOCK_ID]);
			}
		}
	}
};

} // namespace gl
