#pragma once
#include "shaders.hpp"
#include "graphics_common.hpp"
#include "dear_imgui.hpp"

class WorldgenRaymarch {
public:

	Shader shader = Shader("worldgen_raymarch");

	gl::Vao vao; // empty vao even though I generate a screen-filling quad in the vertex shader, no vao works but generates an error on my machine

	Sampler trilinear_sampler = Sampler(gl::Enum::LINEAR, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::CLAMP_TO_EDGE);
	Sampler nearest_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::LINEAR, gl::Enum::CLAMP_TO_EDGE);
	Texture2D heat_gradient = { "textures/heat_gradient.png" };
	Texture2D dbg_font = { "textures/consolas_ascii9x17.png" };

	bool raytracer_draw = 1;
	bool overlay = 0;

	float slider = 1.00f;

	int max_iterations = 512;
	bool visualize_iterations = false;

	float clip_dist = 10000;
	float max_step = 10000;
	float min_step = 0.025f;
	float sdf_fac = 1;
	float smin_k = 10;

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
		ImGui::SliderFloat("max_iterations", &max_iterationsf, 1,4096, "%.0f", 2);
		max_iterations = (int)max_iterationsf;

		ImGui::Checkbox("visualize_iterations", &visualize_iterations);

		ImGui::DragFloat("clip_dist", &clip_dist, 10);
		ImGui::DragFloat("max_step", &max_step, 0.1f, 5, 1000, "%.2f", 2);
		ImGui::DragFloat("min_step", &min_step, 0.1f, 1 / 1024, 1, "%.2f", 2);
		ImGui::DragFloat("sdf_fac", &sdf_fac, 0.005f, 0.1f, 2, "%.3f", 2);
		ImGui::DragFloat("smin_k", &smin_k, 0.1f);

		imgui_pop();
	}

	void draw () {
		if (raytracer_draw && shader) {
			ZoneScoped;
			GPU_SCOPE("WorldgenRaymarch pass");

			shader.bind();

			glBindVertexArray(vao);

			shader.set_uniform("slider", slider);

			shader.set_uniform("max_iterations", max_iterations);
			shader.set_uniform("visualize_iterations", visualize_iterations);

			shader.set_uniform("clip_dist", clip_dist);
			shader.set_uniform("max_step", max_step);
			shader.set_uniform("min_step", min_step);
			shader.set_uniform("sdf_fac", sdf_fac);
			shader.set_uniform("smin_k", smin_k);

			GLint texunit = 0;

			glActiveTexture(GL_TEXTURE0 + texunit);
			shader.set_texture_unit("heat_gradient", texunit);
			trilinear_sampler.bind(texunit++);
			heat_gradient.bind();

			glActiveTexture(GL_TEXTURE0 + texunit);
			shader.set_texture_unit("dbg_font", texunit);
			nearest_sampler.bind(texunit++);
			dbg_font.bind();

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
	}
};
