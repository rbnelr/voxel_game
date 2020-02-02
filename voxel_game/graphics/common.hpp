#pragma once
#include "gl.hpp"
#include "camera.hpp"
#include "shader_manager.hpp"
#include "../input.hpp"

struct ViewUniforms {
	float4x4 world_to_cam;
	float4x4 cam_to_world;
	float4x4 cam_to_clip;
	float2 viewport_size;

	static constexpr void check_layout (SharedUniformsLayoutChecker& c) {
		c.member<decltype(world_to_cam )>(offsetof(ViewUniforms, world_to_cam ));
		c.member<decltype(cam_to_world )>(offsetof(ViewUniforms, cam_to_world ));
		c.member<decltype(cam_to_clip  )>(offsetof(ViewUniforms, cam_to_clip  ));
		c.member<decltype(viewport_size)>(offsetof(ViewUniforms, viewport_size));
	}
};
struct DebugUniforms {
	float2 cursor_pos;

	static constexpr void check_layout (SharedUniformsLayoutChecker& c) {
		c.member<decltype(cursor_pos)>(offsetof(DebugUniforms, cursor_pos));
	}
};

static inline constexpr SharedUniformsInfo COMMON_UNIFORMS[] = {
	{ "View", 0 },
	{ "Debug", 1 },
};

struct CommonUniforms {
	SharedUniforms<ViewUniforms> view_uniforms = COMMON_UNIFORMS[0];
	SharedUniforms<DebugUniforms> debug_uniforms = COMMON_UNIFORMS[1];

	void set_view_uniforms (Camera_View const& view) {
		ViewUniforms u;
		u.world_to_cam = (float4x4)view.world_to_cam;
		u.cam_to_world = (float4x4)view.cam_to_world;
		u.cam_to_clip = view.cam_to_clip;
		u.viewport_size = (float2)input.window_size;
		view_uniforms.set(u);
	}

	void set_debug_uniforms () {
		DebugUniforms u;
		u.cursor_pos = input.cursor_pos;
		debug_uniforms.set(u);
	}
};
