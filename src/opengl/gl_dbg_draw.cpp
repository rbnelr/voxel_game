#include "common.hpp"
#include "opengl_renderer.hpp"

namespace gl {

void glDebugDraw::draw (OpenglRenderer& r) {
	OGL_TRACE("debug_draw");

	PipelineState s;
	s.depth_test = true;
	s.depth_write = true;
	s.blend_enable = true;

	PipelineState s_occluded;
	s_occluded.depth_test = true;
	s_occluded.depth_func = DEPTH_BEHIND;
	s_occluded.depth_write = true;
	s_occluded.blend_enable = true;

	{
		OGL_TRACE("draw tris");

		glBindVertexArray(vbo_tris.vao);
		stream_buffer(vbo_tris, g_debugdraw.tris);

		if (g_debugdraw.tris.size() > 0) {
			r.state.set(s);
			glUseProgram(shad_tris->prog);
			r.state.bind_textures(shad_tris, {});

			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g_debugdraw.tris.size());
		}
	}

	{
		OGL_TRACE("draw lines");

		stream_buffer(vbo_lines, g_debugdraw.lines);

		if (g_debugdraw.lines.size() > 0) {
			{
				r.state.set(s);
				glUseProgram(shad_lines->prog);
				r.state.bind_textures(shad_lines, {});

				{
					OGL_TRACE("normal");

					glBindVertexArray(vbo_lines.vao);
					glDrawArrays(GL_LINES, 0, (GLsizei)g_debugdraw.lines.size());
				}
				{
					OGL_TRACE("normal indirect");

					glBindVertexArray(indirect_lines_vao);
					glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_vbo);
					
					glDrawArraysIndirect(GL_LINES, (void*)offsetof(IndirectBuffer, lines.cmd));

					glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
				}
			}
			if (draw_occluded) { // lines occluded by geometry

				r.state.set(s_occluded);
				glUseProgram(shad_lines_occluded->prog);
				r.state.bind_textures(shad_lines_occluded, {});

				shad_lines_occluded->set_uniform("occluded_alpha", occluded_alpha);

				{
					OGL_TRACE("occluded");

					glBindVertexArray(vbo_lines.vao);
					glDrawArrays(GL_LINES, 0, (GLsizei)g_debugdraw.lines.size());
				}
				{
					OGL_TRACE("occluded indirect");

					glBindVertexArray(indirect_lines_vao);
					glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_vbo);

					glDrawArraysIndirect(GL_LINES, (void*)offsetof(IndirectBuffer, lines.cmd));

					glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
				}
			}
		}
	}

	{
		OGL_TRACE("draw wire cubes");

		r.state.set(s);
		glUseProgram(shad_wire_instance->prog);
		r.state.bind_textures(shad_wire_instance, {});

		{
			glBindVertexArray(vbo_wire_cube.vao);
			reupload_vbo(vbo_wire_cube.instance_vbo, g_debugdraw.wire_cubes.data(), g_debugdraw.wire_cubes.size(), GL_STREAM_DRAW);

			if (g_debugdraw.wire_cubes.size() > 0) {
				glDrawElementsInstanced(GL_LINES, ARRLEN(DebugDraw::_wire_cube_indices), GL_UNSIGNED_SHORT,
					(void*)0, (GLsizei)g_debugdraw.wire_cubes.size());
			}
			
		}

		{
			glBindVertexArray(indirect_wire_cube_vao);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_vbo);

			glDrawElementsIndirect(GL_LINES, GL_UNSIGNED_SHORT, (void*)offsetof(IndirectBuffer, wire_cubes.cmd));

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		}

		{
			glBindVertexArray(indirect_wire_sphere_vao);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_vbo);

			glDrawElementsIndirect(GL_LINES, GL_UNSIGNED_SHORT, (void*)offsetof(IndirectBuffer, wire_spheres.cmd));
		}

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	}
}

} // namespace gl
