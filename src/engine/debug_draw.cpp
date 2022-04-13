#include "common.hpp"
#include "debug_draw.hpp"
#include "camera.hpp"

void DebugDraw::vector (float3 const& pos, float3 const& dir, lrgba const& col) {
	auto* out = push_back(lines, 2);

	*out++ = { pos, col };
	*out++ = { pos + dir, col };
}

void DebugDraw::point (float3 const& pos, float3 const& size, lrgba const& col) {
	auto* out = push_back(lines, 8);

	*out++ = { pos + size * float3(-1,-1,-1), col };
	*out++ = { pos + size * float3(+1,+1,+1), col };

	*out++ = { pos + size * float3(+1,-1,-1), col };
	*out++ = { pos + size * float3(-1,+1,+1), col };

	*out++ = { pos + size * float3(-1,+1,-1), col };
	*out++ = { pos + size * float3(+1,-1,+1), col };

	*out++ = { pos + size * float3(+1,+1,-1), col };
	*out++ = { pos + size * float3(-1,-1,+1), col };
}

void DebugDraw::wire_cube (float3 const& pos, float3 const& size, lrgba const& col) {
#if 0
	size_t idx = lines.size();
	lines.resize(idx + ARRLEN(_wire_cube_indices));
	auto* out = &lines[idx];

	for (auto& idx : _wire_cube_indices) {
		auto& v = _wire_cube_vertices[idx];

		out->pos.x = v.x * size.x + pos.x;
		out->pos.y = v.y * size.y + pos.y;
		out->pos.z = v.z * size.z + pos.z;

		out->col = col;

		out++;
	}
#else
	wire_cubes.emplace_back();
	auto& v = wire_cubes.back();

	v.pos = float4(pos, 0);
	v.size = float4(size, 0);
	v.col = col;
#endif
}

void DebugDraw::wire_sphere (float3 const& pos, float r, lrgba const& col, int angres, int wires) {
	int wiresz = wires/2 -1; // one less wire, so we get even vertical wires and odd number of horiz wires, so that there is a 'middle' horiz wire
	int wiresxy = wires;

	int count = (wiresz + wiresxy) * angres * 2; // every wire is <angres> lines, <wires> * 2 because horiz and vert wires

	auto* out = push_back(lines, count);

	float ang_step = deg(360) / (float)angres;

	for (int i=0; i<count; ++i)
		out[i].col = col;

	auto set = [&] (float3 p) {
		out->pos = p * r + pos;
		out++;
	};

	for (int j=0; j<wiresz; ++j) {
		float a = (((float)j + 1) / (float)(wires/2) - 0.5f) * deg(180); // j +1 / (wires/2) gives us better placement of wires

		float sa = sin(a);
		float ca = cos(a);

		float sb0=0, cb0=1; // optimize not calling sin&cos 2x per loop
		for (int i=0; i<angres; ++i) {
			float b1 = (float)(i+1) * ang_step;

			float sb1 = sin(b1);
			float cb1 = cos(b1);

			set(float3(cb0 * ca, sb0 * ca, sa));
			set(float3(cb1 * ca, sb1 * ca, sa));

			sb0 = sb1;
			cb0 = cb1;
		}
	}

	for (int j=0; j<wiresxy; ++j) {
		float a = (float)j / (float)wiresxy * deg(360);
	
		float sa = sin(a);
		float ca = cos(a);

		float sb0=0, cb0=1; // optimize not calling sin&cos 2x per loop
		for (int i=0; i<angres; ++i) {
			float b1 = (float)(i+1) * ang_step;

			float sb1 = sin(b1);
			float cb1 = cos(b1);

			set(float3(cb0 * ca, cb0 * sa, sb0));
			set(float3(cb1 * ca, cb1 * sa, sb1));

			sb0 = sb1;
			cb0 = cb1;
		}
	}
}

void DebugDraw::wire_cone (float3 const& pos, float ang, float length, float3x3 const& rot, lrgba const& col, int circres, int wires) {
	int count = (circres + wires) * 2;

	auto* out = push_back(lines, count);

	float r = tan(ang * 0.5f);

	for (int i=0; i<count; ++i)
		out[i].col = col;

	auto set = [&] (float3 p) {
		out->pos = pos + (rot * p) * length;
		out++;
	};

	// circle of cone base
	float s0 = 0;
	float c0 = 1;
	for (int i=0; i<circres; ++i) {
		float ang = (float)(i + 1) * deg(360) / (float)circres;

		float s1 = sin(ang);
		float c1 = cos(ang);

		set(float3(c0*r, s0*r, 1));
		set(float3(c1*r, s1*r, 1));

		s0 = s1;
		c0 = c1;
	}

	// lines from tip to base
	for (int i=0; i<wires; ++i) {
		float ang = (float)(i + 1) * deg(360) / (float)wires;

		set(float3(0, 0, 0));
		set(float3(cos(ang)*r, sin(ang)*r, 1));
	}
}


void DebugDraw::wire_frustrum (Camera_View const& view, lrgba const& col) {
	static constexpr int _frustrum_corners[12 * 2] {
		0,1,  1,2,  2,3,  3,0, // bottom lines
		0,4,  1,5,  2,6,  3,7, // vertical lines
		4,5,  5,6,  6,7,  7,4, // top lines
	};

	auto* out = push_back(lines, ARRLEN(_frustrum_corners));

	for (int corner : _frustrum_corners) {
		out->pos = view.frustrum.corners[corner];
		out->col = col;
		out++;
	}
}

void DebugDraw::quad (float3 const& center, float2 size, lrgba const& col) {
	auto* out = push_back(tris, 6);
	
	*out++ = { center + float3(+0.5f,-0.5f, 0.0f), float3(0,0,1), col };
	*out++ = { center + float3(+0.5f,+0.5f, 0.0f), float3(0,0,1), col };
	*out++ = { center + float3(-0.5f,-0.5f, 0.0f), float3(0,0,1), col };
	*out++ = { center + float3(-0.5f,-0.5f, 0.0f), float3(0,0,1), col };
	*out++ = { center + float3(+0.5f,+0.5f, 0.0f), float3(0,0,1), col };
	*out++ = { center + float3(-0.5f,+0.5f, 0.0f), float3(0,0,1), col };
}

void DebugDraw::cylinder (float3 const& base, float radius, float height, lrgba const& col, int sides) {
	auto* out = push_back(tris, sides * 4 * 3); // tri for bottom + top cap + 2 tris for side

	float ang_step = 2*PI / (float)sides;

	float sin0=0, cos0=1; // optimize not calling sin 2x per loop

	auto push = [&] (float3 pos, float3 normal) {
		out->pos = pos * float3(radius, radius, height) + base;
		out->normal = normal;
		out->col = col;
		out++;
	};

	for (int i=0; i<sides; ++i) {
		float ang1 = (float)(i+1) * ang_step;

		float sin1 = sin(ang1);
		float cos1 = cos(ang1);

		push(float3(   0,    0, 0), float3(0, 0, -1));
		push(float3(cos1, sin1, 0), float3(0, 0, -1));
		push(float3(cos0, sin0, 0), float3(0, 0, -1));

		push(float3(cos1, sin1, 0), float3(cos1, sin1, 0));
		push(float3(cos1, sin1, 1), float3(cos1, sin1, 0));
		push(float3(cos0, sin0, 0), float3(cos0, sin0, 0));
		push(float3(cos0, sin0, 0), float3(cos0, sin0, 0));
		push(float3(cos1, sin1, 1), float3(cos1, sin1, 0));
		push(float3(cos0, sin0, 1), float3(cos0, sin0, 0));

		push(float3(   0,    0, 1), float3(0, 0, +1));
		push(float3(cos0, sin0, 1), float3(0, 0, +1));
		push(float3(cos1, sin1, 1), float3(0, 0, +1));

		sin0 = sin1;
		cos0 = cos1;
	}
}

void DebugDraw::axis_gizmo (Camera_View const& view, int2 const& viewport_size) {
	//float2 pos_px = float2(viewport_size.x - 100.5f, 100.5f);
	//float2 pos_clip = pos_px / (float2)viewport_size * 2.0f - 1.0f;
	float2 pos_clip = float2(+0.9f, -0.9f);

	float3 pos_cam = (float3)(view.clip_to_cam * float4(pos_clip, 0,1));
	float3 pos_world = (float3)(view.cam_to_world * float4(pos_cam,1));

	float size_clip = 0.05f;
	float3 pos_cam2 = (float3)(view.clip_to_cam * float4(pos_clip.x, pos_clip.y + size_clip, 0,1)).y; // size in cam = size in world
	float size_world = pos_cam2.y - pos_cam.y;

	vector(pos_world, float3(size_world,0,0), lrgba(1,0,0,1));
	vector(pos_world, float3(0,size_world,0), lrgba(0,1,0,1));
	vector(pos_world, float3(0,0,size_world), lrgba(0,0,1,1));
}

void DebugDraw::prepare_selectables (Camera_View const& view, Input& I, bool enable) {
	enable_selectables = enable;
	
	cursor_pos = I.cursor_pos;
	lmb_went_down = I.buttons[MOUSE_BUTTON_LEFT].went_down;
	lmb_is_down   = I.buttons[MOUSE_BUTTON_LEFT].is_down;

	window_size = (float2)I.window_size;
	this->view = view;

	if (!enable) grabbed = false;
}
void DebugDraw::finish_selectables () {
	if (lmb_went_down)
		selected_id = nullptr; // we clicked but nothing was selected
}

bool DebugDraw::selectable (char const* id, float3 const& pos, float size, lrgba const& col) {
	if (!enable_selectables) return false;

	float2 pos_screen;
	project_world2screen(pos, view, window_size, &pos_screen);

	bool hovered = length(pos_screen - cursor_pos) < 50;
	bool selected = id == selected_id;

	if (!selected) {
		if (hovered && lmb_went_down) {
			selected_id = id;
			selected = true;
			lmb_went_down = false; // first hovered item consumes mouse click
		}
	}

	if (selected) {
		auto& s = ImGui::GetStyle().MouseCursorScale;
		ImGui::SetNextWindowPos(ImVec2(pos_screen.x + 16*s, pos_screen.y + 8*s));
		ImGui::Begin("##DebugDraw selected toolip", NULL, ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs);
		ImGui::Text(id);
		ImGui::End();
	} else if (hovered) {
		ImGui::BeginTooltip();
		ImGui::Text(id);
		ImGui::EndTooltip();
	}

	//lrgba c = col;
	//
	//if (hovered) c = lrgba(0,0,0,1);
	//if (selected) c = lrgba(1,0,0,1);
	//
	//c.w *= 0.5f;
	//
	//point(pos, size, c);

	return selected;
}

bool DebugDraw::gizmo (float3* pos) {

	float2 base_pos = NAN;
	project_world2screen(*pos, view, window_size, &base_pos);

	auto axis_cursor_dist = [&] (float3 const& dir) {
		float2 axis_pos = NAN;
		project_world2screen(*pos + dir, view, window_size, &axis_pos);

		return point_line_segment_dist(base_pos, axis_pos - base_pos, cursor_pos);
	};
	auto axis_cursor_intersect = [&] (float3 axis_pos, int axis, float3* intersect) {
		assert(axis >= 0 && axis <= 2);

		auto ray = screen_ray(cursor_pos, view, window_size);

		float3 axis_dir = 0;
		axis_dir[axis] = 1;

		return ray_line_closest_intersect(ray.pos, ray.dir, axis_pos, axis_dir, intersect);
	};

	if (lmb_went_down)
		printf("blah");

	if (!lmb_is_down)
		grabbed = false;

	if (!grabbed) {
		float3 dists;
		dists.x = axis_cursor_dist(float3(1,0,0));
		dists.y = axis_cursor_dist(float3(0,1,0));
		dists.z = axis_cursor_dist(float3(0,0,1));

		int axis;
		float dist = min_component(dists, &axis);

		gizmo_axis = -1;
		if (dist < 20) gizmo_axis = axis;

		if (gizmo_axis >= 0 && lmb_went_down) {
			lmb_went_down = false;

			grabbed_pos = *pos;
			float3 intersect;
			grabbed = axis_cursor_intersect(*pos, gizmo_axis, &intersect);

			grabbed_offset = intersect - grabbed_pos;
		}
	} else {
		float3 intersect;
		if (axis_cursor_intersect(grabbed_pos, gizmo_axis, &intersect))
			*pos = intersect - grabbed_offset;
	}

	vector(*pos, float3(1,0,0), gizmo_axis == 0 ? lrgba(1,0,0,1) : lrgba(0.6f, 0.0f, 0.0f, 1));
	vector(*pos, float3(0,1,0), gizmo_axis == 1 ? lrgba(0,1,0,1) : lrgba(0.0f, 0.6f, 0.0f, 1));
	vector(*pos, float3(0,0,1), gizmo_axis == 2 ? lrgba(0,0,1,1) : lrgba(0.0f, 0.0f, 0.6f, 1));

	return grabbed;
}
