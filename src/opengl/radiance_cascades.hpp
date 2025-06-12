#pragma once
#include "common.hpp"
#include "engine/renderer.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"

struct Game;
namespace gl {
class OpenglRenderer;

struct ComputeTexture {
	Texture2D tex;
	int2 size;

	ComputeTexture () {}
	ComputeTexture (std::string_view label, int2 size): tex{label} {
		this->size = size;

		glTextureStorage2D(tex, 1, GL_RGBA16F, size.x,size.y);
		glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
		glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		lrgba col = srgba(0,0,0,0);
		glClearTexImage(tex, 0, GL_RGBA, GL_FLOAT, &col.x);
	}
};
struct ComputeTextureArray {
	//Texture2DArray tex;
	Texture3D tex; // use 3d texture because arary count is really limited
	int3 size;

	ComputeTextureArray () {}
	ComputeTextureArray (std::string_view label, int2 size, int array_count): tex{label} {
		this->size = int3(size, array_count);

		glTextureStorage3D(tex, 1, GL_RGBA16F, size.x,size.y, array_count);
		glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
		glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		lrgba col = srgba(0,0,0,0);
		glClearTexImage(tex, 0, GL_RGBA, GL_FLOAT, &col.x);
	}
};

struct TexturedQuadDrawer {
	gl::Shader* shad;
	gl::Shader* shad_2dArray;
	gl::Shader* shad_3d;
	IndexedMesh quad;

	Sampler sampl = {"sampl"};
	Sampler sampl_nearest = {"sampl_nearest"};

	TexturedQuadDrawer (OpenglRenderer& r);

	void draw (StateManager& state, float3 pos, float3 size, Texture2D& tex, bool nearest=false) {
		glUseProgram(shad->prog);
		PipelineState s;
		state.set(s);
		state.bind_textures(shad, {
			{"tex", tex, nearest ? sampl_nearest : sampl},
		});

		auto obj2world = translate(pos) * rotate3_X(deg(90)) * scale(size);
		shad->set_uniform("obj2world", (float4x4)obj2world);

		glBindVertexArray(quad.ib.vao);
		glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}
	void draw (StateManager& state, float3 pos, float3 size, Texture2DArray& tex, int arr_idx=-1, int grid_width=-1, bool nearest=false) {
		glUseProgram(shad_2dArray->prog);
		PipelineState s;
		state.set(s);
		state.bind_textures(shad_2dArray, {
			{"tex", tex, nearest ? sampl_nearest : sampl},
		});

		auto obj2world = translate(pos) * rotate3_X(deg(90)) * scale(size);
		shad_2dArray->set_uniform("obj2world", (float4x4)obj2world);
		shad_2dArray->set_uniform("arr_idx", arr_idx);
		shad_2dArray->set_uniform("grid_width", grid_width);

		glBindVertexArray(quad.ib.vao);
		glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}
	void draw (StateManager& state, float3 pos, float3 size, Texture3D& tex, int z_idx=-1, int grid_width=-1, bool nearest=false) {
		glUseProgram(shad_3d->prog);
		PipelineState s;
		state.set(s);
		state.bind_textures(shad_3d, {
			{"tex", tex, nearest ? sampl_nearest : sampl},
		});

		auto obj2world = translate(pos) * rotate3_X(deg(90)) * scale(size);
		shad_3d->set_uniform("obj2world", (float4x4)obj2world);
		shad_3d->set_uniform("arr_idx", z_idx);
		shad_3d->set_uniform("grid_width", grid_width);

		glBindVertexArray(quad.ib.vao);
		glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}
};

class RadianceCascadesTesting {
public:
	SERIALIZE(RadianceCascadesTesting, base_pos, size)

	bool imopen = true;

	int3 base_pos = 0;
	int2 size = 100;

	int cascades = 7;
	float base_spacing = 0.125f;
	int base_rays = 4; // = branching_factor  ->  sqrt(base_rays) = scale_factor
	float base_interval_mul = 1;

	int show_cascade = -1;
	int show_ray = -1;
	float _show_ray_ang = deg(90);

	int get_num_rays (int casc) { return (int)powf((float)base_rays, (float)casc+1); }
	float get_spacing (int casc) { return base_spacing * (int)powf(sqrtf((float)base_rays), (float)casc); }
	float2 get_interval (int casc) {
		float start = casc == 0 ? 0.0f :
		            base_interval_mul * base_spacing * powf((float)base_rays, (float)casc-1);
		float end = base_interval_mul * base_spacing * powf((float)base_rays, (float)casc);
		return float2(start, end);
	}

	int2 get_num_probes (float spacing) {
		return ceili((float2)size / spacing);
	}

	bool recreate = true;
	
	gl::Shader* trace_shad;
	gl::Shader* combine_shad;

	std::unique_ptr<ComputeTextureArray[]> cascade_texs;
	ComputeTexture result_tex;

	TexturedQuadDrawer tex_draw;

	RadianceCascadesTesting (OpenglRenderer& r);

	void imgui () {
		if (!imopen) return;
		if (ImGui::Begin("RadianceCascadesTesting", &imopen)) {

			ImGui::DragInt3("base_pos", &base_pos.x, 0.1f);
			recreate |= ImGui::DragInt2("size", &size.x, 0.1f);

			recreate |= ImGui::SliderInt("cascades", &cascades, 1, 7); // 8 cascades results in 65k rays which breaks 3d texture z index
			recreate |= ImGui::DragFloat("base_spacing", &base_spacing, 0.01f, 0.05f, 8);
			recreate |= ImGui::DragInt("base_rays", &base_rays, 0.1f, 1, 64);
			ImGui::DragFloat("base_interval", &base_interval_mul, 0.1f, 1, 16);

			ImGui::SliderInt("show_cascade", &show_cascade, -1, cascades-1);
			ImGui::SliderAngle("show_ray (by angle)", &_show_ray_ang, -10, 360);

			int num_rays = get_num_rays(max(show_cascade, 0));
			//_show_ray_ang = (float(show_ray) + 0.5) / (float)num_rays * deg(360);
			show_ray = roundi(_show_ray_ang * (float)num_rays / deg(360) - 0.5f);

			if (show_cascade < 0)
				ImGui::Text("Showing Result (Average light from all directions at %.3f m res)", base_spacing);
			else {
				auto spacing = get_spacing(show_cascade);
				auto probes = get_num_probes(spacing);
				auto rays = get_num_rays(show_cascade);
				ImGui::Text("Showing Cascade #%d", show_cascade);
				ImGui::Text("%dx%d Probes at %.3f spacing", probes.x, probes.y, spacing);
				ImGui::Text("%d Rays per Probe, Ray spacing: %.3f deg", rays, 360.0f / (float)rays);
				ImGui::Text("Ray interval %.1f to %.1f m", get_interval(show_cascade).x, get_interval(show_cascade).y);
				ImGui::Text("Rays stored and traced in Cascade: %d", probes.x * probes.y * rays);
			}
		}
		ImGui::End();
	}

	void do_recreate () {
		cascade_texs = std::make_unique<ComputeTextureArray[]>(cascades);
		for (int casc=0; casc<cascades; casc++) {

			int2 probes = get_num_probes(get_spacing(casc));
			int rays = get_num_rays(casc);

			cascade_texs[casc] = ComputeTextureArray("RCtex", max(probes, 1), clamp(rays, 1, 32*1024));
		}

		result_tex = ComputeTexture("RCtex", max(get_num_probes(get_spacing(0)), 1));
	}
	void update (Game& game, OpenglRenderer& r);
};
}
