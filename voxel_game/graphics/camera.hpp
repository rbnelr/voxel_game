#pragma once
#include "../kissmath.hpp"
#include "../dear_imgui.hpp"
#include <string>

enum perspective_mode {
	PERSPECTIVE,
	ORTHOGRAPHIC
};

struct Camera_View {
	// World space to camera space transform
	float3x4	world_to_cam;

	// Camera space to world space transform
	float3x4	cam_to_world;

	// Camera space to clip space transform
	float4x4	cam_to_clip;
};

class Camera {
public:
	std::string			name = "<Camera>";

	perspective_mode	mode = PERSPECTIVE;

	// camera position (in world)
	float3				pos = 0;

	// TODO: add quaternions
	//quaternion		base_ori = quaternion::identity;

	// camera rotation azimuth, elevation, roll in radians
	//  azimuth 0 has the camera looking towards +y, rotates the camera ccw around the z axis (deg(90) would have it face -x)
	//  elevation [0, deg(128)] represents [looking_down (-z), looking_up (+z)]
	//  roll rolls ccw with 0 having the camera top point up (+z)
	float3				rot_aer = float3(0, deg(90), 0);

	// near clipping plane
	float				clip_near = 1.0f/32;
	// far clipping plane
	float				clip_far = 8192;

	// [mode == PERSPECTIVE] vertical fov (horizontal fov depends on render target aspect ratio)
	float				vfov = deg(70);

	// [mode == ORTHOGRAPHIC] vertical size (horizontal size depends on render target aspect ratio)
	float				ortho_vsize = 10;

	Camera () {}
	Camera (std::string name, float3 pos=0, float3 rot_aer=float3(0, deg(90), 0)): name{std::move(name)}, pos{pos}, rot_aer{rot_aer} {}

	virtual ~Camera () = default;

	void imgui () {
		if (!imgui_treepush("Camera")) return;

		ImGui::Text("%s:", name.c_str());

		int cur_mode = (int)mode;
		ImGui::Combo("mode", &cur_mode, "PERSPECTIVE\0ORTHOGRAPHIC\0");
		mode = (perspective_mode)cur_mode;

		ImGui::DragFloat3("pos", &pos.x, 0.05f);

		float3 rot_aer_deg = to_degrees(rot_aer);
		if (ImGui::DragFloat3("rot_aer", &rot_aer_deg.x, 0.05f))
			rot_aer = to_radians(rot_aer_deg);

		ImGui::DragFloat("clip_near", &clip_near, 0.05f);
		ImGui::DragFloat("clip_far", &clip_far, 0.05f);
		ImGui::SliderAngle("vfov", &vfov, 0, 180.0f);
		ImGui::DragFloat("ortho_vsize", &ortho_vsize, 0.05f);

		imgui_treepop();
	}

	// Calculate camera transformation matricies
	Camera_View calc_view ();
};

float4x4 perspective_matrix (float vfov, float aspect, float clip_near=1.0f/32, float clip_far=8192);
float4x4 orthographic_matrix (float vsize, float aspect, float clip_near=1.0f/32, float clip_far=8192);

// rotate camera with azimuth, elevation, roll angles (azimuth and roll wrap and elevation is clamped)
void rotate_camera (Camera* cam, float3 aer_delta, float down_limit=deg(2), float up_limit=deg(2));

// translate camera with camera space vector
void translate_camera (Camera* cam, Camera_View const& view, float3 translation_local);

// rotate camera via mouselook, raw_mouselook should be raw dpi deltas coming from the mouse, sensitiviy_divider describes how many dpi counts it takes to rotate the camera by one visual screen height (so this makes is visually consistent)
void rotate_camera_with_mouselook (Camera* cam, float2 raw_mouselook, float sensitiviy_divider);

// Free flying camera
class Flycam : public Camera {
public:
	float base_speed = 0.5f;
	float max_speed = 1000.0f;
	float speedup_factor = 2;
	float fast_multiplier = 4;

	float cur_speed = 0;

	// TODO: configurable input bindings

	Flycam () {}
	Flycam (std::string name, float3 pos=0, float3 rot_aer=float3(0, deg(90), 0), float base_speed=0.5f): Camera(name, pos, rot_aer), base_speed{base_speed} {}

	void imgui () {
		if (!imgui_treepush("Flycam")) return;

		Camera::imgui();

		ImGui::DragFloat("base_speed", &base_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", 1.05f);
		ImGui::DragFloat("max_speed", &max_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", 1.05f);
		ImGui::DragFloat("speedup_factor", &speedup_factor, 0.001f);
		ImGui::DragFloat("fast_multiplier", &fast_multiplier, 0.05f);
		ImGui::Text("cur_speed: %.3f", cur_speed);

		imgui_treepop();
	}

	Camera_View update ();
};
