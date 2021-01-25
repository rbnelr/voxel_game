#include "common.hpp"
#include "opengl_renderer.hpp"

#include "GLFW/glfw3.h" // include after glad

namespace gl {


void OpenglRenderer::frame_begin (GLFWwindow* window, kiss::ChangedFiles& changed_files) {
	ctx.imgui_begin();

	shaders.update_recompilation(changed_files, wireframe);
}

void OpenglRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {

	{
		OGL_TRACE("upload_remeshed");
		chunk_renderer.upload_remeshed(game.world->chunks);
	}

	{
		state.override_poly = wireframe;
		state.override_cull = wireframe && wireframe_backfaces;
		state.override_state.poly_mode = POLY_LINE;
		state.override_state.culling = false;

		state.set_default();
	}

	{
		CommonUniforms u = { I, game, I.window_size };
		upload_bind_ubo(common_uniforms, 0, &u, sizeof(u));
	}

	glViewport(0,0, I.window_size.x, I.window_size.y);
	glScissor(0,0, I.window_size.x, I.window_size.y);

	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	PipelineState s;

	{
		OGL_TRACE("test");

		state.set(s);
		glUseProgram(test->prog);

		glBindVertexArray(dummy_vao);

		glDrawArrays(GL_TRIANGLES, 0, 6 * 1024);
	}

	//
	bind_ubo(block_meshes_ubo, 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);
	glBindSampler(0, tile_sampler);

	chunk_renderer.draw_chunks(*this, game);

	//
	debug_draw.draw(*this);

	ctx.imgui_draw();

	TracyGpuCollect;

	glfwSwapBuffers(window);
}

void glDebugDraw::draw (OpenglRenderer& r) {
	OGL_TRACE("debug_draw");

	PipelineState s;
	s.depth_test = true;
	s.depth_write = false;
	s.blend_enable = true;

	{
		OGL_TRACE("draw lines");

		r.state.set(s);
		glUseProgram(shad_lines->prog);

		glBindVertexArray(vbo_lines.vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_lines.vbo);

		GLsizeiptr vertex_count = g_debugdraw.lines.size() * sizeof(DebugDraw::LineVertex);
		glBufferData(GL_ARRAY_BUFFER, vertex_count, nullptr, GL_STREAM_DRAW);
		if (vertex_count > 0) {
			glBufferData(GL_ARRAY_BUFFER, vertex_count, g_debugdraw.lines.data(), GL_STREAM_DRAW);

			glDrawArrays(GL_LINES, 0, (GLsizei)vertex_count);
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
}

void OpenglRenderer::upload_static_data () {

	upload_ubo(block_meshes_ubo,
		g_assets.block_meshes.slices.data(),
		g_assets.block_meshes.slices.size() * sizeof(g_assets.block_meshes.slices[0]));

	{
		Image<srgba8> img;
		img.load_from_file("textures/atlas.png", &img);
		assert(img.size == int2(16*TILEMAP_SIZE));
	
		// place layers at y dir so ot make the memory contiguous
		Image<srgba8> img_arr (int2(16, 16 * TILEMAP_SIZE.x * TILEMAP_SIZE.y));
		{ // convert texture atlas/tilemap into texture array for proper sampling in shader
			for (int y=0; y<TILEMAP_SIZE.y; ++y) {
				for (int x=0; x<TILEMAP_SIZE.x; ++x) {
					Image<srgba8>::blit_rect(
						img, int2(x,y)*16,
						img_arr, int2(0, (y * TILEMAP_SIZE.x + x) * 16),
						16);
				}
			}
		}
		

		glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);

		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, 16, 16, TILEMAP_SIZE.x * TILEMAP_SIZE.y, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, img_arr.pixels);

		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}
}

} // namespace gl
