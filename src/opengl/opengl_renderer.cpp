#include "common.hpp"
#include "opengl_renderer.hpp"

#include "GLFW/glfw3.h" // include after glad

namespace gl {


void OpenglRenderer::frame_begin (GLFWwindow* window, kiss::ChangedFiles& changed_files) {
	ctx.imgui_begin();

	shaders.update_recompilation(changed_files, wireframe);
}

void OpenglRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {

	{ // GL state defaults
		glEnable(GL_FRAMEBUFFER_SRGB);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		// scissor
		glDisable(GL_SCISSOR_TEST);
		// depth
		gl_enable(GL_DEPTH_TEST, true);

		// use_reverse_depth
		glClearDepth(0.0f);
		glDepthFunc(GL_GEQUAL);

		glDepthRange(0.0f, 1.0f);
		glDepthMask(GL_TRUE);
		// culling
		gl_enable(GL_CULL_FACE, !wireframe || !wireframe_backfaces);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		// blending
		gl_enable(GL_BLEND, false);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//
		glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
		glLineWidth(line_width);
	}

	common_uniforms.set({ I, game, I.window_size });

	glViewport(0,0, I.window_size.x, I.window_size.y);
	glScissor(0,0, I.window_size.x, I.window_size.y);

	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		OGL_TRACE("test");

		glUseProgram(test->prog);

		glBindVertexArray(dummy_vao);

		glDrawArrays(GL_TRIANGLES, 0, 6 * 1024);

		glBindVertexArray(0);
	}

	ctx.imgui_draw();

	TracyGpuCollect;

	glfwSwapBuffers(window);
}

} // namespace gl
