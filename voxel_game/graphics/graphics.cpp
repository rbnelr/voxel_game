#include "graphics.hpp"

//

#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
#define QUAD_INWARD(a,b,c,d) a,d,b, b,d,c // facing inward

SkyboxGraphics::SkyboxGraphics () {
	static constexpr Vertex LLL = { float3(-1,-1,-1) };
	static constexpr Vertex HLL = { float3(+1,-1,-1) };
	static constexpr Vertex LHL = { float3(-1,+1,-1) };
	static constexpr Vertex HHL = { float3(+1,+1,-1) };
	static constexpr Vertex LLH = { float3(-1,-1,+1) };
	static constexpr Vertex HLH = { float3(+1,-1,+1) };
	static constexpr Vertex LHH = { float3(-1,+1,+1) };
	static constexpr Vertex HHH = { float3(+1,+1,+1) };

	static constexpr Vertex arr[6*6] = {
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

	mesh.upload(arr, 6*6);
}

void SkyboxGraphics::draw () {
	if (shader) {
		shader.bind();

		glEnable(GL_DEPTH_CLAMP); // prevent skybox clipping with near plane
		glDepthRange(1, 1); // Draw skybox behind everything, even though it's actually a box of size 1 placed on the camera

		mesh.bind();
		mesh.draw();

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

	mesh.upload(vertices);
}

void BlockHighlightGraphics::draw (float3 pos, BlockFace face) {
	if (shader) {
		shader.bind();

		shader.set_uniform("block_pos", pos);
		shader.set_uniform("face_rotation", face_rotation[face]);

		glEnable(GL_BLEND);

		mesh.bind();
		mesh.draw();

		glDisable(GL_BLEND);
	}
}
