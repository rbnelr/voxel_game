#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"

namespace gl {

class OpenglRenderer;

struct BloomRenderer {
	SERIALIZE(BloomRenderer, enable, radius, cutoff, strength)

	bool enable = true;

	struct GaussianKernel {
		std::vector<float> weights;

		Texture1D kernel_tex = {"BloomRenderer.gaussian_kernel"};

		// gauss: e^(-1/2 * (x/stddev)^2) / (stddev * sqrt(2*PI))

		// integrate gauss over [x-0.5, x+0.5]
		float integrate_gauss (float x, float stddev) {
			float total = 0.0;
			float t = x - 0.5f;
			float step = 1.0f / 256.0f;
			for (int i=0; i<256; ++i) {
				total += expf(t*t * (1.0f / (-2.0f * stddev*stddev)));
				t += step;
			}

			return total / (stddev * sqrt(2.0f * PI) * 256.0f);
		}

		void calc (int radius) {
			weights.resize(radius + 1);

			float stddev = (float)radius * 0.3f; // magic number to make stddev so that curve reaches close to 0 at width
			
			for (int i=0; i <= radius; ++i) {
				weights[i] = integrate_gauss((float)i, stddev);
			}

			float total = weights[0];
			for (int i=1; i <= radius; ++i) {
				total += weights[i] * 2.0f;
			}

			// normalize to exactly 1 sum
			for (int i=0; i <= radius; ++i) {
				weights[i] /= total;
			}

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_1D, kernel_tex);
			glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, (GLsizei)weights.size(), 0, GL_RED, GL_FLOAT, weights.data());
			glTextureParameteri(kernel_tex, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(kernel_tex, GL_TEXTURE_MAX_LEVEL, 0);
			glBindTexture(GL_TEXTURE_1D, 0);
		}
	};
	GaussianKernel gaussian;

	int radius = 8;
	float cutoff = 1.2f;
	float strength = 0.2f;

	float exposure = 1.0f;

	struct Pass {
		Shader*		shad;
		GLuint		color	= 0;
		GLuint		fbo		= 0;

		void create_fbo (std::string_view label, GLenum format, int2 const& size) {
			glActiveTexture(GL_TEXTURE0); // try clobber consistent texture at least

			glGenTextures(1, &color);
			glBindTexture(GL_TEXTURE_2D, color);
			glTexStorage2D(GL_TEXTURE_2D, 1, format, size.x, size.y);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			glBindTexture(GL_TEXTURE_2D, 0);

			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);

			OGL_DBG_LABEL(GL_FRAMEBUFFER, fbo, label);
			OGL_DBG_LABEL(GL_TEXTURE, color, label);

			//GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			//if (status != GL_FRAMEBUFFER_COMPLETE) {
			//	fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
			//}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		void destroy_fbo () {
			glDeleteTextures(1, &color);
			glDeleteFramebuffers(1, &fbo);
		}
	};
	Pass passes[2];

	int2 size = 0;
	float renderscale = 1.0f;


	void resize_fbos (int2 base_size) {
		auto old_size = size;
		size = max(1, roundi((float2)base_size * renderscale));

		if (old_size != size) {
			// delete old
			for (auto& p : passes)
				p.destroy_fbo();

			for (auto& p : passes)
				p.create_fbo("BloomRenderer.fbo", GL_RGBA16F, size);
		}
	}

	void imgui () {
		if (!ImGui::TreeNodeEx("Bloom")) return;

		ImGui::Checkbox("enable", &enable);

		ImGui::DragFloat("exposure", &exposure, 0.02f, 0, 16);

		if (ImGui::SliderInt("radius", &radius, 1, 128, "%d", ImGuiSliderFlags_Logarithmic))
			gaussian.calc(radius);

		ImGui::DragFloat("cutoff", &cutoff, 0.05f, 0, 3);
		ImGui::DragFloat("strength", &strength, 0.05f, 0, 2);

		ImGui::Text("res: %4d x %4d px (%5.2f Mpx)", size.x, size.y, (float)(size.x * size.y) / (1000*1000));
		ImGui::SliderFloat("resolution", &renderscale, 0.02f, 2.0f);

		//
		ImGui::TreePop();
	}

	BloomRenderer (Shaders& shaders) {
		int i = 0;
		for (auto& p : passes)
			p.shad = shaders.compile("bloom", { {"PASS", prints("%d", i++)} });

		gaussian.calc(radius);
	}

	void apply_bloom (OpenglRenderer& r, Framebuffer& main_fbo);
};

}
