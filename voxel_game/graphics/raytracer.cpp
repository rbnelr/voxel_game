#include "stdafx.hpp"
#include "raytracer.hpp"
#include "graphics.hpp"
#include "../svo.hpp"

void Raytracer::imgui () {
	if (!imgui_push("Raytracer")) {
		ImGui::SameLine();
		ImGui::Checkbox("draw", &raytracer_draw);
		return;
	}

	ImGui::Checkbox("draw", &raytracer_draw);

	ImGui::Checkbox("overlay", &overlay);

	ImGui::SliderFloat("slider", &slider, 0,1);

	ImGui::SliderInt("max_iterations", &max_iterations, 1,512);
	ImGui::Checkbox("visualize_iterations", &visualize_iterations);

	ImGui::SliderFloat("sun_radius", &sun_radius, 0, 0.5f);

	ImGui::SliderFloat("water_F0", &water_F0, 0, 1);
	ImGui::SliderFloat("water_IOR", &water_IOR, 0, 2);

	ImGui::DragFloat2("water_scroll_dir1", &water_scroll_dir1.x, 0.05f);
	ImGui::DragFloat2("water_scroll_dir2", &water_scroll_dir2.x, 0.05f);
	ImGui::DragFloat("water_scale1", &water_scale1, 0.05f);
	ImGui::DragFloat("water_scale2", &water_scale2, 0.05f);
	ImGui::DragFloat("water_strength1", &water_strength1, 0.05f);
	ImGui::DragFloat("water_strength2", &water_strength2, 0.05f);
	ImGui::DragFloat("water_lod_bias", &water_lod_bias, 0.05f);

	ImGui::DragFloat("time", &time);
	ImGui::DragFloat("time_speed", &time_speed, 0.05f);

	imgui_pop();
}

void Raytracer::draw (svo::SVO& svo, Camera_View const& view, Graphics& graphics, TimeOfDay& tod) {

	if (shader) {
		shader.bind();

		glBindVertexArray(vao);

		shader.set_uniform("svo_root_pos", svo.root->pos);
		shader.set_uniform("svo_root_scale", svo.root->scale);

		shader.set_uniform("slider", slider);

		shader.set_uniform("max_iterations", max_iterations);
		shader.set_uniform("visualize_iterations", visualize_iterations);

		shader.set_uniform("sun_col", tod.cols.sun_col);
		shader.set_uniform("sun_dir", tod.sun_dir);
		shader.set_uniform("sun_radius", sun_radius);

		shader.set_uniform("water_F0", water_F0);
		shader.set_uniform("water_IOR", water_IOR);

		shader.set_uniform("water_scroll_dir1", water_scroll_dir1);
		shader.set_uniform("water_scroll_dir2", water_scroll_dir2);
		shader.set_uniform("water_scale1", water_scale1);
		shader.set_uniform("water_scale2", water_scale2);
		shader.set_uniform("water_strength1", water_strength1);
		shader.set_uniform("water_strength2", water_strength2);
		shader.set_uniform("water_lod_bias", water_lod_bias);

		time += time_speed * input.dt;
		shader.set_uniform("time", time);

		if (svo.allocator.gpu_nodes.ssbo == 0) {
			clog(ERROR, "GL_ARB_sparse_buffer not supported!");
			raytracer_draw = false;
			return;
		}
		
		svo.allocator.gpu_nodes.upload_changes(svo);

		//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, svo.allocator.gpu_nodes.ssbo);
		//glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, svo.allocator.gpu_nodes.ssbo, 0, 2ull * 1024 * 1024 * 1024);

		glUniformui64NV(glGetUniformLocation(shader.shader->shad, "svo_nodes"), svo.allocator.gpu_nodes.ssbo_ptr);

		std::vector<float4> block_tile_info;
		for (auto& bti : graphics.tile_textures.block_tile_info) {
			block_tile_info.push_back((float4)int4( bti.base_index, bti.top, bti.bottom, bti.variants ));
		}
		block_tile_info_texture.upload(&block_tile_info[0], (int)block_tile_info.size(), false, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);

		GLint texunit = 0;

		glActiveTexture(GL_TEXTURE0 + texunit);
		shader.set_texture_unit("tile_textures", texunit);
		graphics.sampler.bind(texunit++);
		graphics.tile_textures.tile_textures.bind();

		glActiveTexture(GL_TEXTURE0 + texunit);
		shader.set_texture_unit("block_tile_info", texunit);
		svo_sampler.bind(texunit++);
		block_tile_info_texture.bind();

		//glActiveTexture(GL_TEXTURE0 + texunit);
		//shader.set_texture_unit("svo_texture", texunit);
		//svo_sampler.bind(texunit++);
		//svo_texture.bind();

		glActiveTexture(GL_TEXTURE0 + texunit);
		shader.set_texture_unit("water_normal", texunit);
		water_sampler.bind(texunit++);
		water_normal.bind();

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
