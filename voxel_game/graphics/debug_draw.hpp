#pragma once
#include "../kissmath.hpp"
#include "util.hpp"
#include <optional>
#include <array>

class DebugDraw {
public:

	struct Vertex {
		float3	pos;
		lrgba	color;
	};
	static constexpr std::array<Attribute, 2> layout = {
		Attribute{ "pos",   gl::T_V3, sizeof(Vertex), offsetof(Vertex, pos) },
		Attribute{ "color", gl::T_V4, sizeof(Vertex), offsetof(Vertex, color) },
	};

	std::shared_ptr<Shader> shader;
	Uniform world_to_cam, cam_to_world, cam_to_clip;

	MeshStreamDrawer<Vertex> faces, lines;

	void init () {
		shader = shader_manager.load_shader("overlay");
		
		if (shader) {
			world_to_cam = shader->get_uniform("world_to_cam", gl::T_M4);
			cam_to_world = shader->get_uniform("cam_to_world", gl::T_M4);
			cam_to_clip  = shader->get_uniform("cam_to_clip", gl::T_M4);
		
			faces.init(shader, layout);
			lines.init(shader, layout);
		}
	}

	// draw quad
	
	void push_wire_cube (float3 center, float size, lrgba col) {
		//vector_append(

		//vertices.push_back
	}

	// draw arrow

	void push_cylinder (float3 center, float radius, float height, lrgba col, int sides) {
		faces.mesh.resize(faces.mesh.size() + 12 * sides);
		
		float2 rv = float2(radius, 0);
		float h = height;
		
		float2x2 prev_rot = float2x2::identity();
		
		for (int i=0; i<sides; ++i) {
			float rot_b = (float)(i + 1) / (float)sides * deg(360);
		
			float2x2 ma = prev_rot;
			float2x2 mb = rotate2(rot_b);
		
			prev_rot = mb;
		
			faces.mesh[i*12 +  0] = { center +float3(0,0,     +h/2), col };
			faces.mesh[i*12 +  1] = { center +float3(ma * rv, +h/2), col };
			faces.mesh[i*12 +  2] = { center +float3(mb * rv, +h/2), col };
		
			faces.mesh[i*12 +  3] = { center +float3(mb * rv, -h/2), col };
			faces.mesh[i*12 +  4] = { center +float3(mb * rv, +h/2), col };
			faces.mesh[i*12 +  5] = { center +float3(ma * rv, -h/2), col };
			faces.mesh[i*12 +  6] = { center +float3(ma * rv, -h/2), col };
			faces.mesh[i*12 +  7] = { center +float3(mb * rv, +h/2), col };
			faces.mesh[i*12 +  8] = { center +float3(ma * rv, +h/2), col };
		
			faces.mesh[i*12 +  9] = { center +float3(0,0,     -h/2), col };
			faces.mesh[i*12 + 10] = { center +float3(mb * rv, -h/2), col };
			faces.mesh[i*12 + 11] = { center +float3(ma * rv, -h/2), col };
		}
	}

	void draw (Camera_View& view) {

		if (shader && (faces.mesh.size() || lines.mesh.size())) {
			glUseProgram(shader->shad);

			world_to_cam.set((float4x4)view.world_to_cam);
			cam_to_world.set((float4x4)view.cam_to_world);
			cam_to_clip.set(view.cam_to_clip);

			glEnable(GL_BLEND);
			{ // triangles
				//glDisable(GL_CULL_FACE);
				//glDisable(GL_DEPTH_TEST);

				faces.draw();

				//glEnable(GL_DEPTH_TEST);
				//glEnable(GL_CULL_FACE);
			}

			{ // lines
				//glDisable(GL_DEPTH_TEST);

				lines.draw();

				//glEnable(GL_DEPTH_TEST);
			}
			
			glDisable(GL_BLEND);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glUseProgram(0);
		}
	}
};

// global DebugDraw
extern DebugDraw debug_draw;
