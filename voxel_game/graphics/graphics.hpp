#pragma once
#include "../kissmath.hpp"
#include "glshader.hpp"
#include "globjects.hpp"
#include "debug_graphics.hpp"

struct SkyboxGraphics {

	struct Vertex {
		float3 world_dir;

		static constexpr Vertex_Layout<1> layout = {
			Attribute{ "world_dir", gl::T_V3, 3*4, 0 },
		};
	};
	static_assert(sizeof(Vertex) == 3*4);

	Shader shader = Shader("skybox");

	gl::Vbo skybox; // a inward facing cube of size 1

	SkyboxGraphics ();

	void draw (Camera_View& view);
};

//struct ChunkMesh {
//
//	std::vecot
//
//	Shader shader = Shader("skybox");
//
//	void draw (Camera_View& view);
//};
//struct ChunkGraphics {
//
//	Shader shader = Shader("skybox");
//
//	void draw (Camera_View& view);
//};
