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

	int max_iterations = 200;
	bool visualize_iterations = false;

	float clip_dist = 10000;
	float max_step = 60;
	float min_step = 0.25f;
	float sdf_fac = 0.25f;
	float smin_k = 10;

	struct NoiseSetting {
		float period = 10.0f; // inverse of frequency
		float amplitude = 100.0f; // proportional to period
		float param0 = 0.0f;
		float param1 = 0.0f;
	};

	NoiseSetting noises[8] = {
		{ 730, 20 },
		{ 2000, 100, 34 },
		{ 100, 68 },
		{ 500, 20, 10 },
	};

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
		ImGui::DragFloat("min_step", &min_step, 0.1f, 1 / 1024, 1, "%.5f", 2);
		ImGui::DragFloat("sdf_fac", &sdf_fac, 0.005f, 0.1f, 2, "%.5f", 2);
		ImGui::DragFloat("smin_k", &smin_k, 0.1f);

		ImGui::Separator();
		ImGui::TreeNode("Noise Layers");
		ImGui::Columns(5, "Noise Layers", false);
		ImGui::Text("Noise");		ImGui::NextColumn();
		ImGui::Text("period");		ImGui::NextColumn();
		ImGui::Text("amplitude");	ImGui::NextColumn();
		ImGui::Text("param0");		ImGui::NextColumn();
		ImGui::Text("param1");		ImGui::NextColumn();
		ImGui::Separator();
		ImGui::SetColumnWidth(0, 50);

		for (int i=0; i<ARRLEN(noises); ++i) {
			auto& n = noises[i];
			ImGui::TreePush(&n);
			ImGui::SetNextItemWidth(20);
			ImGui::Text("[%d]", i); ImGui::NextColumn();
			ImGui::SetNextItemWidth(100);
			ImGui::DragFloat("##period", &n.period, 0.1f, 0.0001f, 10000, "%.2f", 2);	ImGui::NextColumn();
			ImGui::SetNextItemWidth(100);
			ImGui::DragFloat("##amplitude", &n.amplitude, 0.1f);						ImGui::NextColumn();
			ImGui::SetNextItemWidth(100);
			ImGui::DragFloat("##param0", &n.param0, 0.1f);								ImGui::NextColumn();
			ImGui::SetNextItemWidth(100);
			ImGui::DragFloat("##param1", &n.param1, 0.1f);								ImGui::NextColumn();
			ImGui::TreePop();
		}
		ImGui::Columns(1);
		ImGui::TreePop();

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

			for (int i=0; i<ARRLEN(noises); ++i) {
				glUniform1f(glGetUniformLocation(shader.shader->shad, prints("nfreq[%d]", i).c_str()), 1.0f / noises[i].period);
				glUniform1f(glGetUniformLocation(shader.shader->shad, prints("namp[%d]", i).c_str()), noises[i].period * noises[i].amplitude / 100.0f);
				glUniform1f(glGetUniformLocation(shader.shader->shad, prints("param0[%d]", i).c_str()), noises[i].param0);
				glUniform1f(glGetUniformLocation(shader.shader->shad, prints("param1[%d]", i).c_str()), noises[i].param1);
			}

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
