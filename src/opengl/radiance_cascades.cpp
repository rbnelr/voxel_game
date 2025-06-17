#include "common.hpp"
#include "radiance_cascades.hpp"
#include "game.hpp"
#include "opengl_renderer.hpp"

namespace gl {

TexturedQuadDrawer::TexturedQuadDrawer (OpenglRenderer& r) {
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

RadianceCascades2D::RadianceCascades2D (OpenglRenderer& r): tex_draw{r} {
	trace_shad = r.shaders.compile("rc_trace", {}, {{ COMPUTE_SHADER }});
	combine_shad = r.shaders.compile("rc_combine", {}, {{ COMPUTE_SHADER }});
}

void RadianceCascades2D::update (Game& game, OpenglRenderer& r) {
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
				{"voxel_tex", r.raytracer.voxel_tex.tex},
				{"df_tex", r.raytracer.df_tex.tex},

				(has_higher_cascade ?
					StateManager::TextureBind{"higher_cascade", cascade_texs[casc+1].tex} :
					StateManager::TextureBind{"higher_cascade", Texture2D{}}),
			});
			
			trace_shad->set_uniform("voxtex_world_min", (float3)(r.raytracer.voxtex_offset * CHUNK_SIZE));

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

		if (show_cascade >= 0) tex_draw.draw(cascade_texs[show_cascade].tex, r.state, transform(p, float3(deg(90),0,0), sz), show_ray,-1, true);
		else                   tex_draw.draw(result_tex.tex, r.state, transform(p, float3(deg(90),0,0), sz), true);
	
		float3 fsize = float3((float)size.x, 1, (float)size.y);
		g_debugdraw.wire_cube((float3)base_pos + fsize*0.5f, fsize, lrgba(1,0,0,1));
	};
	show();
}


RadianceCascades3D::RadianceCascades3D (OpenglRenderer& r): tex_draw{r} {
	trace_shad = r.shaders.compile("rc_trace3d", {}, {{ COMPUTE_SHADER }});
}

void RadianceCascades3D::update (Game& game, OpenglRenderer& r) {
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
			{"voxel_tex", r.raytracer.voxel_tex.tex},
			{"df_tex", r.raytracer.df_tex.tex},
		});
		trace_shad->set_uniform("voxtex_world_min", (float3)(r.raytracer.voxtex_offset * CHUNK_SIZE));

		float spacing = get_spacing();
		int2 rays_oct = get_rays_oct();

		int3 dbg_idx = floori(_dbg_pos / spacing - 0.5f);

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
	}
	
	{
		g_debugdraw.wire_cube((float3)base_pos + (float3)size*0.5f, (float3)size, lrgba(1,0,0,1));

		float spacing = get_spacing();
		float3 tex_size = (float3)get_num_probes(spacing) * spacing;

		int z_idx = floori(_dbg_pos.z / spacing - 0.5f);
		float z_slice = ((float)z_idx + 0.5f) * spacing;

		float3 p = (float3)base_pos + float3(0,0,z_slice);
		tex_draw.draw(cascade0_tex.tex, r.state, transform(p, float3(0,0,0), tex_size), z_idx,-1, true);
	}
}
}
