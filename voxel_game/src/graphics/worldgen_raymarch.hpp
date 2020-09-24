#pragma once
#include "shaders.hpp"
#include "graphics_common.hpp"
#include "worldgen.hpp"

class WorldgenRaymarch {
public:

	Shader shader = Shader("worldgen_raymarch");

	gl::Vao vao; // empty vao even though I generate a screen-filling quad in the vertex shader, no vao works but generates an error on my machine

	Sampler trilinear_sampler = Sampler(gl::Enum::LINEAR, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::REPEAT);
	Sampler nearest_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::LINEAR, gl::Enum::CLAMP_TO_EDGE);
	Texture2D heat_gradient = { "textures/heat_gradient.png" };

	Texture2D env = { "textures/env/blue_grotto_1k.hdr" };

	bool raytracer_draw = 1;
	bool overlay = 0;

	float slider = 1.00f;

	// max raymarch iterations, depending on scene and settings below this limit can be reached or not, when reached causes cutoff at distance
	int max_iterations = 200;
	bool visualize_iterations = false;

	// distance where to stop raymarch 'farplane'
	float far_clip = 50000;
	// multiplier to pseudo SDF step size, to allow tweaking of noise raymarch to look correct
	// (higher= higher perf but warping artefacts because of pseudo SDF,  lower= lower perf but more correct)
	float sdf_fac = 0.30f;
	// max step size, lower to fix large scale warping
	float max_step = 400;
	// min step size, higher for better perf, but causes silluette to warp
	float min_step = 2.0f;
	// termination distance for surface binary search, lower = slightly lower perf, but less depth error for surface (visible as banding)
	float surf_precision = 0.01f;

	void imgui () {
		if (!imgui_push("WorldgenRaymarch")) {
			ImGui::SameLine();
			ImGui::Checkbox("draw", &raytracer_draw);
			return;
		}

		ImGui::Checkbox("draw", &raytracer_draw);

		ImGui::Checkbox("overlay", &overlay);

		ImGui::SliderFloat("slider", &slider, 0,1);

		float max_iterationsf = (float)max_iterations;
		ImGui::SliderFloat("max_iterations", &max_iterationsf, 1,4096, "%.0f", ImGuiSliderFlags_Logarithmic);
		max_iterations = (int)max_iterationsf;

		ImGui::Checkbox("visualize_iterations", &visualize_iterations);

		ImGui::DragFloat("far_clip", &far_clip, 10);
		ImGui::DragFloat("sdf_fac", &sdf_fac, 0.005f, 0.1f, 2, "%.5f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("max_step", &max_step, 0.1f, 5, 1000, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("min_step", &min_step, 0.1f, 1 / 1024, 20, "%.5f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("surf_precision", &surf_precision, 0.1f, 1 / 1024, 20, "%.5f", ImGuiSliderFlags_Logarithmic);

		imgui_pop();
	}

	void draw (WorldGenerator& wg) {
		if (raytracer_draw && shader) {
			ZoneScoped;
			GPU_SCOPE("WorldgenRaymarch pass");

			shader.bind();

			glBindVertexArray(vao);

			shader.set_uniform("slider", slider);

			shader.set_uniform("max_iterations", max_iterations);
			shader.set_uniform("visualize_iterations", visualize_iterations);

			shader.set_uniform("far_clip", far_clip);
			shader.set_uniform("sdf_fac", sdf_fac);
			shader.set_uniform("max_step", max_step);
			shader.set_uniform("min_step", min_step);
			shader.set_uniform("surf_precision", surf_precision);

			wg.set_uniforms(shader);

			GLint texunit = 0;

			glActiveTexture(GL_TEXTURE0 + texunit);
			shader.set_texture_unit("heat_gradient", texunit);
			trilinear_sampler.bind(texunit++);
			heat_gradient.bind();

			glActiveTexture(GL_TEXTURE0 + texunit);
			shader.set_texture_unit("env", texunit);
			trilinear_sampler.bind(texunit++);
			env.bind();

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
	}
};
