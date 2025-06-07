#pragma once
#include "input.hpp"
#include "kisslib/kissmath.hpp"
#include "kisslib/collision.hpp"

enum perspective_mode {
	PERSPECTIVE,
	ORTHOGRAPHIC
};
NLOHMANN_JSON_SERIALIZE_ENUM(perspective_mode, {{PERSPECTIVE, "PERSPECTIVE"}, {ORTHOGRAPHIC, "ORTHOGRAPHIC"}})

struct Camera_View {
	// World space to camera space transform
	float3x4	world_to_cam;

	// Camera space to world space transform
	float3x4	cam_to_world;

	// Camera space to clip space transform
	float4x4	cam_to_clip;

	// Clip space to camera space transform
	float4x4	clip_to_cam;

	// near clip plane distance (positive)
	float		clip_near;
	// far clip plane distance (positive)
	float		clip_far;

	float2		frustrum_size;

	View_Frustrum frustrum;

	void calc_frustrum();
};

inline bool project_world2screen (float3 const& pos_world, float4x4 const& world2clip, float2 const& screen_size, float2* pos_screen) {

	float4 c = world2clip * float4(pos_world, 1);

	if (  c.x < -c.w || c.x > c.w || 
		  c.y < -c.w || c.y > c.w ||
		  c.z < -c.w || c.z > c.w)
		return false; // clipped by frustrum

	float2 ndc = (float2)c / c.w; // perspective divide usually done by opengl

	float2 normalized = ndc * float2(0.5f,-0.5f) + 0.5f; // [-1,+1] to [0,1] also flip y to be top-down
	*pos_screen = normalized * screen_size;

	return true;
}
inline bool project_world2screen (float3 const& pos_world, Camera_View const& view, float2 const& screen_size, float2* pos_screen) {
	return project_world2screen(pos_world, view.cam_to_clip * (float4x4)view.world_to_cam, screen_size, pos_screen);
}

inline Ray screen_ray (float2 const& pos_px, Camera_View const& view, float2 const& screen_size) {
	
	float2 px_center = pos_px + 0.5f;
	float2 ndc = (px_center / screen_size * 2.0f - 1.0f) * float2(1,-1);

	float4 clip = float4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	float3 cam = (float3)(view.clip_to_cam * clip);

	Ray ray;

	ray.dir = (float3)(view.cam_to_world * float4(cam, 0));
	ray.dir = normalize(ray.dir);

	// ray starts on the near plane
	ray.pos = (float3)(view.cam_to_world * float4(cam, 1));

	return ray;
}

struct Camera {
	SERIALIZE(Camera, pos, rot_aer, mode, clip_near, clip_far, vfov, ortho_vsize)

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
	float4x4 calc_cam_to_clip (int2 viewport_size, float4x4* clip_to_cam=nullptr, View_Frustrum* frust=nullptr, float2* frust_size=nullptr);
};

float4x4 perspective_matrix (float vfov, float aspect, float clip_near=1.0f/32, float clip_far=8192, float4x4* clip_to_cam=nullptr, View_Frustrum* frust=nullptr, float2* frust_size=nullptr);
float4x4 orthographic_matrix (float vsize, float aspect, float clip_near=1.0f/32, float clip_far=8192, float4x4* clip_to_cam=nullptr, View_Frustrum* frust=nullptr, float2* frust_size=nullptr);

// rotate azimuth, elevation via mouselook
void rotate_with_mouselook (Input& I, float* azimuth, float* elevation, float vfov);

// Calculate rotation matricies for azimuth, elevation
float3x3 calc_ae_rotation (float2 ae, float3x3* out_inverse=nullptr);

// Calculate rotation matricies for azimuth, elevation and roll
float3x3 calc_aer_rotation (float3 aer, float3x3* out_inverse=nullptr);

// Free flying camera
struct Flycam {
	SERIALIZE(Flycam, cam, base_speed, max_speed, speedup_factor, fast_multiplier)

	Camera cam;

	float base_speed = 0.5f;
	float max_speed = 1000000.0f;
	float speedup_factor = 2;
	float fast_multiplier = 4;

	float cur_speed = 0;

	// TODO: configurable input bindings

	Flycam (float3 pos=0, float3 rot_aer=0, float base_speed=0.5f): cam(pos, rot_aer), base_speed{base_speed} {}

	void imgui (const char* name=nullptr) {
		if (!imgui_push("Flycam", name)) return;

		cam.imgui(name);

		ImGui::DragFloat("base_speed", &base_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("max_speed", &max_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("speedup_factor", &speedup_factor, 0.001f);
		ImGui::DragFloat("fast_multiplier", &fast_multiplier, 0.05f);
		ImGui::Text("cur_speed: %.3f", cur_speed);

		imgui_pop();
	}

	float3x3 calc_world_to_cam_rot (float3x3* cam_to_world_rot);

	Camera_View update (Input& I, int2 const& viewport_size, bool lock_controls=false);
};
