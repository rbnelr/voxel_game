#pragma once
#include "gl.hpp"
#include "camera.hpp"
#include "shaders.hpp"
#include "../input.hpp"

struct ViewUniforms {
	float4x4 world_to_cam;
	float4x4 cam_to_world;
	float4x4 cam_to_clip;
	float4x4 world_to_clip;
	float2 viewport_size;

	static constexpr void check_layout (SharedUniformsLayoutChecker& c) {
		c.member<decltype(world_to_cam )>(offsetof(ViewUniforms, world_to_cam ));
		c.member<decltype(cam_to_world )>(offsetof(ViewUniforms, cam_to_world ));
		c.member<decltype(cam_to_clip  )>(offsetof(ViewUniforms, cam_to_clip  ));
		c.member<decltype(world_to_clip)>(offsetof(ViewUniforms, world_to_clip));
		c.member<decltype(viewport_size)>(offsetof(ViewUniforms, viewport_size));
	}
};
struct DebugUniforms {
	float2 cursor_pos;
	int wireframe;

	static constexpr void check_layout (SharedUniformsLayoutChecker& c) {
		c.member<decltype(cursor_pos)>(offsetof(DebugUniforms, cursor_pos));
		c.member<decltype(wireframe )>(offsetof(DebugUniforms, wireframe ));
	}
};

static inline constexpr SharedUniformsInfo COMMON_UNIFORMS[] = {
	{ "View", 0 },
	{ "Debug", 1 },
};

struct CommonUniforms {
	SharedUniforms<ViewUniforms> view_uniforms = COMMON_UNIFORMS[0];
	SharedUniforms<DebugUniforms> debug_uniforms = COMMON_UNIFORMS[1];

	bool dbg_wireframe = false;
	bool wireframe_shaded = false;
	bool wireframe_colored = false;
	bool wireframe_backfaces = true;

	void imgui () {
		ImGui::Checkbox("dbg_wireframe", &dbg_wireframe);
		ImGui::SameLine();
		ImGui::Checkbox("shaded", &wireframe_shaded);
		ImGui::SameLine();
		ImGui::Checkbox("colored", &wireframe_colored);
		ImGui::SameLine();
		ImGui::Checkbox("backfaces", &wireframe_backfaces);

		int size = sizeof(DebugUniforms);
	}

	void set_view_uniforms (Camera_View const& view) {
		ViewUniforms u = {}; // zero padding
		u.world_to_cam = (float4x4)view.world_to_cam;
		u.cam_to_world = (float4x4)view.cam_to_world;
		u.cam_to_clip = view.cam_to_clip;
		u.world_to_clip = view.cam_to_clip * (float4x4)view.world_to_cam;
		u.viewport_size = (float2)input.window_size;
		view_uniforms.set(u);
	}

	void set_debug_uniforms () {
		DebugUniforms u = {}; // zero padding
		u.cursor_pos = input.cursor_pos;
		u.wireframe = dbg_wireframe ? 1 | (wireframe_shaded ? 2:0) | (wireframe_colored ? 4:0) : 0;
		debug_uniforms.set(u);
	}
};
