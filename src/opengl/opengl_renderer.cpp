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
		gui_renderer.draw(*this, I, game);

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

	glBindVertexArray(mesh.vao);
	draw_submesh(mesh, block_highl);
	
	if (block.hit.face >= 0) {
		shad->set_uniform("face_rotation", face_rotation[ (BlockFace)(block.hit.face >= 0 ? block.hit.face : 0) ]);
		draw_submesh(mesh, face_highl);
	}
}

void GUIRenderer::draw_gui_quad (Rect const& r, AtlasUVs const& uv) {
	GUIVertex verts[4];
	verts[0] = { r.pos + float2(0,0) * r.size, float3((uv.pos + float2(0,0) * uv.size) * (1.0f/256), -1) };
	verts[1] = { r.pos + float2(1,0) * r.size, float3((uv.pos + float2(1,0) * uv.size) * (1.0f/256), -1) };
	verts[2] = { r.pos + float2(0,1) * r.size, float3((uv.pos + float2(0,1) * uv.size) * (1.0f/256), -1) };
	verts[3] = { r.pos + float2(1,1) * r.size, float3((uv.pos + float2(1,1) * uv.size) * (1.0f/256), -1) };
	push_quad(verts);
}

const float3x3 gui_block_rot = rotate3_X(deg(-78.0f)) * rotate3_Z(deg(12.0f));
void GUIRenderer::draw_item_quad (Rect const& r, item_id item) {
	GUIVertex verts[4];
	if (item < MAX_BLOCK_ID) {

		static constexpr float scale = 0.39f;

		float tile_idx = (float)g_assets.block_tiles[item].sides[BF_NEG_X];
		verts[0] = { r.pos + (float2)(gui_block_rot * float3(-1,+1,-1)*scale + 0.5f) * r.size, float3(0,0, tile_idx) };
		verts[1] = { r.pos + (float2)(gui_block_rot * float3(-1,-1,-1)*scale + 0.5f) * r.size, float3(1,0, tile_idx) };
		verts[2] = { r.pos + (float2)(gui_block_rot * float3(-1,+1,+1)*scale + 0.5f) * r.size, float3(0,1, tile_idx) };
		verts[3] = { r.pos + (float2)(gui_block_rot * float3(-1,-1,+1)*scale + 0.5f) * r.size, float3(1,1, tile_idx) };
		push_quad(verts);

		tile_idx = (float)g_assets.block_tiles[item].sides[BF_NEG_Y];
		verts[0] = { r.pos + (float2)(gui_block_rot * float3(-1,-1,-1)*scale + 0.5f) * r.size, float3(0,0, tile_idx) };
		verts[1] = { r.pos + (float2)(gui_block_rot * float3(+1,-1,-1)*scale + 0.5f) * r.size, float3(1,0, tile_idx) };
		verts[2] = { r.pos + (float2)(gui_block_rot * float3(-1,-1,+1)*scale + 0.5f) * r.size, float3(0,1, tile_idx) };
		verts[3] = { r.pos + (float2)(gui_block_rot * float3(+1,-1,+1)*scale + 0.5f) * r.size, float3(1,1, tile_idx) };
		push_quad(verts);

		tile_idx = (float)g_assets.block_tiles[item].sides[BF_TOP];
		verts[0] = { r.pos + (float2)(gui_block_rot * float3(-1,-1,+1)*scale + 0.5f) * r.size, float3(0,0, tile_idx) };
		verts[1] = { r.pos + (float2)(gui_block_rot * float3(+1,-1,+1)*scale + 0.5f) * r.size, float3(1,0, tile_idx) };
		verts[2] = { r.pos + (float2)(gui_block_rot * float3(-1,+1,+1)*scale + 0.5f) * r.size, float3(0,1, tile_idx) };
		verts[3] = { r.pos + (float2)(gui_block_rot * float3(+1,+1,+1)*scale + 0.5f) * r.size, float3(1,1, tile_idx) };
		push_quad(verts);

	} else {
		float tile_idx = (float)ITEM_TILES[item - MAX_BLOCK_ID];
		verts[0] = { r.pos + float2(0,0) * r.size, float3(0,0, tile_idx) };
		verts[1] = { r.pos + float2(1,0) * r.size, float3(1,0, tile_idx) };
		verts[2] = { r.pos + float2(0,1) * r.size, float3(0,1, tile_idx) };
		verts[3] = { r.pos + float2(1,1) * r.size, float3(1,1, tile_idx) };
		push_quad(verts);
	}
}

void GUIRenderer::draw_gui (Input& I, Game& game) {
	vertex_data.clear();

	float2 screen_center = round((float2)I.window_size /2);
	
	// calc pixel coords

	if (crosshair) { // crosshair
		draw_gui_quad(calc(screen_center, 0, 32), crosshair_uv);
	}
	{ // toolbar
		float frame_sz = 16+4;

		int toolbar_count = ARRLEN(game.player.inventory.toolbar.slots);
		auto& toolbar = game.player.inventory.toolbar;
		
		float2 anchor = float2(screen_center.x, 0);
		float2 start = float2(-(float)toolbar_count/2 * frame_sz, 1) + frame_sz/2;

		auto draw_slot = [&] (int i, AtlasUVs const& uv) {
			draw_gui_quad(calc(anchor, start + float2((float)i * frame_sz, 0), 32), uv);
		};
		auto draw_item = [&] (int i, Item const& item) {
			draw_item_quad(calc(anchor, start + float2((float)i * frame_sz, 0), 16), item.id);
		};

		for (int i=0; i<toolbar_count; ++i)
			draw_slot(i, toolbar_uv);

		for (int i=0; i<toolbar_count; ++i)
			draw_item(i, toolbar.slots[i]);

		draw_slot(toolbar.selected, toolbar_selected_uv);
	}
}
void GUIRenderer::draw (OpenglRenderer& r, Input& I, Game& game) {
	ZoneScoped;
	OGL_TRACE("gui");
	
	draw_gui(I, game);

	glBindVertexArray(gui_vbo.vao);
	stream_vbo(gui_vbo.vbo, vertex_data);

	if (vertex_data.size() > 0) {
		PipelineState s;
		s.depth_test = false;
		s.blend_enable = true;
		r.state.set(s);
		glUseProgram(shad->prog);

		glUniform1i(shad->get_uniform_location("tex"), OpenglRenderer::GUI_ATLAS);
		glUniform1i(shad->get_uniform_location("tile_textures"), OpenglRenderer::TILE_TEXTURES);

		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_data.size());
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
		stream_vbo(vbo_lines.vbo, g_debugdraw.lines);
	
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
		stream_vbo(vbo_tris.vbo, g_debugdraw.tris);
	
		if (g_debugdraw.tris.size() > 0) {
			r.state.set(s);
			glUseProgram(shad_tris->prog);
	
			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g_debugdraw.tris.size());
		}
	}

	{
		OGL_TRACE("draw wire cubes");
	
		glBindVertexArray(vbo_wire_cube.vao);
		stream_vbo(vbo_wire_cube.instance_vbo, g_debugdraw.wire_cubes);
		
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
