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
	ComputeTexture (std::string_view label, int2 const& size): tex{label} {
		this->size = size;

		glTextureStorage2D(tex, 1, GL_RGBA16F, size.x,size.y);
		glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
		glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);

		lrgba col = srgba(0,0,0,0);
		glClearTexImage(tex, 0, GL_RGBA, GL_FLOAT, &col.x);
	}
};
struct TexturedQuadDrawer {
	gl::Shader* shad;
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
};

class RadianceCascadesTesting {
public:
	SERIALIZE(RadianceCascadesTesting, base_pos, size)

	bool imopen = true;

	int3 base_pos = 0;
	int2 size = 100;

	int cascades = 2;
	float base_spacing = 0.25f;
	int base_rays = 4;
	float base_interval = 3;

	int show_cascade = -1;

	int get_num_rays (int casc) { return (int)powf((float)base_rays, (float)casc+1); }
	float get_spacing (int casc) { return base_spacing * (int)powf(sqrtf((float)base_rays), (float)casc); }
	float2 get_interval (int casc) {
		float start = casc == 0 ? 0.0f :
		            base_interval * powf((float)base_rays, (float)casc-1);
		float end = base_interval * powf((float)base_rays, (float)casc);
		return float2(start, end);
	}

	int2 get_num_probes (float spacing) {
		return ceili((float2)size / spacing);
	}
	int2 get_ray_regions (int casc) {
		int num = get_num_rays(casc);
		int w = (int)sqrtf((float)num);
		int h = (num + w-1) / w;
		return int2(w,h);
	}

	bool recreate = true;
	
	gl::Shader* trace_shad;
	gl::Shader* combine_shad;

	static constexpr int2 GROUP_SZ = int2(8,8);

	std::unique_ptr<ComputeTexture[]> cascade_texs;
	ComputeTexture result_tex;

	TexturedQuadDrawer tex_draw;

	RadianceCascadesTesting (OpenglRenderer& r);

	void imgui () {
		if (ImGui::Begin("RadianceCascadesTesting", &imopen)) {

			ImGui::DragInt3("base_pos", &base_pos.x, 0.1f);
			recreate |= ImGui::DragInt2("size", &size.x, 0.1f);

			recreate |= ImGui::DragInt("cascades", &cascades, 0.1f, 0, 16);
			recreate |= ImGui::DragFloat("base_spacing", &base_spacing, 0.1f, 0.25f, 8);
			ImGui::DragInt("base_rays", &base_rays, 0.1f, 1, 64);
			ImGui::DragFloat("base_interval", &base_interval, 0.1f, 1, 16);

			ImGui::SliderInt("show_cascade", &show_cascade, -1, cascades-1);
		}
		ImGui::End();
	}

	void do_recreate () {
		cascade_texs = std::make_unique<ComputeTexture[]>(cascades);
		for (int casc=0; casc<cascades; casc++) {

			int2 probes = get_num_probes(get_spacing(casc));
			int2 rays = get_ray_regions(casc);
			int2 tex_res = probes * rays;

			cascade_texs[casc] = ComputeTexture("RCtex", tex_res);
		}

		result_tex = ComputeTexture("RCtex", get_num_probes(get_spacing(0)));
	}
	void update (Game& game, OpenglRenderer& r);
};
}
