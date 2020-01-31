#include "graphics.hpp"

#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
#define QUAD_INWARD(a,b,c,d) a,d,b, b,d,c // facing inward

SkyboxGraphics::SkyboxGraphics () {
	static constexpr float3 LLL = float3(-1,-1,-1);
	static constexpr float3 HLL = float3(+1,-1,-1);
	static constexpr float3 LHL = float3(-1,+1,-1);
	static constexpr float3 HHL = float3(+1,+1,-1);
	static constexpr float3 LLH = float3(-1,-1,+1);
	static constexpr float3 HLH = float3(+1,-1,+1);
	static constexpr float3 LHH = float3(-1,+1,+1);
	static constexpr float3 HHH = float3(+1,+1,+1);

	static constexpr float3 arr[6*6] = {
		QUAD_INWARD(	LHL,
						LLL,
						LLH,
						LHH ),

		QUAD_INWARD(	HLL,
						HHL,
						HHH,
						HLH ),

		QUAD_INWARD(	LLL,
						HLL,
						HLH,
						LLH ),

		QUAD_INWARD(	HHL,
						LHL,
						LHH,
						HHH ),

		QUAD_INWARD(	HLL,
						LLL,
						LHL,
						HHL ),

		QUAD_INWARD(	LLH,
						HLH,
						HHH,
						LHH )
	};

	glBindBuffer(GL_ARRAY_BUFFER, mesh);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arr), arr, GL_STATIC_DRAW);
}

void SkyboxGraphics::draw (Camera_View& view) {
	if (shader) {
		glUseProgram(shader.shader->shad);

		shader.set_uniform("world_to_cam", (float4x4)view.world_to_cam);
		shader.set_uniform("cam_to_world", (float4x4)view.cam_to_world);
		shader.set_uniform("cam_to_clip", view.cam_to_clip);

		glEnable(GL_DEPTH_CLAMP); // prevent skybox clipping with near plane
		glDepthRange(1, 1); // Draw skybox behind everything, even though it's actually a box of size 1 placed on the camera

		glBindBuffer(GL_ARRAY_BUFFER, mesh);
		bind_attrib_arrays(Vertex::layout, shader);

		glDrawArrays(GL_TRIANGLES, 0, 6*6);

		glDepthRange(0, 1);
		glDisable(GL_DEPTH_CLAMP);
	}
}

BlockHighlightGraphics::BlockHighlightGraphics () {

	lrgba col = srgb(40,40,40,240);
	lrgba dot_col = srgb(40,40,40,240);

	float r = 0.504f;
	float inset = 1.0f / 100;

	float side_r = r * 0.04f;
	
	std::vector<Vertex> vertices;

	for (int face=0; face<6; ++face) {

		auto vert = [&] (float3 v, lrgba col) {
			vertices.push_back(Vertex{ face_rotation[face] * v, col });
		};
		auto quad = [&] (float3 a, float3 b, float3 c, float3 d, lrgba col) {
			vert(b, col);	vert(c, col);	vert(a, col);
			vert(a, col);	vert(c, col);	vert(d, col);
		};

		quad(	float3(-r,-r,+r),
				float3(+r,-r,+r),
				float3(+r,-r,+r) + float3(-inset,+inset,0),
				float3(-r,-r,+r) + float3(+inset,+inset,0),
				col);

		quad(	float3(+r,-r,+r),
				float3(+r,+r,+r),
				float3(+r,+r,+r) + float3(-inset,-inset,0),
				float3(+r,-r,+r) + float3(-inset,+inset,0),
				col);

		quad(	float3(+r,+r,+r),
				float3(-r,+r,+r),
				float3(-r,+r,+r) + float3(+inset,-inset,0),
				float3(+r,+r,+r) + float3(-inset,-inset,0),
				col);

		quad(	float3(-r,+r,+r),
				float3(-r,-r,+r),
				float3(-r,-r,+r) + float3(+inset,+inset,0),
				float3(-r,+r,+r) + float3(+inset,-inset,0),
				col);

		if (face == BF_POS_Z) {
			// face highlight
			quad(	float3(-side_r,-side_r,+r),
					float3(+side_r,-side_r,+r),
					float3(+side_r,+side_r,+r),
					float3(-side_r,+side_r,+r),
					dot_col );
		}
	}

	vertices_count = vertices.size();

	glBindBuffer(GL_ARRAY_BUFFER, mesh);
	glBufferData(GL_ARRAY_BUFFER, vertices_count * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);
}

void BlockHighlightGraphics::draw (Camera_View& view, float3 pos, BlockFace face) {
	if (shader) {
		glUseProgram(shader.shader->shad);

		shader.set_uniform("world_to_cam", (float4x4)view.world_to_cam);
		shader.set_uniform("cam_to_world", (float4x4)view.cam_to_world);
		shader.set_uniform("cam_to_clip", view.cam_to_clip);

		shader.set_uniform("block_pos", pos);
		shader.set_uniform("face_rotation", face_rotation[face]);

		ImGui::Text("face: %d", face);

		glEnable(GL_BLEND);

		glBindBuffer(GL_ARRAY_BUFFER, mesh);
		bind_attrib_arrays(Vertex::layout, shader);

		glDrawArrays(GL_TRIANGLES, 0, vertices_count);

		glDisable(GL_BLEND);
	}
}
