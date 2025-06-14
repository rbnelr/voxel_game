#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"

namespace gl {
class OpenglRenderer;

struct glDebugDraw {
	SERIALIZE(glDebugDraw, draw_occluded, occluded_alpha, line_width)

	VertexBuffer vbo_lines	= vertex_buffer<DebugDraw::LineVertex>("DebugDraw.vbo_lines");
	VertexBuffer vbo_tris	= vertex_buffer<DebugDraw::TriVertex> ("DebugDraw.vbo_tris");

	IndexedInstancedBuffer vbo_wire_cube = indexed_instanced_buffer<DebugDraw::PosVertex, DebugDraw::Instance>("DebugDraw.vbo_wire_cube");
	IndexedInstancedBuffer vbo_wire_sphere = indexed_instanced_buffer<DebugDraw::PosVertex, DebugDraw::Instance>("DebugDraw.vbo_wire_sphere");

	struct IndirectLineVertex {
		float4 pos; // vec4 for std430 alignment
		float4 col;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(IndirectLineVertex));
			a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "pos", offsetof(IndirectLineVertex, pos));
			a.template add<AttribMode::FLOAT, decltype(col)>(loc++, "col", offsetof(IndirectLineVertex, col));
		}
	};
	struct IndirectWireInstace {
		float4 pos; // vec4 for std430 alignment
		float4 size; // vec4 for std430 alignment
		float4 col;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			a.init(sizeof(IndirectWireInstace), true);
			a.template add<AttribMode::FLOAT, decltype(pos)>(1, "instance_pos", offsetof(IndirectWireInstace, pos));
			a.template add<AttribMode::FLOAT, decltype(size)>(2, "instance_size", offsetof(IndirectWireInstace, size));
			a.template add<AttribMode::FLOAT, decltype(col)>(3, "instance_col", offsetof(IndirectWireInstace, col));
		}
	};
	struct IndirectBuffer {
		struct Lines {
			glDrawArraysIndirectCommand cmd;
			IndirectLineVertex vertices[4096];
		} lines;

		struct WireInstances {
			glDrawElementsIndirectCommand cmd;
			uint32_t _pad[3]; // padding for std430 alignment
			IndirectWireInstace vertices[4096];
		};
		
		WireInstances wire_cubes;
		WireInstances wire_spheres;
	};

	Vbo indirect_vbo = {"DebugDraw.indirect_draw"};
	
	Vao indirect_lines_vao     = setup_vao<IndirectLineVertex>("DebugDraw.indirect_lines", { indirect_vbo, offsetof(IndirectBuffer, lines.vertices[0]) });
	
	Vao indirect_wire_cube_vao = setup_instanced_vao<DebugDraw::PosVertex, IndirectWireInstace>("DebugDraw.indirect_wire_cubes", 
		{ indirect_vbo, offsetof(IndirectBuffer, wire_cubes.vertices[0]) },
		vbo_wire_cube.mesh_vbo, vbo_wire_cube.mesh_ebo);

	Vao indirect_wire_sphere_vao = setup_instanced_vao<DebugDraw::PosVertex, IndirectWireInstace>("DebugDraw.indirect_wire_spheres", 
		{ indirect_vbo, offsetof(IndirectBuffer, wire_spheres.vertices[0]) },
		vbo_wire_sphere.mesh_vbo, vbo_wire_sphere.mesh_ebo);

	int wire_sphere_indices_count;

	void clear_indirect () {
		glBindBuffer(GL_ARRAY_BUFFER, indirect_vbo);

		{
			glDrawArraysIndirectCommand cmd = {};
			cmd.instanceCount = 1;
			glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, lines.cmd), sizeof(cmd), &cmd);
		}

		{
			glDrawElementsIndirectCommand cmd = {};
			cmd.count = ARRLEN(DebugDraw::_wire_cube_indices);
			glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, wire_cubes.cmd), sizeof(cmd), &cmd);
		}

		{
			glDrawElementsIndirectCommand cmd = {};
			cmd.count = wire_sphere_indices_count;
			glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, wire_spheres.cmd), sizeof(cmd), &cmd);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	Shader* shad_lines;
	Shader* shad_lines_occluded;
	Shader* shad_wire_instance;

	Shader* shad_tris;

	bool draw_occluded = false;
	float occluded_alpha = 0.3f;

	bool update_indirect = false;
	bool accum_indirect = false;
	bool _clear_indirect = false;

	
	bool  wireframe = false;
	bool  wireframe_solid = true;
	bool  wireframe_backfaces = true;
	float line_width = 1.0f;

	int  _wireframe_pass;

	void set_overrides (StateManager& state) {
		state.override_poly = wireframe && (!wireframe_solid || _wireframe_pass > 0);
		state.override_cull = wireframe && wireframe_backfaces;

		state.override_state.poly_mode = POLY_LINE;
		state.override_state.culling = false;
	}

	template <typename FUNC>
	void draw_wireframe_able (StateManager& state, FUNC draw_func) {
		_wireframe_pass = 0;
		set_overrides(state);
		draw_func();

		if (wireframe && wireframe_solid) {
			_wireframe_pass = 1;
			set_overrides(state);

			glEnable(GL_POLYGON_OFFSET_LINE);
			glPolygonOffset(0.0f, 2.0f);

			draw_func();

			glDisable(GL_POLYGON_OFFSET_LINE);
		}

		set_overrides(state);
	}
	void wireframe_able_shader (Shader* shader) {
		if (wireframe) {
			bool val = false;
			if (wireframe_solid) {
				val = _wireframe_pass > 0; // draw normal first, then wireframe on top
			}
			else {
				val = true; // normal wireframe
			}
			shader->set_uniform("_WIREFRAME_PASS", val);
		}
	}

	void imgui () {
		ImGui::Checkbox("wireframe", &wireframe);
		ImGui::SameLine();
		ImGui::Checkbox("solid", &wireframe_solid);
		ImGui::Checkbox("backfaces", &wireframe_backfaces);

		ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);

		ImGui::Checkbox("draw_occluded", &draw_occluded);
		ImGui::SliderFloat("occluded_alpha", &occluded_alpha, 0,1);

		ImGui::Checkbox("update_indirect [T]", &update_indirect);
		_clear_indirect = ImGui::Button("clear_indirect") || _clear_indirect;
		ImGui::SameLine();
		ImGui::Checkbox("accum_indirect", &accum_indirect);
	}

	glDebugDraw (Shaders& shaders) {
		shad_lines			= shaders.compile("debug_lines", {{"DRAW_OCCLUDED", "0"}});
		shad_lines_occluded	= shaders.compile("debug_lines", {{"DRAW_OCCLUDED", "1"}});
		shad_tris			= shaders.compile("debug_tris");

		shad_wire_instance	= shaders.compile("debug_wire_instance", {{"DRAW_OCCLUDED", "0"}});

		{
			glBindBuffer(GL_ARRAY_BUFFER, vbo_wire_cube.mesh_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(DebugDraw::_wire_cube_vertices), DebugDraw::_wire_cube_vertices, GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_wire_cube.mesh_ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(DebugDraw::_wire_cube_indices), DebugDraw::_wire_cube_indices, GL_STATIC_DRAW);
		}

		{
			std::vector<uint16_t> indices;
			std::vector<float3> vertices;
			DebugDraw::gen_simple_wire_sphere(&vertices, &indices, 0.5f, 24);

			wire_sphere_indices_count = (int)indices.size();

			glBindBuffer(GL_ARRAY_BUFFER, vbo_wire_sphere.mesh_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float3)*vertices.size(), vertices.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_wire_sphere.mesh_ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t)*indices.size(), indices.data(), GL_STATIC_DRAW);
		}

		glBindBuffer(GL_ARRAY_BUFFER, indirect_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(IndirectBuffer), nullptr, GL_STREAM_DRAW);
		clear_indirect();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void update (Input& I) {
		if (I.buttons[KEY_T].went_down) {
			if (I.buttons[KEY_LEFT_SHIFT].is_down) {
				update_indirect = false;
				_clear_indirect = true;
			} else {
				update_indirect = !update_indirect;
			}
		}

		if (_clear_indirect || (update_indirect && !accum_indirect)) {
			clear_indirect();
			_clear_indirect = false;
		}
	}

	void draw (OpenglRenderer& r);
};

} // namespace gl
