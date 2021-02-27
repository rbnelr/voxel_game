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
	bind_ubo(block_meshes_ubo, 1);
	//bind_ubo(block_tiles_ubo, 2);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);
	glBindSampler(0, tile_sampler);

	glActiveTexture(GL_TEXTURE0+1);
	glBindTexture(GL_TEXTURE_2D, heat_gradient);
	glBindSampler(1, normal_sampler);


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

	ImGui::End();
	ctx.imgui_draw();

	if (trigger_screenshot && screenshot_hud)	take_screenshot(I.window_size);
	trigger_screenshot = false;

	TracyGpuCollect;

	glfwSwapBuffers(window);
}


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

void BlockHighlight::draw (OpenglRenderer& r, SelectedBlock& block) {
	OGL_TRACE("block_highlight");

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

void glDebugDraw::draw (OpenglRenderer& r) {
	OGL_TRACE("debug_draw");

	PipelineState s;
	s.depth_test = true;
	s.depth_write = false;
	s.blend_enable = true;

	PipelineState s_occluded;
	s_occluded.depth_test = true;
	s_occluded.depth_func = DEPTH_BEHIND;
	s_occluded.depth_write = false;
	s_occluded.blend_enable = true;

	{
		OGL_TRACE("draw lines");

		glBindVertexArray(vbo_lines.vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_lines.vbo);

		GLsizeiptr vertex_count = g_debugdraw.lines.size() * sizeof(DebugDraw::LineVertex);
		glBufferData(GL_ARRAY_BUFFER, vertex_count, nullptr, GL_STREAM_DRAW);
		if (vertex_count > 0) {
			glBufferData(GL_ARRAY_BUFFER, vertex_count, g_debugdraw.lines.data(), GL_STREAM_DRAW);

			{ // lines in front of geometry
				OGL_TRACE("normal");

				r.state.set(s);
				glUseProgram(shad_lines->prog);

				glDrawArrays(GL_LINES, 0, (GLsizei)vertex_count);
			}
			if (draw_occluded) { // lines occluded by geometry
				OGL_TRACE("occluded");

				r.state.set(s_occluded);
				glUseProgram(shad_lines_occluded->prog);

				shad_lines_occluded->set_uniform("occluded_alpha", occluded_alpha);

				glDrawArrays(GL_LINES, 0, (GLsizei)vertex_count);
			}
		}
	}

	{
		OGL_TRACE("draw tris");

		r.state.set(s);
		glUseProgram(shad_tris->prog);

		glBindVertexArray(vbo_tris.vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_tris.vbo);

		GLsizeiptr vertex_count = g_debugdraw.tris.size() * sizeof(DebugDraw::TriVertex);
		glBufferData(GL_ARRAY_BUFFER, vertex_count, nullptr, GL_STREAM_DRAW);
		if (vertex_count > 0) {
			glBufferData(GL_ARRAY_BUFFER, vertex_count, g_debugdraw.tris.data(), GL_STREAM_DRAW);

			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
		}
	}

	{
		OGL_TRACE("draw wire cubes");

		glBindVertexArray(vbo_wire_cube.vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_wire_cube.instance_vbo);

		GLsizeiptr instance_count = g_debugdraw.wire_cubes.size() * sizeof(DebugDraw::Instance);
		glBufferData(GL_ARRAY_BUFFER, instance_count, nullptr, GL_STREAM_DRAW);
		if (instance_count > 0) {
			glBufferData(GL_ARRAY_BUFFER, instance_count, g_debugdraw.wire_cubes.data(), GL_STREAM_DRAW);

			r.state.set(s);
			glUseProgram(shad_wire_cube->prog);

			glDrawElementsInstanced(GL_LINES, ARRLEN(DebugDraw::_wire_indices), GL_UNSIGNED_SHORT, (void*)0, (GLsizei)instance_count);
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

	return true;
}

void OpenglRenderer::load_static_data () {

	upload_ubo(block_meshes_ubo,
		g_assets.block_meshes.slices.data(),
		g_assets.block_meshes.slices.size() * sizeof(g_assets.block_meshes.slices[0]));

	//upload_ubo(block_tiles_ubo,
	//	g_assets.block_tiles.data(),
	//	g_assets.block_tiles.size() * sizeof(g_assets.block_tiles[0]));

	load_textures();
}

} // namespace gl
