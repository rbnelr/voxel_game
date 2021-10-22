#include "common.hpp"
#include "bloom.hpp"
#include "opengl_renderer.hpp"

namespace gl {

	void BloomRenderer::apply_bloom (OpenglRenderer& r, FramebufferTexture& main_fbo) {
		resize_fbos(r.framebuffer.size);

		GLuint input_tex = main_fbo.color;

		PipelineState s;
		s.blend_enable = false;
		s.depth_test = false;
		s.depth_write = false;
		r.state.set_no_override(s);

		glViewport(0,0, size.x, size.y);
		glScissor (0,0, size.x, size.y);

		for (auto& p : passes) {
			if (!p.shad->prog) continue;

			glUseProgram(p.shad->prog);

			r.state.bind_textures(p.shad, {
				{"input_tex", {GL_TEXTURE_2D, input_tex}, r.bloom_sampler},
				{"gaussian_kernel", gaussian.kernel_tex},
			});

			p.shad->set_uniform("exposure", exposure);

			p.shad->set_uniform("stepsz", 1.0f / (float2)size);
			p.shad->set_uniform("radius", radius);
			p.shad->set_uniform("cutoff", cutoff);
			p.shad->set_uniform("strength", strength);

			glBindFramebuffer(GL_FRAMEBUFFER, p.fbo);
			
			glDrawArrays(GL_TRIANGLES, 0, 3); // full screen triangle

			input_tex = p.color;
		}
	}

}
