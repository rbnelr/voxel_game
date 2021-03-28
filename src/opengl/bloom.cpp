#include "common.hpp"
#include "bloom.hpp"
#include "opengl_renderer.hpp"

namespace gl {

	void BloomRenderer::apply_bloom (OpenglRenderer& r, Framebuffer& main_fbo) {
		resize_fbos(r.framebuffer.size);

		GLuint input_tex = main_fbo.color;

		PipelineState s;
		s.blend_enable = false;
		s.depth_test = false;
		s.depth_write = false;
		r.state.set(s);

		glViewport(0,0, size.x, size.y);
		glScissor (0,0, size.x, size.y);

		for (auto& p : passes) {
			if (!p.shad->prog) continue;

			glUseProgram(p.shad->prog);

			p.shad->set_uniform("exposure", exposure);

			p.shad->set_uniform("stepsz", 1.0f / (float2)size);
			p.shad->set_uniform("radius", radius);
			p.shad->set_uniform("cutoff", cutoff);
			p.shad->set_uniform("strength", strength);

			glBindFramebuffer(GL_FRAMEBUFFER, p.fbo);

			glActiveTexture(GL_TEXTURE0);
			glUniform1i(p.shad->get_uniform_location("input_tex"), 0);
			glBindSampler(0, r.post_sampler);
			glBindTexture(GL_TEXTURE_2D, input_tex);

			glActiveTexture(GL_TEXTURE1);
			//glBindSampler(1, r.post_sampler);
			glBindTexture(GL_TEXTURE_1D, gaussian.kernel_tex);
			glUniform1i(p.shad->get_uniform_location("gaussian_kernel"), 1);
			
			glDrawArrays(GL_TRIANGLES, 0, 3); // full screen triangle

			input_tex = p.color;
		}
	}

}
