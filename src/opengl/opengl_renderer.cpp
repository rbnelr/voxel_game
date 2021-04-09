#include "common.hpp"
#include "opengl_renderer.hpp"

#include "GLFW/glfw3.h" // include after glad

namespace gl {

// Repeat reloading of assets because reacting to filechanges often fails because of half-written files
template <typename FUNC>
bool try_reloading (FUNC loadfunc) {
	int max_retries = 100;
	int retry_delay = 10; // ms

	for (int i=0; i<max_retries; ++i) {
		Sleep(retry_delay); // start out with a delay in hopes of getting a working file the first time

		if (loadfunc())
			return true; // success
	}
	return false; // fail
}

//// OpenglRenderer
void OpenglRenderer::frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files) {
	ctx.imgui_begin();

	shaders.update_recompilation(changed_files, wireframe);

	if (changed_files.contains("textures/atlas.png", FILE_ADDED|FILE_MODIFIED|FILE_RENAMED_NEW_NAME)) {
		clog(INFO, "[OpenglRenderer] Reload textures due to file change");
		try_reloading([&] () { return load_static_data(); });
	}

	framebuffer.update(I.window_size);
}

void OpenglRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {
	ImGui::Begin("Debug");

	chunk_renderer.upload_remeshed(game.chunks);
	raytracer.upload_changes(*this, game, I);

	glLineWidth(line_width);
	{
		OGL_TRACE("set state defaults");

		state.override_poly = wireframe;
		state.override_cull = wireframe && wireframe_backfaces;
		state.override_state.poly_mode = POLY_LINE;
		state.override_state.culling = false;

		state.set_default();
	}

	{
		OGL_TRACE("binds");

		//{ // init debug_draw.indirect_lines
		//	glDrawArraysIndirectCommand cmd = {};
		//	cmd.instanceCount = 1;
		//	glBindBuffer(GL_ARRAY_BUFFER, debug_draw.indirect_lines.vbo);
		//	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cmd), &cmd);
		//	glBindBuffer(GL_ARRAY_BUFFER, 0);
		//}

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, debug_draw.indirect_lines.vbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, block_meshes_ssbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, block_tiles_ssbo);

		glActiveTexture(GL_TEXTURE0+TILE_TEXTURES);
		glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);
		glBindSampler(TILE_TEXTURES, tile_sampler);

		glActiveTexture(GL_TEXTURE0+HEAT_GRADIENT);
		glBindTexture(GL_TEXTURE_2D, heat_gradient);
		glBindSampler(HEAT_GRADIENT, normal_sampler);
	}

	{
		OGL_TRACE("3d draws");
		{
			memset(&common_uniforms, 0, sizeof(common_uniforms)); // zero padding
			common_uniforms.view.set(game.view);
			common_uniforms.view.viewport_size = (float2)framebuffer.size;
			upload_bind_ubo(common_uniforms_ubo, 0, &common_uniforms, sizeof(common_uniforms));

			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
			glViewport(0,0, framebuffer.size.x, framebuffer.size.y);
			glScissor(0,0, framebuffer.size.x, framebuffer.size.y);

			glClearColor(0,0,0,1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		if (raytracer.enable_rt_lighting)
			raytracer.compute_lighting(*this, game);

		if (raytracer.enable)
			raytracer.draw(*this, game);

		// draw before chunks so it shows through transparent blocks
		if (game.player.selected_block)
			block_highl.draw(*this, game.player.selected_block);

		if (!raytracer.enable)
			chunk_renderer.draw_chunks(*this, game);

		debug_draw.draw(*this);

		if (!game.activate_flycam && !game.player.third_person) {
			// clear depth buffer to draw first person items on top of everything to avoid clipping into walls
			glClear(GL_DEPTH_BUFFER_BIT); // NOTE: clobbers the depth buffer, if it's still needed for SSAO etc. we might want to use a second depth buffer instead
		}

		// draws first and third person player items
		player_rederer.draw(*this, game);
	}

	{
		OGL_TRACE("post passes");
		
		{
			OGL_TRACE("generate framebuffer mips");
			glBindTexture(GL_TEXTURE_2D, framebuffer.color);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if (bloom_renderer.enable)
			bloom_renderer.apply_bloom(*this, framebuffer);

		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glViewport(0,0, I.window_size.x, I.window_size.y);
			glScissor(0,0, I.window_size.x, I.window_size.y);

			{
				common_uniforms.view.viewport_size = (float2)I.window_size;
				upload_bind_ubo(common_uniforms_ubo, 0, &common_uniforms, sizeof(common_uniforms));
			}
		}

		{
			OGL_TRACE("postprocess");
			glUseProgram(post_shad->prog);

			PipelineState s;
			s.blend_enable = false;
			s.depth_test = false;
			s.depth_write = false;
			state.set_no_override(s);

			GLuint textures[] = { framebuffer.color, bloom_renderer.passes[1].color };
			const char* textures_uniforms[] = { "main_color", "bloom" };
			for (int i=0; i<ARRLEN(textures); ++i) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, textures[i]);
				glBindSampler(i, post_sampler);
				glUniform1i(post_shad->get_uniform_location(textures_uniforms[i]), i);
			}

			post_shad->set_uniform("enable_bloom", bloom_renderer.enable);
			post_shad->set_uniform("exposure", bloom_renderer.exposure);

			glDrawArrays(GL_TRIANGLES, 0, 3); // full screen triangle
		}
	}

	{
		OGL_TRACE("ui draws");

		{
			glActiveTexture(GL_TEXTURE0+TILE_TEXTURES);
			glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);
			glBindSampler(TILE_TEXTURES, tile_sampler);

			glActiveTexture(GL_TEXTURE0+GUI_ATLAS);
			glBindTexture(GL_TEXTURE_2D, gui_atlas);
			glBindSampler(GUI_ATLAS, gui_renderer.gui_sampler);
		}

		if (trigger_screenshot && !screenshot_hud)	take_screenshot(I.window_size);

		if (!game.activate_flycam || game.creative_mode)
			gui_renderer.draw_gui(*this, I, game);

		ImGui::End();
		ctx.imgui_draw();

		if (trigger_screenshot && screenshot_hud)	take_screenshot(I.window_size);
		trigger_screenshot = false;
	}

	TracyGpuCollect;

	glfwSwapBuffers(window);
}

//// BlockHighlight
void BlockHighlight::draw (OpenglRenderer& r, SelectedBlock& block) {
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

	shad->set_uniform("block_pos", (float3)block.hit.pos);
	shad->set_uniform("model_to_clip", face_rotation[0]);
	shad->set_uniform("tint", srgba(40,40,40,240));

	glBindVertexArray(r.mesh_data.vao);
	draw_submesh(block_highl.block_highlight);
	
	if (block.hit.face >= 0) {
		shad->set_uniform("face_rotation", face_rotation[ (BlockFace)(block.hit.face >= 0 ? block.hit.face : 0) ]);
		draw_submesh(block_highl.face_highlight);
	}
}

//// GuiRenderer
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

void GuiRenderer::draw_gui_quad (float2 const& pos, float2 const& size, AtlasUVs const& uv) {
	GUIVertex* verts = push_quads(1);
	for (int i=0; i<4; ++i) {
		verts[i].pos    = pos + QUAD_CORNERS[i] * size;
		verts[i].normal = float3(0, 0, 1);
		verts[i].uvi    = float3((uv.pos + QUAD_UV[i] * uv.size) * (1.0f/256), -1);
	}
}
void GuiRenderer::draw_item_quad (float2 const& pos, float2 const& size, item_id item) {
	if (item < MAX_BLOCK_ID) {
		GUIVertex* verts = push_quads(3);
		
		float tile_idxs[] = {
			(float)g_assets.block_tiles[item].sides[BF_NEG_X],
			(float)g_assets.block_tiles[item].sides[BF_NEG_Y],
			(float)g_assets.block_tiles[item].sides[BF_POS_Z],
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

void GuiRenderer::update_gui (Input& I, Game& game) {
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
		auto& backpack = game.player.inventory.backpack;

		float2 start = anchor -(float2)int2(w-1,h-1)/2 * frame_sz;

		int2 hovered_idx = roundi((I.cursor_pos - start) / frame_sz);

		for (int i=0; i<count; ++i) {
			int2 idx2 = int2(i%w, h-1 -i/w);

			bool hovered = I.cursor_enabled && idx2 == hovered_idx;
			if (hovered && clicked) {
				std::swap(items[i], game.player.inventory.hand);
				clicked = false;
			}
			auto& tex = hovered ?
				//(game.player.inventory.hand.id != I_NULL ? frame_grabbed_uv : frame_highl_uv) :
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
		auto& toolbar = game.player.inventory.toolbar;

		float2 anchor = float2(screen_center.x, frame_sz/2 +1*sz);
		draw_items_grid(toolbar.slots, ARRLEN(toolbar.slots), ARRLEN(toolbar.slots), 1, anchor, toolbar.selected);
	}

	if (game.player.inventory.is_open) { // backpack
		auto& backpack = game.player.inventory.backpack;

		static int w = 10;
		ImGui::DragInt("backpack gui w", &w, 0.05f);

		float2 anchor = screen_center;
		draw_items_grid(&backpack.slots[0][0], 10*10, w, 10, anchor);

		if (I.cursor_enabled && game.player.inventory.hand.id != I_NULL)
			draw_item_quad(I.cursor_pos +(item_sz/2), item_sz, game.player.inventory.hand.id);
	}
}
void GuiRenderer::draw_gui (OpenglRenderer& r, Input& I, Game& game) {
	ZoneScoped;
	OGL_TRACE("gui");
	
	update_gui(I, game);

	glBindVertexArray(gui_ib.vao);
	stream_buffer(gui_ib, gui_vertex_data, gui_index_data);

	if (gui_vertex_data.size() > 0) {
		PipelineState s;
		s.depth_test = false;
		s.blend_enable = true;
		r.state.set(s);
		glUseProgram(gui_shad->prog);

		glUniform1i(gui_shad->get_uniform_location("tex"), OpenglRenderer::GUI_ATLAS);
		glUniform1i(gui_shad->get_uniform_location("tile_textures"), OpenglRenderer::TILE_TEXTURES);

		glDrawElements(GL_TRIANGLES, (GLsizei)gui_index_data.size(), GL_UNSIGNED_SHORT, (void*)0);
	}
}

//// PlayerRenderer
void PlayerRenderer::draw (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("player");

	auto& item = game.player.inventory.toolbar.get_selected();
	
	if (item.id != I_NULL) {
		auto& assets = g_assets.player;
	
		float anim_t = game.player.break_block.anim_t != 0 ? game.player.break_block.anim_t : game.player.block_place.anim_t;
		auto a = assets.animation.calc(anim_t);

		PipelineState s;
		s.depth_test = true;
		s.blend_enable = true;
		r.state.set(s);

		float3x4 mat = game.player.head_to_world * translate(a.pos) * a.rot;

		if (item.is_block()) {
			glUseProgram(held_block_shad->prog);

			glUniform1i(held_block_shad->get_uniform_location("tile_textures"), OpenglRenderer::TILE_TEXTURES);

			held_block_shad->set_uniform("model_to_world", (float4x4)(mat * assets.block_mat));

			//
			auto bid = (block_id)item.id;

			auto& tile = g_assets.block_tiles[bid];
			int meshid = g_assets.block_meshes.block_meshes[(block_id)item.id];

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
				auto& bm_info = g_assets.block_meshes.meshes[meshid];

				held_block_shad->set_uniform("meshid", bm_info.index);

				count = bm_info.length;
				for (int i=0; i<bm_info.length; ++i) {
					texids[i] = (float)tile.calc_tex_index((BlockFace)0, 0);
				}
			}

			glUniform1fv(held_block_shad->get_uniform_location("texids[0]"), ARRLEN(texids), texids);

			glBindVertexArray(dummy_vao);
			glDrawArrays(GL_TRIANGLES, 0, count*6);
		} else {
			glUseProgram(held_item_shad->prog);
		
			glUniform1i(held_item_shad->get_uniform_location("tile_textures"), OpenglRenderer::TILE_TEXTURES);

			auto id = (item_id)item.id;

			held_item_shad->set_uniform("model_to_world", (float4x4)(mat * assets.tool_mat));
			held_item_shad->set_uniform("texid", (float)ITEM_TILES[id - MAX_BLOCK_ID]);

			glBindVertexArray(r.mesh_data.vao);
			draw_submesh(r.item_meshes[id - MAX_BLOCK_ID]);
		}
	}
}

//// glDebugDraw
void glDebugDraw::draw (OpenglRenderer& r) {
	OGL_TRACE("debug_draw");

	PipelineState s;
	s.depth_test = true;
	s.depth_write = true;
	s.blend_enable = true;

	PipelineState s_occluded;
	s_occluded.depth_test = true;
	s_occluded.depth_func = DEPTH_BEHIND;
	s_occluded.depth_write = true;
	s_occluded.blend_enable = true;

	{
		OGL_TRACE("draw lines");
	
		glBindVertexArray(vbo_lines.vao);
		stream_buffer(vbo_lines, g_debugdraw.lines);
	
		if (g_debugdraw.lines.size() > 0) {
			{
				OGL_TRACE("normal");
	
				r.state.set(s);
				glUseProgram(shad_lines->prog);
	
				glDrawArrays(GL_LINES, 0, (GLsizei)g_debugdraw.lines.size());
			}
			if (draw_occluded) { // lines occluded by geometry
				OGL_TRACE("occluded");
	
				r.state.set(s_occluded);
				glUseProgram(shad_lines_occluded->prog);
	
				shad_lines_occluded->set_uniform("occluded_alpha", occluded_alpha);
	
				glDrawArrays(GL_LINES, 0, (GLsizei)g_debugdraw.lines.size());
			}
		}
	}

	{
		OGL_TRACE("draw lines indirect");

		glBindVertexArray(indirect_lines.vao);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_lines.vbo);

		r.state.set(s);
		glUseProgram(shad_lines->prog);

		glDrawArraysIndirect(GL_LINES, (void*)0);

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	}

	{
		OGL_TRACE("draw tris");
	
		glBindVertexArray(vbo_tris.vao);
		stream_buffer(vbo_tris, g_debugdraw.tris);
	
		if (g_debugdraw.tris.size() > 0) {
			r.state.set(s);
			glUseProgram(shad_tris->prog);
	
			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g_debugdraw.tris.size());
		}
	}

	{
		OGL_TRACE("draw wire cubes");
	
		glBindVertexArray(vbo_wire_cube.vao);
		reupload_vbo(vbo_wire_cube.instance_vbo, g_debugdraw.wire_cubes.data(), g_debugdraw.wire_cubes.size(), GL_STREAM_DRAW);
		
		if (g_debugdraw.wire_cubes.size() > 0) {
			r.state.set(s);
			glUseProgram(shad_wire_cube->prog);
	
			glDrawElementsInstanced(GL_LINES, ARRLEN(DebugDraw::_wire_indices), GL_UNSIGNED_SHORT,
				(void*)0, (GLsizei)g_debugdraw.wire_cubes.size());
		}
	}
}

//// OpenglRenderer
bool OpenglRenderer::load_textures (GenericVertexData& mesh_data) {

	{
		Image<srgba8> img;
		if (!img.load_from_file("textures/atlas.png", &img))
			return false;

		// place layers at y dir so ot make the memory contiguous
		Image<srgba8> img_arr (int2(16, 16 * TILEMAP_SIZE.x * TILEMAP_SIZE.y));
		// convert texture atlas/tilemap into texture array for proper sampling in shader
		for (int y=0; y<TILEMAP_SIZE.y; ++y)
		for (int x=0; x<TILEMAP_SIZE.x; ++x) {
			Image<srgba8>::blit_rect(
				img, int2(x,y)*16,
				img_arr, int2(0, ((15-y) * TILEMAP_SIZE.x + x) * 16),
				16);
		}

		{
			glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);

			glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, 16, 16, TILEMAP_SIZE.x * TILEMAP_SIZE.y, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, img_arr.pixels);

			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		}

		{
			auto get_pixel = [&] (int x, int y, int tileid) {
				return img_arr.pixels[tileid * 16*16 + y*16 + x];
			};

			GenericVertexData data;
			item_meshes = generate_item_meshes(&mesh_data, get_pixel, ITEM_COUNT, ITEM_TILES);
		}
	}

	upload_texture(heat_gradient, "textures/heat_gradient.png");
	upload_texture(gui_atlas, "textures/gui.png");

	return true;
}

bool OpenglRenderer::load_static_data () {

	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_meshes_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, g_assets.block_meshes.slices.size() * sizeof(g_assets.block_meshes.slices[0]),
		                                       g_assets.block_meshes.slices.data(), GL_STREAM_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, block_meshes_ssbo);
	}
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_tiles_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, g_assets.block_tiles.size() * sizeof(g_assets.block_tiles[0]),
		                                       g_assets.block_tiles.data(), GL_STREAM_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, block_tiles_ssbo);
	}

	GenericVertexData data;

	block_highl.block_highl = load_block_highlight_mesh(&data);

	if (!load_textures(data))
		return false;

	upload_buffer(mesh_data, data.vertices, data.indices);

	return true;
}

} // namespace gl
