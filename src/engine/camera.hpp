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

	void calc_frustrum () {
		// frustrum_corners set to cam space by perspective_matrix() or orthographic_matrix()

		for (int i=0; i<8; ++i)
			frustrum.corners[i] = cam_to_world * frustrum.corners[i];

		//near, left, right, bottom, up, far

		auto& corn = frustrum.corners;

		frustrum.planes[0] = {
			(corn[0] + corn[2]) / 2,
			normalize(cross(corn[2] - corn[1], corn[0] - corn[1]))
		};

		frustrum.planes[1] = {
			(corn[0] + corn[3]) / 2,
			normalize(cross(corn[3] - corn[0], corn[4] - corn[0]))
		};

		frustrum.planes[2] = {
			(corn[1] + corn[2]) / 2,
			normalize(cross(corn[1] - corn[2], corn[6] - corn[2]))
		};

		frustrum.planes[3] = {
			(corn[0] + corn[1]) / 2,
			normalize(cross(corn[0] - corn[1], corn[5] - corn[1]))
		};

		frustrum.planes[4] = {
			(corn[3] + corn[2]) / 2,
			normalize(cross(corn[2] - corn[3], corn[7] - corn[3]))
		};

		frustrum.planes[5] = {
			(corn[4] + corn[6]) / 2,
			normalize(cross(corn[7] - corn[4], corn[5] - corn[4]))
		};
	}
};

struct CameraBinds {
	SERIALIZE(CameraBinds, rotate, rotate_sensitivity,
		move_left, move_right, move_back, move_forw, move_down, move_up,
		rot_left, rot_right, zoom_in, zoom_out, change_fov, modifier)

	Button rotate      = MOUSE_BUTTON_RIGHT;

	float rotate_sensitivity = deg(120) / 1000;
	
	Button move_left   = KEY_A;
	Button move_right  = KEY_D;
	Button move_back   = KEY_S;
	Button move_forw   = KEY_W;
	Button move_down   = KEY_LEFT_CONTROL;
	Button move_up     = KEY_SPACE;
	
	Button roll_left    = KEY_NULL;
	Button roll_right   = KEY_NULL;

	Button rot_left    = KEY_Q;
	Button rot_right   = KEY_E;

	Button zoom_in     = KEY_KP_ADD;
	Button zoom_out    = KEY_KP_SUBTRACT;

	Button change_fov  = KEY_F;

	Button modifier    = KEY_LEFT_SHIFT;

	float3 get_local_move_dir (Input& I) const {
		float3 move_dir = 0;
		if (I.buttons[move_left ].is_down) move_dir.x -= 1;
		if (I.buttons[move_right].is_down) move_dir.x += 1;
		if (I.buttons[move_forw ].is_down) move_dir.z -= 1;
		if (I.buttons[move_back ].is_down) move_dir.z += 1;
		if (I.buttons[move_down ].is_down) move_dir.y -= 1;
		if (I.buttons[move_up   ].is_down) move_dir.y += 1;
		return normalizesafe(move_dir);
	}

	float2 get_mouselook_delta (Input& I, float vfov) const {
		// sens scales with fov since muscle memory works with visual distances on screen, not with in game angles
		// (mostly? 180deg flip might also be muscle memory?)
		if (!I.cursor_enabled || I.buttons[rotate].is_down)
			return I.mouse_delta * rotate_sensitivity * vfov;
		return 0;
	}
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


inline float4x4 perspective_matrix (float vfov, float aspect, float clip_near=1.0f/32, float clip_far=8192,
		float4x4* clip_to_cam=nullptr, View_Frustrum* frust=nullptr, float2* frust_size=nullptr) {
	float2 frust_scale;
	frust_scale.y = tan(vfov / 2);
	frust_scale.x = frust_scale.y * aspect;

	float hfov = atan(frust_scale.x) * 2;

	float2 frust_scale_inv = 1.0f / frust_scale;

	float x = frust_scale_inv.x;
	float y = frust_scale_inv.y;

	float a, b;
	// use_reverse_depth with use infinite far plane
	// visible range on z axis in opengl and vulkan goes from -near to -inf
	// depth formula is: (with input w=1) z' = near, w' = -z  ->  depth = near/-z
	// depth values go from 1 at the near plane to 0 at infinity
	// inverse formula ist -z = near / depth
	clip_far = 1000000.0f; // can't actually set far to be infinite if I want frustrum culling to work without modification
	a = 0.0f;
	b = clip_near;

	//a = (clip_far + clip_near) / (clip_near - clip_far);
	//b = (2.0f * clip_far * clip_near) / (clip_near - clip_far);
	
	if (frust) {
		frust->corners[0] = float3(-frust_scale.x * clip_near, -frust_scale.y * clip_near, -clip_near);
		frust->corners[1] = float3(+frust_scale.x * clip_near, -frust_scale.y * clip_near, -clip_near);
		frust->corners[2] = float3(+frust_scale.x * clip_near, +frust_scale.y * clip_near, -clip_near);
		frust->corners[3] = float3(-frust_scale.x * clip_near, +frust_scale.y * clip_near, -clip_near);
		frust->corners[4] = float3(-frust_scale.x * clip_far , -frust_scale.y * clip_far , -clip_far );
		frust->corners[5] = float3(+frust_scale.x * clip_far , -frust_scale.y * clip_far , -clip_far );
		frust->corners[6] = float3(+frust_scale.x * clip_far , +frust_scale.y * clip_far , -clip_far );
		frust->corners[7] = float3(-frust_scale.x * clip_far , +frust_scale.y * clip_far , -clip_far );
	}
	if (frust_size) {
		*frust_size = frust_scale * 2.0f * clip_near;
	}
	if (clip_to_cam) {
		*clip_to_cam = float4x4(
			1.0f/x,      0,      0,       0,
			     0, 1.0f/y,      0,       0,
			     0,      0,      0,      -1,
			     0,      0, 1.0f/b,     a/b
		);
	}
	return float4x4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, b,
		0, 0, -1, 0
	);
}
inline float4x4 orthographic_matrix (float vsize, float aspect, float clip_near=1.0f/32, float clip_far=8192,
	float4x4* clip_to_cam=nullptr, View_Frustrum* frust=nullptr, float2* frust_size=nullptr) {
	float hsize = vsize * aspect;

	float x = 2.0f / hsize;
	float y = 2.0f / vsize;

	float a, b;
	// TODO: why use clip_far here? because I can't divide z while not dividing x and y?
	a = 1.0f / (clip_far - clip_near);
	b = clip_near * a + 1.0f;

	// a = -2.0f / (clip_far - clip_near);
	// b = clip_near * a - 1;

	if (frust) {
		frust->corners[0] = float3(1.0f / -x, 1.0f / -y, -clip_near);
		frust->corners[1] = float3(1.0f / +x, 1.0f / -y, -clip_near);
		frust->corners[2] = float3(1.0f / +x, 1.0f / +y, -clip_near);
		frust->corners[3] = float3(1.0f / -x, 1.0f / +y, -clip_near);
		frust->corners[4] = float3(1.0f / -x, 1.0f / -y, -clip_far );
		frust->corners[5] = float3(1.0f / +x, 1.0f / -y, -clip_far );
		frust->corners[6] = float3(1.0f / +x, 1.0f / +y, -clip_far );
		frust->corners[7] = float3(1.0f / -x, 1.0f / +y, -clip_far );
	}
	if (frust_size) {
		*frust_size = float2(hsize, vsize);
	}
	if (clip_to_cam) {
		*clip_to_cam = float4x4(
			1.0f/x,      0,      0,       0,
			0, 1.0f/y,      0,       0,
			0,      0, 1.0f/a,    -b/a,
			0,      0,      0,       1
		);
	}
	return float4x4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, b,
		0, 0, 0, 1
	);
}

inline void rotate_with_mouselook (Input& I, float vfov, CameraBinds const& binds, float2* ae,
		float azim_min=deg(-180), float azim_max=deg(180), float elev_min=deg(-85), float elev_max=deg(85)) {
	float& azimuth   = ae->x;
	float& elevation = ae->y;

	// Mouselook
	float2 delta = binds.get_mouselook_delta(I, vfov);

	azimuth   -= delta.x;
	elevation += delta.y;

	azimuth = wrap(azimuth, azim_min, azim_max);
	elevation = clamp(elevation, elev_min, elev_max);

	// roll with keys
	float roll_dir = 0;
	if (I.buttons[binds.roll_left ].is_down) roll_dir += 1;
	if (I.buttons[binds.roll_right].is_down) roll_dir -= 1;
}
inline void rotate_with_mouselook (Input& I, float vfov, CameraBinds const& binds, float3* aer,
		float azim_min=deg(-180), float azim_max=deg(180), float elev_min=deg(-85), float elev_max=deg(85)) {
	float& azimuth   = aer->x;
	float& elevation = aer->y;
	float& roll      = aer->z;

	// Mouselook
	float2 delta = binds.get_mouselook_delta(I, vfov);

	azimuth   -= delta.x;
	elevation += delta.y;

	azimuth = wrap(azimuth, azim_min, azim_max);
	elevation = clamp(elevation, elev_min, elev_max);

	// roll with keys
	float roll_dir = 0;
	if (I.buttons[binds.roll_left ].is_down) roll_dir += 1;
	if (I.buttons[binds.roll_right].is_down) roll_dir -= 1;

	float roll_speed = deg(90);

	roll += roll_dir * roll_speed * I.real_dt;
	roll = wrap(roll, deg(-180), deg(180));
}

// Calculate rotation matricies for azimuth, elevation
inline float3x3 calc_ae_rotation (float2 ae, float3x3* out_inverse=nullptr) {
	if (out_inverse)
		*out_inverse = rotate3_Z(+ae.x) * rotate3_X(+ae.y + deg(90));
	return             rotate3_X(-ae.y - deg(90)) * rotate3_Z(-ae.x);
}
// Calculate rotation matricies for azimuth, elevation and roll
inline float3x3 calc_aer_rotation (float3 aer, float3x3* out_inverse=nullptr) {
	if (out_inverse)
		*out_inverse = rotate3_Z(+aer.x) * rotate3_X(+aer.y + deg(90)) * rotate3_Z(-aer.z);
	return             rotate3_Z(+aer.z) * rotate3_X(-aer.y - deg(90)) * rotate3_Z(-aer.x);
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
		if (!ImGui::TreeNode("Camera", name)) return;

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

		ImGui::TreePop();
	}

	// Calculate camera projection matrix
	float4x4 calc_cam_to_clip (int2 viewport_size, float4x4* clip_to_cam=nullptr, View_Frustrum* frust=nullptr, float2* frust_size=nullptr) {
		float aspect = (float)viewport_size.x / (float)viewport_size.y;

		if (mode == PERSPECTIVE) {
			return perspective_matrix(vfov, aspect, clip_near, clip_far, clip_to_cam, frust, frust_size);
		} else {
			assert(mode == ORTHOGRAPHIC);

			return orthographic_matrix(ortho_vsize, aspect, clip_near, clip_far, clip_to_cam, frust, frust_size);
		}
	}
};

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
		if (!ImGui::TreeNode("Flycam", name)) return;

		cam.imgui(name);

		ImGui::DragFloat("base_speed", &base_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("max_speed", &max_speed, 0.05f, 0, FLT_MAX / INT_MAX, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("speedup_factor", &speedup_factor, 0.001f);
		ImGui::DragFloat("fast_multiplier", &fast_multiplier, 0.05f);
		ImGui::Text("cur_speed: %.3f", cur_speed);

		ImGui::TreePop();
	}

	float3x3 calc_world_to_cam_rot (float3x3* cam_to_world_rot) {
		return calc_aer_rotation(cam.rot_aer, cam_to_world_rot);
	}

	Camera_View update (Input& I, int2 const& viewport_size, CameraBinds const& binds, bool lock_controls=false) {
		if (!lock_controls) { //// look
			rotate_with_mouselook(I, cam.vfov, binds, &cam.rot_aer);
		}

		float3x3 cam_to_world_rot;
		float3x3 world_to_cam_rot = calc_aer_rotation(cam.rot_aer, &cam_to_world_rot);

		if (!lock_controls) { //// movement
			float3 move_dir = 0;
			if (I.buttons[KEY_A]           .is_down) move_dir.x -= 1;
			if (I.buttons[KEY_D]           .is_down) move_dir.x += 1;
			if (I.buttons[KEY_W]           .is_down) move_dir.z -= 1;
			if (I.buttons[KEY_S]           .is_down) move_dir.z += 1;
			if (I.buttons[KEY_LEFT_CONTROL].is_down) move_dir.y -= 1;
			if (I.buttons[KEY_SPACE]       .is_down) move_dir.y += 1;

			move_dir = normalizesafe(move_dir);
			float move_speed = length(move_dir); // could be analog with gamepad

			if (move_speed == 0.0f)
				cur_speed = base_speed; // no movement ticks down speed

			if (I.buttons[KEY_LEFT_SHIFT].is_down) {
				move_speed *= fast_multiplier;

				cur_speed += base_speed * speedup_factor * I.real_dt;
			}

			cur_speed = clamp(cur_speed, base_speed, max_speed);

			float3 translation_cam_space = cur_speed * move_dir * I.real_dt;

			cam.pos += cam_to_world_rot * translation_cam_space;
		}

		if (!lock_controls) { //// fov change
			if (!I.buttons[KEY_F].is_down) {
				float delta_log = 0.1f * I.mouse_wheel_delta;
				base_speed = powf(2, log2f(base_speed) +delta_log );
			} else {
				float delta_log = -0.1f * I.mouse_wheel_delta;
				cam.vfov = clamp(powf(2, log2f(cam.vfov) +delta_log ), deg(1.0f/10), deg(170));
			}
		}

		//// matrix calc
		Camera_View v;
		v.world_to_cam = world_to_cam_rot * translate(-cam.pos);
		v.cam_to_world = translate(cam.pos) * cam_to_world_rot;
		v.cam_to_clip = cam.calc_cam_to_clip(viewport_size, &v.clip_to_cam, &v.frustrum, &v.frustrum_size);
		v.clip_near = cam.clip_near;
		v.clip_far = cam.clip_far;
		v.calc_frustrum();
		return v;
	}
};
