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

void OpenglRenderer::frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files) {
	ctx.imgui_begin();

	shaders.update_recompilation(changed_files, wireframe);

	if (changed_files.contains("textures/atlas.png", FILE_ADDED|FILE_MODIFIED|FILE_RENAMED_NEW_NAME)) {
		clog(INFO, "[OpenglRenderer] Reload textures due to file change");
		try_reloading([&] () { return load_textures(); });
	}

	framebuffer.update(I.window_size);
}

void OpenglRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {
	ImGui::Begin("Debug");

	{
		OGL_TRACE("upload_remeshed");
		chunk_renderer.upload_remeshed(game.chunks);
	}

	glLineWidth(line_width);
	{
		state.override_poly = wireframe;
		state.override_cull = wireframe && wireframe_backfaces;
		state.override_state.poly_mode = POLY_LINE;
		state.override_state.culling = false;

		state.set_default();
	}

	{
		CommonUniforms u = { I, game, framebuffer.size };
		upload_bind_ubo(common_uniforms, 0, &u, sizeof(u));
	}

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
	glViewport(0,0, framebuffer.size.x, framebuffer.size.y);
	glScissor(0,0, framebuffer.size.x, framebuffer.size.y);

	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	PipelineState s;

	//
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, block_meshes_ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, block_tiles_ssbo);

	glActiveTexture(GL_TEXTURE0+TILE_TEXTURES);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);
	glBindSampler(TILE_TEXTURES, tile_sampler);

	glActiveTexture(GL_TEXTURE0+GUI_ATLAS);
	glBindTexture(GL_TEXTURE_2D, gui_atlas);
	glBindSampler(GUI_ATLAS, gui_sampler);

	glActiveTexture(GL_TEXTURE0+HEAT_GRADIENT);
	glBindTexture(GL_TEXTURE_2D, heat_gradient);
	glBindSampler(HEAT_GRADIENT, normal_sampler);


	// draw before chunks so it shows through transparent blocks
	if (game.player.selected_block)
		block_highl.draw(*this, game.player.selected_block);

	chunk_renderer.draw_chunks(*this, game);

	if (raytracer.enable)
		raytracer.draw(*this, game);

	//
	debug_draw.draw(*this);

	{
		CommonUniforms u = { I, game, I.window_size };
		upload_bind_ubo(common_uniforms, 0, &u, sizeof(u));
	}
	{
		OGL_TRACE("framebuffer.blit");
		framebuffer.blit(I.window_size);
	}
	glViewport(0,0, I.window_size.x, I.window_size.y);
	glScissor(0,0, I.window_size.x, I.window_size.y);

	if (trigger_screenshot && !screenshot_hud)	take_screenshot(I.window_size);

	if (!game.activate_flycam || game.creative_mode)
		gui_renderer.draw_gui(*this, I, game);

	ImGui::End();
	ctx.imgui_draw();

	if (trigger_screenshot && screenshot_hud)	take_screenshot(I.window_size);
	trigger_screenshot = false;

	TracyGpuCollect;

	glfwSwapBuffers(window);
}

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
	shad->set_uniform("face_rotation", face_rotation[0]);
	shad->set_uniform("tint", srgba(40,40,40,240));

	glBindVertexArray(mesh.ib.vao);
	draw_submesh(mesh, block_highl);
	
	if (block.hit.face >= 0) {
		shad->set_uniform("face_rotation", face_rotation[ (BlockFace)(block.hit.face >= 0 ? block.hit.face : 0) ]);
		draw_submesh(mesh, face_highl);
	}
}

constexpr float2 _quad[] = { float2(0,0), float2(1,0), float2(0,1), float2(1,1) };
void GuiRenderer::draw_gui_quad (float2 const& pos, float2 const& size, AtlasUVs const& uv) {
	GUIVertex* verts = push_quads(1);
	for (int i=0; i<4; ++i) {
		verts[i].pos    = pos + _quad[i] * size;
		verts[i].normal = float3(0, 0, 1);
		verts[i].uv     = float3((uv.pos + _quad[i] * uv.size) * (1.0f/256), -1);
	}
}

std::array<GuiRenderer::GUIVertex, 12> build_gui_block_mesh () {
	static constexpr float scale = 0.39f;
	const float3x3 rot = rotate3_X(deg(-78.0f)) * rotate3_Z(deg(12.0f));
	std::array<GuiRenderer::GUIVertex, 12> verts;

	static constexpr float3 corners[] = {
		float3(-1,+1,-1),
		float3(-1,-1,-1),
		float3(-1,+1,+1),
		float3(-1,-1,+1),

		float3(-1,-1,-1),
		float3(+1,-1,-1),
		float3(-1,-1,+1),
		float3(+1,-1,+1),

		float3(-1,-1,+1),
		float3(+1,-1,+1),
		float3(-1,+1,+1),
		float3(+1,+1,+1),
	};
	static constexpr float3 normals[] = {
		float3(-1,0,0),
		float3(0,-1,0),
		float3(0,0,+1),
	};

	for (int i=0; i<12; ++i) {
		verts[i] = { (float2)(rot * corners[i]*scale + 0.5f), rot * normals[i/4], float3(_quad[i%4], 0) };
	}
	return verts;
}
std::array<GuiRenderer::GUIVertex, 12> _gui_block_mesh = build_gui_block_mesh();

void GuiRenderer::draw_item_quad (float2 const& pos, float2 const& size, item_id item) {
	if (item < MAX_BLOCK_ID) {
		GUIVertex* verts = push_quads(3);
		
		static constexpr float scale = 0.39f;
		
		float tile_idxs[] = {
			(float)g_assets.block_tiles[item].sides[BF_NEG_X],
			(float)g_assets.block_tiles[item].sides[BF_NEG_Y],
			(float)g_assets.block_tiles[item].sides[BF_TOP],
		};
		
		for (int i=0; i<12; ++i) {
			verts[i] = _gui_block_mesh[i];
			verts[i].pos    = pos + _gui_block_mesh[i].pos * size;
			verts[i].normal =       _gui_block_mesh[i].normal;
			verts[i].uv     = float3(_gui_block_mesh[i].uv.x, _gui_block_mesh[i].uv.y, tile_idxs[i/4]);
		}

	} else {
		GUIVertex* verts = push_quads(1);
		
		float tile_idx = (float)ITEM_TILES[item - MAX_BLOCK_ID];
		for (int i=0; i<4; ++i) {
			verts[i].pos    = pos + _quad[i] * size;
			verts[i].normal = float3(0, 0, 1);
			verts[i].uv     = float3(_quad[i], tile_idx);
		}
	}
}

void GuiRenderer::generate_gui (Input& I, Game& game) {
	gui_vertex_data.clear();
	gui_index_data.clear();

	float sz = (float)gui_scale;

	float2 screen_center = round((float2)I.window_size /2);
	
	// calc pixel coords

	if (crosshair) { // crosshair
		draw_gui_quad(screen_center -16*sz, 32*sz, crosshair_uv);
	}

	float frame_sz = 16+4;
	bool clicked = I.buttons[MOUSE_BUTTON_LEFT].went_down;

	auto draw_selected_slot = [&] (float2 start, int2 const& idx) {
		float2 pos = start + (float2)idx * frame_sz*sz;
		draw_gui_quad(pos + (frame_sz/2 - 32/2)*sz, 32*sz, toolbar_selected_uv);
	};
	auto draw_slot = [&] (float2 start, int2 const& idx) {
		float2 pos = start + (float2)idx * frame_sz*sz;
		
		float2 offs = I.cursor_pos - pos;
		bool hovered = I.cursor_enabled && offs.x >= 0.0f && offs.y >= 0.0f && offs.x < frame_sz*sz && offs.y < frame_sz*sz;

		draw_gui_quad(pos + (frame_sz/2 - 32/2)*sz, 32*sz, hovered ? toolbar_highl_uv : toolbar_uv);
	};
	auto draw_item = [&] (float2 start, int2 const& idx, Item& item) {
		auto pos = start + (float2)idx * frame_sz*sz;

		float2 offs = I.cursor_pos - pos;
		bool hovered = I.cursor_enabled && offs.x >= 0.0f && offs.y >= 0.0f && offs.x < frame_sz*sz && offs.y < frame_sz*sz;

		if (item.id != I_NULL)
			draw_item_quad(pos + (frame_sz/2 - 16/2)*sz, 16*sz, item.id);

		if (hovered && clicked) {
			std::swap(item, game.player.inventory.hand);
			clicked = false; // make sure faulty selection cannot swap item twice?
		}
	};

	auto draw_items_grid = [&] (Item* items, int count, int w, int h, float2 const& anchor, int selected=-1) {
		auto& backpack = game.player.inventory.backpack;

		float2 start = anchor -((float2)int2(w,h)/2 * frame_sz) * sz;

		for (int i=0; i<count; ++i)
			draw_slot(start, int2(i%w, h-1 -i/w));

		for (int i=0; i<count; ++i)
			draw_item(start, int2(i%w, h-1 -i/w), items[i]);

		if (selected >= 0)
			draw_selected_slot(start, int2(selected%w, h-1 -selected/w));
	};

	{ // toolbar
		auto& toolbar = game.player.inventory.toolbar;

		float2 anchor = float2(screen_center.x, (frame_sz/2 +1) * sz);
		draw_items_grid(toolbar.slots, ARRLEN(toolbar.slots), ARRLEN(toolbar.slots), 1, anchor, toolbar.selected);
	}

	if (game.player.inventory.is_open) { // backpack
		auto& backpack = game.player.inventory.backpack;

		static int w = 10;
		ImGui::DragInt("backpack gui w", &w, 0.05f);

		float2 anchor = screen_center;
		draw_items_grid(&backpack.slots[0][0], 10*10, w, 10, anchor);

		if (I.cursor_enabled && game.player.inventory.hand.id != I_NULL)
			draw_item_quad(I.cursor_pos -4*sz, 16*sz, game.player.inventory.hand.id);
	}
}
void GuiRenderer::draw_gui (OpenglRenderer& r, Input& I, Game& game) {
	ZoneScoped;
	OGL_TRACE("gui");
	
	generate_gui(I, game);

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

bool OpenglRenderer::load_textures () {
	Image<srgba8> img;
	if (!img.load_from_file("textures/atlas.png", &img))
		return false;

	// place layers at y dir so ot make the memory contiguous
	Image<srgba8> img_arr (int2(16, 16 * TILEMAP_SIZE.x * TILEMAP_SIZE.y));
	{ // convert texture atlas/tilemap into texture array for proper sampling in shader
		for (int y=0; y<TILEMAP_SIZE.y; ++y) {
			for (int x=0; x<TILEMAP_SIZE.x; ++x) {
				Image<srgba8>::blit_rect(
					img, int2(x,y)*16,
					img_arr, int2(0, ((15-y) * TILEMAP_SIZE.x + x) * 16),
					16);
			}
		}
	}


	glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, 16, 16, TILEMAP_SIZE.x * TILEMAP_SIZE.y, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, img_arr.pixels);

	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	upload_texture(heat_gradient, "textures/heat_gradient.png");
	upload_texture(gui_atlas, "textures/gui.png");

	return true;
}

void OpenglRenderer::load_static_data () {

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

	load_textures();
}

} // namespace gl
