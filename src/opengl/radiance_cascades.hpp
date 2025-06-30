#pragma once
#include "common.hpp"
#include "opengl_renderer.hpp"

class Game;
namespace gl {
struct RCComputeTexture { // Normal 2d texture for results
	Texture2D tex;
	int2 size;

	RCComputeTexture () {}
	RCComputeTexture (std::string_view label, int2 size): tex{label} {
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
struct RCProbeTexture2D {
	//Texture2DArray tex;
	Texture3D tex; // use 3d texture because arary count is really limited
	int3 size;

	RCProbeTexture2D () {}
	RCProbeTexture2D (std::string_view label, int2 size, int array_count): tex{label} {
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
struct RCProbeTexture3D {
	Texture3D tex;
	int3 num_probes;
	int2 rays_octh_size;
	int3 total_size () { return int3(rays_octh_size, 1) * num_probes; }
	size_t mem_size () {
		auto num = total_size();
		size_t sz = num.x * num.y * num.z;
		return sz * (2*4); // sizeof(RGBA16F)
	}

	RCProbeTexture3D () {}
	RCProbeTexture3D (std::string_view label, int3 num_probes, int2 rays_octh_size): tex{label} {
		this->num_probes = num_probes;
		this->rays_octh_size = rays_octh_size;
		int3 sz = total_size();

		glTextureStorage3D(tex, 1, GL_RGBA16F, sz.x,sz.y,sz.z);
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

struct QuadDrawer {
	gl::Shader* shad;
	gl::Shader* shad_2dArray;
	gl::Shader* shad_3d;
	IndexedMesh quad;

	Sampler sampl = {"sampl"};
	Sampler sampl_nearest = {"sampl_nearest"};

	QuadDrawer (OpenglRenderer& r) {
		shad = r.shaders.compile("debug_texture");
		shad_2dArray = r.shaders.compile("debug_texture", {{"TEX_ARRAY","1"}});
		shad_3d = r.shaders.compile("debug_texture", {{"TEX3D","1"}});

		GenericVertex verts[] = {
			{ float3(0,0,0), float3(0,0,1), float2(0,0), lrgba(1) },
			{ float3(1,0,0), float3(0,0,1), float2(1,0), lrgba(1) },
			{ float3(0,1,0), float3(0,0,1), float2(0,1), lrgba(1) },
			{ float3(1,1,0), float3(0,0,1), float2(1,1), lrgba(1) },
		};
		quad = upload_mesh("quad_mesh", verts, ARRLEN(verts), QUAD_INDICES, ARRLEN(QUAD_INDICES));
	
		glSamplerParameteri(sampl, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(sampl, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(sampl, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(sampl, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glSamplerParameteri(sampl_nearest, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(sampl_nearest, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(sampl_nearest, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(sampl_nearest, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	void draw (Texture2D& tex, StateManager& state, float3x4 obj2world, bool nearest=false) {
		glUseProgram(shad->prog);
		PipelineState s;
		s.culling = false;
		state.set(s);
		state.bind_textures(shad, {
			{"tex", tex, nearest ? sampl_nearest : sampl},
		});
		
		shad->set_uniform("obj2world", (float4x4)obj2world);

		glBindVertexArray(quad.ib.vao);
		glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}
	void draw (Texture2DArray& tex, StateManager& state, float3x4 obj2world, int arr_idx=-1, int grid_width=-1, bool nearest=false) {
		glUseProgram(shad_2dArray->prog);
		PipelineState s;
		s.culling = false;
		state.set(s);
		state.bind_textures(shad_2dArray, {
			{"tex", tex, nearest ? sampl_nearest : sampl},
		});
		
		shad_2dArray->set_uniform("obj2world", (float4x4)obj2world);
		shad_2dArray->set_uniform("arr_idx", arr_idx);
		shad_2dArray->set_uniform("grid_width", grid_width);

		glBindVertexArray(quad.ib.vao);
		glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}
	void draw (Texture3D& tex, StateManager& state, float3x4 obj2world, int z_idx=-1, int grid_width=-1, bool nearest=false) {
		glUseProgram(shad_3d->prog);
		PipelineState s;
		s.culling = false;
		state.set(s);
		state.bind_textures(shad_3d, {
			{"tex", tex, nearest ? sampl_nearest : sampl},
		});
		
		shad_3d->set_uniform("obj2world", (float4x4)obj2world);
		shad_3d->set_uniform("z_idx", z_idx);
		shad_3d->set_uniform("grid_width", grid_width);

		glBindVertexArray(quad.ib.vao);
		glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}

	// do glUseProgram beforehand
	void draw_using (Shader* shad, StateManager& state, float3x4 obj2world) {
		PipelineState s;
		s.culling = false;
		state.set(s);
		
		shad->set_uniform("obj2world", (float4x4)obj2world);

		glBindVertexArray(quad.ib.vao);
		glDrawElements(GL_TRIANGLES, quad.index_count, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}
};

class RadianceCascades2D {
public:
	SERIALIZE(RadianceCascades2D, imopen, base_pos, size, cascades, base_spacing, base_rays, base_interval_mul,
		_dbg_pos)

	bool imopen = true;

	int3 base_pos = 0;
	int2 size = 100;

	int cascades = 7;
	float base_spacing = 0.125f;
	int base_rays = 4; // = branching_factor  ->  sqrt(base_rays) = scale_factor
	float base_interval_mul = sqrtf(2.0f);

	int show_cascade = -1;
	int show_ray = -1;
	float _show_ray_ang = deg(90);
	float2 _dbg_pos = 0;

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

	std::unique_ptr<RCProbeTexture2D[]> cascade_texs;
	RCComputeTexture result_tex;

	QuadDrawer vis_draw;

	RadianceCascades2D (OpenglRenderer& r): vis_draw{r} {
		trace_shad = r.shaders.compile("rc_trace", {}, {{ COMPUTE_SHADER }});
		combine_shad = r.shaders.compile("rc_combine", {}, {{ COMPUTE_SHADER }});
	}

	void imgui () {
		if (!imopen) return;
		if (ImGui::Begin("RadianceCascades2D", &imopen)) {

			ImGui::DragInt3("base_pos", &base_pos.x, 0.1f);
			recreate |= ImGui::DragInt2("size", &size.x, 0.1f);

			recreate |= ImGui::SliderInt("cascades", &cascades, 1, 7); // 8 cascades results in 65k rays which breaks 3d texture z index
			recreate |= ImGui::DragFloat("base_spacing", &base_spacing, 0.01f, 0.05f, 8);
			recreate |= ImGui::DragInt("base_rays", &base_rays, 0.1f, 1, 64);
			ImGui::DragFloat("base_interval", &base_interval_mul, 0.1f, 1, 16);

			ImGui::SliderInt("show_cascade", &show_cascade, -1, cascades-1);
			ImGui::SliderAngle("show_ray (by angle)", &_show_ray_ang, -10, 360);

			ImGui::DragFloat2("dbg_pos", &_dbg_pos.x, 0.1f);

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
		cascade_texs = std::make_unique<RCProbeTexture2D[]>(cascades);
		for (int casc=0; casc<cascades; casc++) {

			int2 probes = get_num_probes(get_spacing(casc));
			int rays = get_num_rays(casc);

			cascade_texs[casc] = RCProbeTexture2D("RCtex", max(probes, 1), clamp(rays, 1, 32*1024));
		}

		result_tex = RCComputeTexture("RCtex", max(get_num_probes(get_spacing(0)), 1));
	}
	void update (OpenglRenderer& r) {
		if (!imopen) return;
		if (recreate) do_recreate();
		recreate = false;

		OGL_TRACE("radiance cascades");
	
		lrgba dbg_cols[] = {
			lrgba(0,0,1,1),
			lrgba(0,1,0,1),
			lrgba(1,0,0,1),
			lrgba(1,1,0,1),
			lrgba(1,0,1,1),
			lrgba(0,1,1,1),
		};

		if (trace_shad->prog) {
			glUseProgram(trace_shad->prog);

			for (int casc=cascades-1; casc>=0; casc--) {
				OGL_TRACE("radiance cascade");

				bool has_higher_cascade = casc < cascades-1;

				r.state.bind_textures(trace_shad, {
					{"tile_textures", r.tile_textures, r.pixelated_sampler},
					{"voxel_tex", r.raytracer->voxel_tex.tex},
					{"df_tex", r.raytracer->df_tex.tex},

					(has_higher_cascade ?
						StateManager::TextureBind{"higher_cascade", cascade_texs[casc+1].tex} :
						StateManager::TextureBind{"higher_cascade", Texture2D{}}),
				});
			
				trace_shad->set_uniform("voxtex_world_min", (float3)(r.raytracer->voxtex_offset * CHUNK_SIZE));

				int num_rays = get_num_rays(casc);
				float spacing = get_spacing(casc);
				float2 interval = get_interval(casc);
				float2 hi_interval = has_higher_cascade ? get_interval(casc+1) : 0;
		
				auto dbg_col = dbg_cols[casc % ARRLEN(dbg_cols)];
				int dbg_ray = roundi(_show_ray_ang * (float)num_rays / deg(360) - 0.5f);
			
				trace_shad->set_uniform("cascade", casc);
				trace_shad->set_uniform("has_higher_cascade", has_higher_cascade);
				trace_shad->set_uniform("world_base_pos", base_pos);
				trace_shad->set_uniform("world_size", size);
				trace_shad->set_uniform("num_rays", num_rays);
				trace_shad->set_uniform("spacing", spacing);
				trace_shad->set_uniform("interval", interval);
				trace_shad->set_uniform("hi_interval", hi_interval);
				trace_shad->set_uniform("scale_factor", sqrtf((float)base_rays));
				trace_shad->set_uniform("branching_factor", (float)base_rays);
				trace_shad->set_uniform("dbg_col", dbg_col);
				trace_shad->set_uniform("dbg_pos", _dbg_pos);
				trace_shad->set_uniform("dbg_ray", dbg_ray);
				trace_shad->set_uniform("update_debugdraw", r.debug_draw.update_indirect);

				auto& tex = cascade_texs[casc];
				glBindImageTexture(4, tex.tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
			
				trace_shad->set_uniform("dispatch_size", tex.size);
			
				constexpr int3 GROUP_SZ = int3(4,4,4);
				int3 dispatch_size = (tex.size + GROUP_SZ-1) / GROUP_SZ;
				glDispatchCompute(dispatch_size.x, dispatch_size.y, dispatch_size.z);
			
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);

				//float3x4 mat = translate((float3)base_pos) * rotate3_X(deg(90));
				//for (int y=0; y<4; ++y)
				//for (int x=0; x<4; ++x) {
				//	int2 probe_coord = int2(x,y);
				//	float2 probe_pos = spacing * ((float2)probe_coord + 0.5f);
				//	float angle_step = 2.0f*PI / float(num_rays);
				//
				//	for (int ray=0; ray<num_rays; ray++) {
				//		float ang = (float(ray) + 0.5f) * angle_step;
				//		float2 dir = float2(cosf(ang), sinf(ang));
				//
				//		g_debugdraw.line(mat * float3(probe_pos + dir*interval.x, 0),
				//						 mat * float3(probe_pos + dir*interval.y, 0), dbg_col);
				//	}
				//}
			}

			//g_debugdraw.point((float3)base_pos + float3(_dbg_pos.x, 0.9f, _dbg_pos.y), 0.1f, lrgba(1,1,1,0.5f));
		}
	
		if (combine_shad->prog) {
			glUseProgram(combine_shad->prog);

			r.state.bind_textures(trace_shad, {
				{"cascade0", cascade_texs[0].tex},
			});
		
			float spacing = get_spacing(0);
			int2 num_probes = get_num_probes(spacing);

			combine_shad->set_uniform("dispatch_size", result_tex.size);
		
			glBindImageTexture(4, result_tex.tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

			constexpr int2 GROUP_SZ = int2(8,8);
			int2 dispatch_size = (result_tex.size + (int2)GROUP_SZ-1) / (int2)GROUP_SZ;
			glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
		}
	
		auto show = [&] () {
			float tex_spacing = get_spacing(max(show_cascade, 0));
			float2 tex_size = (float2)get_num_probes(tex_spacing) * tex_spacing;
			float3 p = (float3)base_pos + float3(0,1,0);
			float3 sz = float3((float2)tex_size, 1);

			if (show_cascade >= 0) vis_draw.draw(cascade_texs[show_cascade].tex, r.state, transform(p, float3(deg(90),0,0), sz), show_ray,-1, true);
			else                   vis_draw.draw(result_tex.tex, r.state, transform(p, float3(deg(90),0,0), sz), true);
	
			float3 fsize = float3((float)size.x, 1, (float)size.y);
			g_debugdraw.wire_cube((float3)base_pos + fsize*0.5f, fsize, lrgba(1,0,0,1));
		};
		show();
	}
};

class RadianceCascades3D {
public:
	SERIALIZE(RadianceCascades3D, imopen, base_pos, size, base_spacing, base_rays_oct,
		dbg_pos, vis)

	bool imopen = true;

	// limited region the probes exist in for testing purposed
	int3 base_pos = 0;
	int3 size = 100;

	float base_spacing = 4; // cascade0 probe spacing
	int base_rays_oct = 2; // width/height of ocahedral encoding used for probe rays (?)

	float3 dbg_pos = 0;
	
	float get_spacing () { // for casc0 for now
		return base_spacing;
	}
	int2 get_rays_oct () { // for casc0 for now
		return int2(base_rays_oct, base_rays_oct);
	}
	int3 get_num_probes (float spacing) {
		return ceili((float3)size / spacing);
	}

	bool recreate = true;
	
	gl::Shader* trace_shad;
	gl::Shader* vis_shad;

	RCProbeTexture3D cascade0_tex;
	
	QuadDrawer vis_draw;

	bool vis = true;

	RadianceCascades3D (OpenglRenderer& r): vis_draw{r} {
		trace_shad = r.shaders.compile("rc_trace3d", {}, {{ COMPUTE_SHADER }});
		vis_shad = r.shaders.compile("rc_vis3d");
	}

	void imgui () {
		if (!imopen) return;
		if (ImGui::Begin("RadianceCascades3D", &imopen)) {

			ImGui::DragInt3("base_pos", &base_pos.x, 0.1f);
			recreate |= ImGui::DragInt3("size", &size.x, 0.1f);

			recreate |= ImGui::DragFloat("base_spacing", &base_spacing, 0.1f, 0.1f, 64);
			recreate |= ImGui::DragInt("base_rays_oct", &base_rays_oct, 0.1f, 1, 128);
			
			ImGui::DragFloat3("dbg_pos", &dbg_pos.x, 0.1f);
			ImGui::Checkbox("visualize", &vis);
			
			{ // Mem use
				auto mem_sz = cascade0_tex.mem_size();
				ImGui::Text("Total Mem: %.3f MB", (float)mem_sz / 1024/1024);

				int3 probes = get_num_probes(base_spacing);
				int2 rays_oct = get_rays_oct();
				size_t total = (size_t)probes.x * probes.y * probes.z * rays_oct.x * rays_oct.y;
				ImGui::Text("Cascade 0: %dx%dx%d(%llu) Probes at %dx%d(%llu) Rays (%.3f Mrays)",
					probes.x, probes.y, probes.z, (size_t)probes.x * probes.y * probes.z,
					rays_oct.x, rays_oct.y, (size_t)rays_oct.x * rays_oct.y,
					(float)total / 1000000);
			}
		}
		ImGui::End();
	}

	void do_recreate () {

		int3 probes = get_num_probes(base_spacing);
		int2 rays_oct = get_rays_oct();
		cascade0_tex = RCProbeTexture3D("RCtex", probes, rays_oct);
	}
	void update (OpenglRenderer& r) {
		if (!imopen) return;
		if (recreate) do_recreate();
		recreate = false;

		OGL_TRACE("radiance cascades 3d");
	
		lrgba dbg_cols[] = {
			lrgba(0,0,1,1),
			lrgba(0,1,0,1),
			lrgba(1,0,0,1),
			lrgba(1,1,0,1),
			lrgba(1,0,1,1),
			lrgba(0,1,1,1),
		};

		if (trace_shad->prog) {
			glUseProgram(trace_shad->prog);

			r.state.bind_textures(trace_shad, {
				{"tile_textures", r.tile_textures, r.pixelated_sampler},
				{"voxel_tex", r.raytracer->voxel_tex.tex},
				{"df_tex", r.raytracer->df_tex.tex},
			});
			trace_shad->set_uniform("voxtex_world_min", (float3)(r.raytracer->voxtex_offset * CHUNK_SIZE));

			float spacing = get_spacing();
			int2 rays_oct = get_rays_oct();

			int3 dbg_idx = floori(dbg_pos / spacing - 0.5f);

			trace_shad->set_uniform("world_base_pos", base_pos);
			trace_shad->set_uniform("world_size", size);
			trace_shad->set_uniform("spacing", spacing);
			trace_shad->set_uniform("rays_oct", rays_oct);
			trace_shad->set_uniform("dbg_idx", dbg_idx);
			trace_shad->set_uniform("update_debugdraw", r.debug_draw.update_indirect);

			glBindImageTexture(4, cascade0_tex.tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
		
			int3 total = cascade0_tex.total_size();
			trace_shad->set_uniform("dispatch_size", total);
		
			constexpr int3 GROUP_SZ = int3(4,4,4);
			int3 dispatch_size = (total + GROUP_SZ-1) / GROUP_SZ;
			glDispatchCompute(dispatch_size.x, dispatch_size.y, dispatch_size.z);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
		}
	
		{
			g_debugdraw.wire_cube((float3)base_pos + (float3)size*0.5f, (float3)size, lrgba(1,0,0,1));
		
			float spacing = get_spacing();
			int2 rays_oct = get_rays_oct();

			float3 tex_size = (float3)get_num_probes(spacing) * spacing;

			int z_idx = floori(dbg_pos.z / spacing - 0.5f);
			float z_slice = ((float)z_idx + 0.5f) * spacing;

			float3 p = (float3)base_pos + float3(0,0,z_slice);

			if (vis) {
				if (vis_shad->prog) {
					glUseProgram(vis_shad->prog);
				
					r.state.bind_textures(vis_shad, {
						{"cascade0_tex", cascade0_tex.tex},
					});
				
					vis_shad->set_uniform("world_base_pos", base_pos);
					vis_shad->set_uniform("world_size", size);
					vis_shad->set_uniform("spacing", spacing);
					vis_shad->set_uniform("rays_oct", rays_oct);

					vis_draw.draw_using(vis_shad, r.state, transform(p, float3(0,0,0), tex_size));
				}
			}
			else {
				vis_draw.draw(cascade0_tex.tex, r.state, transform(p, float3(0,0,0), tex_size), z_idx,-1, true);
			}
		}
	}
};
} // namespace gl
