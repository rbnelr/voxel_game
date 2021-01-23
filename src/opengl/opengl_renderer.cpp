#include "common.hpp"
#include "opengl_renderer.hpp"

#include "GLFW/glfw3.h" // include after glad

namespace gl {


void OpenglRenderer::frame_begin (GLFWwindow* window, kiss::ChangedFiles& changed_files) {
	ctx.imgui_begin();

	shaders.update_recompilation(changed_files, wireframe);
}

void OpenglRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {
	state.set_default();
	{ //
		glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
		glLineWidth(line_width);
	}

	common_uniforms.set({ I, game, I.window_size });

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

} // namespace gl
