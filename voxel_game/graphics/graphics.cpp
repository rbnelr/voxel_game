#include "graphics.hpp"

SkyboxGraphics::SkyboxGraphics () {
	static constexpr float3 LLL = float3(-1,-1,-1);
	static constexpr float3 HLL = float3(+1,-1,-1);
	static constexpr float3 LHL = float3(-1,+1,-1);
	static constexpr float3 HHL = float3(+1,+1,-1);
	static constexpr float3 LLH = float3(-1,-1,+1);
	static constexpr float3 HLH = float3(+1,-1,+1);
	static constexpr float3 LHH = float3(-1,+1,+1);
	static constexpr float3 HHH = float3(+1,+1,+1);

	//#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
	#define QUAD(a,b,c,d) a,d,b, b,d,c // facing inward

	static constexpr float3 arr[6*6] = {
		QUAD(	LHL,
				LLL,
				LLH,
				LHH ),

		QUAD(	HLL,
				HHL,
				HHH,
				HLH ),

		QUAD(	LLL,
				HLL,
				HLH,
				LLH ),

		QUAD(	HHL,
				LHL,
				LHH,
				HHH ),

		QUAD(	HLL,
				LLL,
				LHL,
				HHL ),

		QUAD(	LLH,
				HLH,
				HHH,
				LHH )
	};

	glBindBuffer(GL_ARRAY_BUFFER, skybox);
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

		glBindBuffer(GL_ARRAY_BUFFER, skybox);
		bind_attrib_arrays(Vertex::layout, shader);

		glDrawArrays(GL_TRIANGLES, 0, 6*6);

		glDepthRange(0, 1);
		glDisable(GL_DEPTH_CLAMP);
	}
}
