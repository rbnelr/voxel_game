#include "common.hpp"
#include "opengl_renderer.hpp"

#include "GLFW/glfw3.h" // include after glad

namespace gl {

// Repeat reloading of assets because reacting to filechanges often fails because of half-written files
template <typename FUNC>
bool try_reloading (FUNC loadfunc) {
	int max_retries = 100;
	int retry_delay = 10; // ms

	for (int i=0; i<max_retries; ++i) {
		Sleep(retry_delay); // start out with a delay in hopes of getting a working file the first time

		if (loadfunc())
			return true; // success
	}
	return false; // fail
}

void OpenglRenderer::frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files) {
	ctx.imgui_begin();

	shaders.update_recompilation(changed_files, wireframe);

	if (changed_files.contains("textures/atlas.png", FILE_ADDED|FILE_MODIFIED|FILE_RENAMED_NEW_NAME)) {
		clog(INFO, "[OpenglRenderer] Reload textures due to file change");
		try_reloading([&] () { return load_textures(); });
	}

	framebuffer.update(I.window_size);
}

void OpenglRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {

	{
		OGL_TRACE("upload_remeshed");
		chunk_renderer.upload_remeshed(game.world->chunks);
	}

	glLineWidth(line_width);
	{
		state.override_poly = wireframe;
		state.override_cull = wireframe && wireframe_backfaces;
		state.override_state.poly_mode = POLY_LINE;
		state.override_state.culling = false;

		state.set_default();
	}

	{
		CommonUniforms u = { I, game, I.window_size };
		upload_bind_ubo(common_uniforms, 0, &u, sizeof(u));
	}

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
	glViewport(0,0, framebuffer.size.x, framebuffer.size.y);
	glScissor(0,0, framebuffer.size.x, framebuffer.size.y);

	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	PipelineState s;

	//
	bind_ubo(block_meshes_ubo, 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);
	glBindSampler(0, tile_sampler);

	chunk_renderer.draw_chunks(*this, game);

	//
	if (game.world->player.selected_block)
		draw_block_highlight(*this, game.world->player.selected_block);

	//
	debug_draw.draw(*this);

	{
		OGL_TRACE("framebuffer.blit");
		framebuffer.blit(I.window_size);
	}
	glViewport(0,0, I.window_size.x, I.window_size.y);
	glScissor(0,0, I.window_size.x, I.window_size.y);

	ctx.imgui_draw();

	TracyGpuCollect;

	glfwSwapBuffers(window);
}


// rotate from facing up to facing in a block face direction
static inline constexpr float3x3 face_rotation[] = {
	// BF_NEG_X  = rotate3_Z(-deg(90)) * rotate3_X(deg(90)),
	float3x3( 0, 0,-1,
	-1, 0, 0,
	0, 1, 0),
	// BF_POS_X  = rotate3_Z(+deg(90)) * rotate3_X(deg(90)),
	float3x3( 0, 0, 1,
	1, 0, 0,
	0, 1, 0),
	// BF_NEG_Y  = rotate3_Z(-deg(0)) * rotate3_X(deg(90)),
	float3x3( 1, 0, 0,
	0, 0,-1,
	0, 1, 0),
	// BF_POS_Y  = rotate3_Z(+deg(180)) * rotate3_X(deg(90)),
	float3x3(-1, 0, 0,
	0, 0, 1,
	0, 1, 0),
	// BF_NEG_Z  = rotate3_X(deg(180)),
	float3x3( 1, 0, 0,
	0,-1, 0,
	0, 0,-1),
	// BF_POS_Z  = float3x3::identity()
	float3x3( 1, 0, 0,
	0, 1, 0,
	0, 0, 1),
};

struct Vertex_BlockHighlight {
	float3	pos;
	lrgba	col;

	template <typename ATTRIBS>
	static void attributes (ATTRIBS& a) {
		int loc = 0;
		a.init(sizeof(Vertex_BlockHighlight));
		a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "pos", offsetof(Vertex_BlockHighlight, pos));
		a.template add<AttribMode::FLOAT, decltype(col)>(loc++, "col", offsetof(Vertex_BlockHighlight, col));
	}
};
Mesh block_highlight () {

	lrgba col = srgba(40,40,40,240);
	lrgba dot_col = srgba(40,40,40,240);

	float r = 0.504f;
	float inset = 1.0f / 100;

	float side_r = r * 0.04f;

	std::vector<Vertex_BlockHighlight> vertices;

	for (int face=0; face<6; ++face) {

		auto vert = [&] (float3 v, lrgba col) {
			vertices.push_back(Vertex_BlockHighlight{ face_rotation[face] * v, col });
		};
		auto quad = [&] (float3 a, float3 b, float3 c, float3 d, lrgba col) {
			vert(b, col);	vert(c, col);	vert(a, col);
			vert(a, col);	vert(c, col);	vert(d, col);
		};

		quad(	float3(-r,-r,+r),
			float3(+r,-r,+r),
			float3(+r,-r,+r) + float3(-inset,+inset,0),
			float3(-r,-r,+r) + float3(+inset,+inset,0),
			col);

		quad(	float3(+r,-r,+r),
			float3(+r,+r,+r),
			float3(+r,+r,+r) + float3(-inset,-inset,0),
			float3(+r,-r,+r) + float3(-inset,+inset,0),
			col);

		quad(	float3(+r,+r,+r),
			float3(-r,+r,+r),
			float3(-r,+r,+r) + float3(+inset,-inset,0),
			float3(+r,+r,+r) + float3(-inset,-inset,0),
			col);

		quad(	float3(-r,+r,+r),
			float3(-r,-r,+r),
			float3(-r,-r,+r) + float3(+inset,+inset,0),
			float3(-r,+r,+r) + float3(+inset,-inset,0),
			col);

		if (face == BF_POS_Z) {
			// face highlight
			quad(float3(-side_r,-side_r,+r),
				float3(+side_r,-side_r,+r),
				float3(+side_r,+side_r,+r),
				float3(-side_r,+side_r,+r),
				dot_col );
		}
	}

	return upload_mesh("block_highlight", vertices.data(), vertices.size());
}
void OpenglRenderer::draw_block_highlight (OpenglRenderer& r, SelectedBlock& block) {
	OGL_TRACE("block_highlight");

	PipelineState s;
	s.depth_test = true;
	s.blend_enable = false;
	r.state.set(s);

	glUseProgram(r.block_highl_shad->prog);

	r.block_highl_shad->set_uniform("block_pos", (float3)block.pos);
	r.block_highl_shad->set_uniform("face_rotation", face_rotation[ (BlockFace)(block.face >= 0 ? block.face : 0) ]);

	glBindVertexArray(r.block_highl_mesh.vao);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)r.block_highl_mesh.vertex_count);
}

void glDebugDraw::draw (OpenglRenderer& r) {
	OGL_TRACE("debug_draw");

	PipelineState s;
	s.depth_test = true;
	s.depth_write = false;
	s.blend_enable = true;

	PipelineState s_occluded;
	s_occluded.depth_test = true;
	s_occluded.depth_func = DEPTH_BEHIND;
	s_occluded.depth_write = false;
	s_occluded.blend_enable = true;

	{
		OGL_TRACE("draw lines");

		glBindVertexArray(vbo_lines.vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_lines.vbo);

		GLsizeiptr vertex_count = g_debugdraw.lines.size() * sizeof(DebugDraw::LineVertex);
		glBufferData(GL_ARRAY_BUFFER, vertex_count, nullptr, GL_STREAM_DRAW);
		if (vertex_count > 0) {
			glBufferData(GL_ARRAY_BUFFER, vertex_count, g_debugdraw.lines.data(), GL_STREAM_DRAW);

			{ // lines in front of geometry
				OGL_TRACE("normal");

				r.state.set(s);
				glUseProgram(shad_lines->prog);

				glDrawArrays(GL_LINES, 0, (GLsizei)vertex_count);
			}
			if (draw_occluded) { // lines occluded by geometry
				OGL_TRACE("occluded");

				r.state.set(s_occluded);
				glUseProgram(shad_lines_occluded->prog);

				shad_lines_occluded->set_uniform("occluded_alpha", occluded_alpha);

				glDrawArrays(GL_LINES, 0, (GLsizei)vertex_count);
			}
		}
	}

	{
		OGL_TRACE("draw tris");

		r.state.set(s);
		glUseProgram(shad_tris->prog);

		glBindVertexArray(vbo_tris.vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_tris.vbo);

		GLsizeiptr vertex_count = g_debugdraw.tris.size() * sizeof(DebugDraw::TriVertex);
		glBufferData(GL_ARRAY_BUFFER, vertex_count, nullptr, GL_STREAM_DRAW);
		if (vertex_count > 0) {
			glBufferData(GL_ARRAY_BUFFER, vertex_count, g_debugdraw.tris.data(), GL_STREAM_DRAW);

			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
		}
	}
}

bool OpenglRenderer::load_textures () {
	Image<srgba8> img;
	if (!img.load_from_file("textures/atlas.png", &img))
		return false;

	// place layers at y dir so ot make the memory contiguous
	Image<srgba8> img_arr (int2(16, 16 * TILEMAP_SIZE.x * TILEMAP_SIZE.y));
	{ // convert texture atlas/tilemap into texture array for proper sampling in shader
		for (int y=0; y<TILEMAP_SIZE.y; ++y) {
			for (int x=0; x<TILEMAP_SIZE.x; ++x) {
				Image<srgba8>::blit_rect(
					img, int2(x,y)*16,
					img_arr, int2(0, (y * TILEMAP_SIZE.x + x) * 16),
					16);
			}
		}
	}


	glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, 16, 16, TILEMAP_SIZE.x * TILEMAP_SIZE.y, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, img_arr.pixels);

	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	return true;
}

void OpenglRenderer::load_static_data () {

	upload_ubo(block_meshes_ubo,
		g_assets.block_meshes.slices.data(),
		g_assets.block_meshes.slices.size() * sizeof(g_assets.block_meshes.slices[0]));

	load_textures();
}

} // namespace gl
