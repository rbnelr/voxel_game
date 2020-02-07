#pragma once
#include "../kissmath.hpp"
#include "../dear_imgui.hpp"
#include "../util/collision.hpp"

enum perspective_mode {
	PERSPECTIVE,
	ORTHOGRAPHIC
};

struct View_Frustrum {
	// The view frustrum planes in world space
	// in the order: near, left, right, bottom, up, far
	// far was put last because it is not really needed in view frustrum culling since it is undesirable for the far plane to intersect any geometry anyway, so it usually gets get to far enough to never cull anything
	Plane		planes[6];

	// Frustrum corners in world space
	// in the order: LBN, RBN, RTN, LTN, LBF, RBF, RTF, LTF
	// (L=left R=right B=bottom T=top N=near F=far)
	float3		corners[8];
};

struct Camera_View {
	// World space to camera space transform
	float3x4	world_to_cam;

	// Camera space to world space transform
	float3x4	cam_to_world;

	// Camera space to clip space transform
	float4x4	cam_to_clip;

	// Clip space to camera space transform
	float4x4	clip_to_cam;

	View_Frustrum frustrum;

	void calc_frustrum();
};

class Camera {
public:
	// camera position
	float3				pos;

	// TODO: add quaternions
	//quaternion		base_ori = quaternion::identity;

	// camera rotation azimuth, elevation, roll in radians
	//  azimuth 0 has the camera looking towards +y, rotates the camera ccw around the z axis (deg(90) would have it face -x)
	//  elevation [0, deg(128)] represents [looking_down (-z), looking_up (+z)]
	//  roll rolls ccw with 0 having the camera top point up (+z)
	float3				rot_aer;

	perspective_mode	mode = PERSPECTIVE;

	// near clipping plane
	float				clip_near = 1.0f/32;
	// far clipping plane
	float				clip_far = 8192;

	// [mode == PERSPECTIVE] vertical fov (horizontal fov depends on render target aspect ratio)
	float				vfov = deg(70);

	// [mode == ORTHOGRAPHIC] vertical size (horizontal size depends on render target aspect ratio)
	float				ortho_vsize = 10;

	Camera (float3 pos=0, float3 rot_aer=0): pos{pos}, rot_aer{rot_aer} {}

	virtual ~Camera () = default;

	void imgui (const char* name=nullptr) {
		if (!imgui_push("Camera", name, false)) return;

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

		imgui_pop();
	}

	// Calculate camera projection matrix
	float4x4 calc_cam_to_clip (View_Frustrum* frust=nullptr, float4x4* clip_to_cam=nullptr);
};

float4x4 perspective_matrix (float vfov, float aspect, float clip_near=1.0f/32, float clip_far=8192, View_Frustrum* frust=nullptr, float4x4* clip_to_cam=nullptr);
float4x4 orthographic_matrix (float vsize, float aspect, float clip_near=1.0f/32, float clip_far=8192, View_Frustrum* frust=nullptr, float4x4* clip_to_cam=nullptr);

// rotate azimuth, elevation via mouselook
void rotate_with_mouselook (float* azimuth, float* elevation, float vfov);

// Calculate rotation matricies for azimuth, elevation
float3x3 calc_ae_rotation (float2 ae, float3x3* out_inverse=nullptr);

// Calculate rotation matricies for azimuth, elevation and roll
float3x3 calc_aer_rotation (float3 aer, float3x3* out_inverse=nullptr);

// Free flying camera
class Flycam : public Camera {
public:
	float base_speed = 0.5f;
	float max_speed = 1000.0f;
	float speedup_factor = 2;
	float fast_multiplier = 4;

	float cur_speed = 0;

	// TODO: configurable input bindings

	Flycam (float3 pos=0, float3 rot_aer=0, float base_speed=0.5f): Camera(pos, rot_aer), base_speed{base_speed} {}

	void imgui (const char* name=nullptr) {
		if (!imgui_push("Flycam", name)) return;

		Camera::imgui(name);

		ImGui::DragFloat("base_speed", &base_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", 1.05f);
		ImGui::DragFloat("max_speed", &max_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", 1.05f);
		ImGui::DragFloat("speedup_factor", &speedup_factor, 0.001f);
		ImGui::DragFloat("fast_multiplier", &fast_multiplier, 0.05f);
		ImGui::Text("cur_speed: %.3f", cur_speed);

		imgui_pop();
	}

	Camera_View update ();
};
