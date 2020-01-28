#pragma once
#include "kissmath.hpp"
#include "gl.hpp"
#include "game.hpp"
#include <vector>

class DebugDraw {
public:

	struct Vertex {
		float3	pos_world;
		lrgba	color;
	};
	static constexpr std::array<Vertex_Attribute, 2> vertex_layout = {
		Vertex_Attribute{ "pos_world",	T_V3,	sizeof(Vertex), offsetof(Vertex, pos_world) },
		Vertex_Attribute{ "color",		T_V4,	sizeof(Vertex), offsetof(Vertex, color) },
	};

	//std::vector<Vertex> mesh_faces;
	//std::vector<Vertex> mesh_lines;

	Vbo_old vbo_faces	= Vbo_old(vertex_layout);
	Vbo_old vbo_lines	= Vbo_old(vertex_layout);

	Shader* shad;

	// draw quad
	
	void push_wire_cube (float3 center, float size, lrgba col) {
		//vector_append(

		//vertices.push_back
	}

	// draw arrow

	void push_cylinder (float3 center, float radius, float height, lrgba col, int sides) {
		//mesh_faces.resize(mesh_faces.size() + 12 * sides);
		//
		//float2 rv = float2(radius, 0);
		//float h = height;
		//
		//float2x2 prev_rot = float2x2::identity();
		//
		//for (int i=0; i<sides; ++i) {
		//	float rot_b = (float)(i + 1) / (float)sides * deg(360);
		//
		//	float2x2 ma = prev_rot;
		//	float2x2 mb = rotate2(rot_b);
		//
		//	prev_rot = mb;
		//
		//	mesh_faces[i*12 +  0] = { center +float3(0,0,     +h/2), col };
		//	mesh_faces[i*12 +  1] = { center +float3(ma * rv, +h/2), col };
		//	mesh_faces[i*12 +  2] = { center +float3(mb * rv, +h/2), col };
		//
		//	mesh_faces[i*12 +  3] = { center +float3(mb * rv, -h/2), col };
		//	mesh_faces[i*12 +  4] = { center +float3(mb * rv, +h/2), col };
		//	mesh_faces[i*12 +  5] = { center +float3(ma * rv, -h/2), col };
		//	mesh_faces[i*12 +  6] = { center +float3(ma * rv, -h/2), col };
		//	mesh_faces[i*12 +  7] = { center +float3(mb * rv, +h/2), col };
		//	mesh_faces[i*12 +  8] = { center +float3(ma * rv, +h/2), col };
		//
		//	mesh_faces[i*12 +  9] = { center +float3(0,0,     -h/2), col };
		//	mesh_faces[i*12 + 10] = { center +float3(mb * rv, -h/2), col };
		//	mesh_faces[i*12 + 11] = { center +float3(ma * rv, -h/2), col };
		//}

		auto* out = (Vertex*)vector_append(&vbo_faces.vertecies, sizeof(Vertex) * 9 * sides);
		
		float2 rv = float2(radius, 0);
		float h = height;
		
		float2x2 prev_rot = float2x2::identity();
		
		for (int i=0; i<sides; ++i) {
			float rot_a = (float)(i + 0) / (float)sides * deg(360);
			float rot_b = (float)(i + 1) / (float)sides * deg(360);
		
			float2x2 ma = rotate2(rot_a);
			float2x2 mb = rotate2(rot_b);
		
			//prev_rot = mb;
		
			*out++ = { center +float3(0,0,     +h/2), col };
			*out++ = { center +float3(ma * rv, +h/2), col };
			*out++ = { center +float3(mb * rv, +h/2), col };
		
			*out++ = { center +float3(mb * rv, -h/2), col };
			*out++ = { center +float3(mb * rv, +h/2), col };
			*out++ = { center +float3(ma * rv, -h/2), col };
			*out++ = { center +float3(ma * rv, -h/2), col };
			*out++ = { center +float3(mb * rv, +h/2), col };
			*out++ = { center +float3(ma * rv, +h/2), col };
		
			//*out++ = { center +float3(0,0,     -h/2), col };
			//*out++ = { center +float3(mb * rv, -h/2), col };
			//*out++ = { center +float3(ma * rv, -h/2), col };
		}
	}

	void draw (Camera_View& view) {
		if (!shad) {
			shad = new_shader("overlay.vert", "overlay.frag", { UCOM, UMAT });
			vbo_faces.init();
			vbo_lines.init();
		}

		if (shad->valid()) {
			shad->bind();

			shad->set_unif("world_to_cam",	(float4x4)view.world_to_cam);
			shad->set_unif("cam_to_world",	(float4x4)view.cam_to_world);
			shad->set_unif("cam_to_clip",	view.cam_to_clip);

			{ // triangles
				glDisable(GL_CULL_FACE);
				glEnable(GL_BLEND);
				//glDisable(GL_DEPTH_TEST);

				vbo_faces.upload();
				vbo_faces.draw_entire(shad);

				//glEnable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
				glEnable(GL_CULL_FACE);
			}

			{ // lines
				glEnable(GL_BLEND);
				//glDisable(GL_DEPTH_TEST);

				vbo_lines.upload();
				vbo_lines.draw_entire(shad);

				//glEnable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
			}
		}

		vbo_faces.clear();
		vbo_lines.clear();
	}
};

// global DebugDraw
extern DebugDraw debug_draw;
