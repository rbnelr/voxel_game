#include "raytracer.hpp"
#include "graphics.hpp"
#include "../util/timer.hpp"
#include "../world_octree.hpp"

void Raytracer::imgui () {
	if (!imgui_push("Raytracer")) return;

	ImGui::Checkbox("draw", &raytracer_draw);
	ImGui::Checkbox("overlay", &overlay);

	ImGui::SliderFloat("slider", &slider, 0,1);

	ImGui::SliderInt("max_iterations", &max_iterations, 1,512);
	ImGui::Checkbox("visualize_iterations", &visualize_iterations);

	ImGui::SliderFloat("water_F0", &water_F0, 0, 1);
	ImGui::SliderFloat("water_IOR", &water_IOR, 0, 2);

	ImGui::ColorPicker3("water_fog", &water_fog.x);
	ImGui::SliderFloat("water_fog_dens", &water_fog_dens, 0, 1.0f);

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

void Raytracer::draw (world_octree::WorldOctree& octree, Camera_View const& view, Graphics& graphics) {

	if (shader) {
		shader.bind();

		glBindVertexArray(vao);

		shader.set_uniform("svo_root_pos", octree.root_pos);
		shader.set_uniform("svo_root_scale", octree.root_scale);

		shader.set_uniform("slider", slider);

		shader.set_uniform("max_iterations", max_iterations);
		shader.set_uniform("visualize_iterations", visualize_iterations);

		shader.set_uniform("water_F0", water_F0);
		shader.set_uniform("water_IOR", water_IOR);

		shader.set_uniform("water_fog", water_fog);
		shader.set_uniform("water_fog_dens", water_fog_dens);

		shader.set_uniform("water_scroll_dir1", water_scroll_dir1);
		shader.set_uniform("water_scroll_dir2", water_scroll_dir2);
		shader.set_uniform("water_scale1", water_scale1);
		shader.set_uniform("water_scale2", water_scale2);
		shader.set_uniform("water_strength1", water_strength1);
		shader.set_uniform("water_strength2", water_strength2);
		shader.set_uniform("water_lod_bias", water_lod_bias);

		time += time_speed * input.dt;
		shader.set_uniform("time", time);

		{
			auto* data = (uint32_t*)&octree.octree.nodes[0];
			int count = (int)(octree.octree.nodes.size() * 8);

			static constexpr int SVO_TEX_WIDTH = 2048;
			int2 size = int2(SVO_TEX_WIDTH, (count + (SVO_TEX_WIDTH-1)) / SVO_TEX_WIDTH);

			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

			glBindTexture(GL_TEXTURE_2D, svo_texture.tex);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, size.x,size.y, 0, GL_RED_INTEGER, GL_INT, nullptr);
			
			// Upload most of the svo data as rectangular region
			if (size.y > 1)
				glTexSubImage2D(GL_TEXTURE_2D, 0,  0, 0,		SVO_TEX_WIDTH, size.y-1, GL_RED_INTEGER, GL_INT, data);
			// copy last row of tex pixels of the svo data
			if (size.y > 0)
				glTexSubImage2D(GL_TEXTURE_2D, 0,  0, size.y-1,	count % SVO_TEX_WIDTH, 1, GL_RED_INTEGER, GL_INT, &data[SVO_TEX_WIDTH * (size.y-1)]);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		}

		std::vector<float4> block_tile_info;
		for (auto& bti : graphics.tile_textures.block_tile_info) {
			block_tile_info.push_back((float4)int4( bti.base_index, bti.top, bti.bottom, bti.variants ));
		}
		block_tile_info_texture.upload(&block_tile_info[0], (int)block_tile_info.size(), false, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);

		glActiveTexture(GL_TEXTURE0 + 0);
		shader.set_texture_unit("tile_textures", 0);
		graphics.sampler.bind(0);
		graphics.tile_textures.tile_textures.bind();

		glActiveTexture(GL_TEXTURE0 + 1);
		shader.set_texture_unit("block_tile_info", 1);
		svo_sampler.bind(1);
		block_tile_info_texture.bind();

		glActiveTexture(GL_TEXTURE0 + 2);
		shader.set_texture_unit("svo_texture", 2);
		svo_sampler.bind(2);
		svo_texture.bind();

		glActiveTexture(GL_TEXTURE0 + 3);
		shader.set_texture_unit("water_normal", 3);
		water_sampler.bind(3);
		water_normal.bind();

		glActiveTexture(GL_TEXTURE0 + 4);
		shader.set_texture_unit("heat_gradient", 4);
		gradient_sampler.bind(4);
		heat_gradient.bind();

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}
