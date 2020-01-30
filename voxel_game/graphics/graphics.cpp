#include "graphics.hpp"

void SkyboxGraphics::draw (Camera_View& view) {
	if (shader) {
		glUseProgram(shader.shader->shad);

		shader.set_uniform("world_to_cam", (float4x4)view.world_to_cam);
		shader.set_uniform("cam_to_world", (float4x4)view.cam_to_world);
		shader.set_uniform("cam_to_clip", view.cam_to_clip);

		glEnable(GL_DEPTH_CLAMP); // prevent skybox clipping with near plane
		glDepthRange(1, 1); // Draw skybox behind everything, even though it's actually a box of size 1 placed on the camera

		glDrawArrays(GL_TRIANGLES, 0, 6*6);

		glDepthRange(0, 1);
		glDisable(GL_DEPTH_CLAMP);
	}
}
