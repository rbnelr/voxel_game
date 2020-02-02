#pragma once
#include "../kissmath.hpp"
#include "glshader.hpp"
#include "debug_graphics.hpp"
#include "../blocks.hpp"
#include "gl.hpp"

// rotate from facing up to facing in a block face direction
static inline constexpr float3x3 face_rotation[] = {
	// BF_NEG_X  = rotate3_Z(-deg(90)) * rotate3_X(deg(90)),
	float3x3( 0, 0,-1,
	         -1, 0, 0,
			  0, 1, 0),
	// BF_POS_X  = rotate3_Z(+deg(90)) * rotate3_X(deg(90)),
	float3x3( 0, 0, 1,
			  1, 0, 0,
			  0, 1, 0),
	// BF_NEG_Y  = rotate3_Z(-deg(0)) * rotate3_X(deg(90)),
	float3x3( 1, 0, 0,
		      0, 0,-1,
		      0, 1, 0),
	// BF_POS_Y  = rotate3_Z(+deg(180)) * rotate3_X(deg(90)),
	float3x3(-1, 0, 0,
	          0, 0, 1,
	          0, 1, 0),
	// BF_NEG_Z  = rotate3_X(deg(180)),
	float3x3( 1, 0, 0,
	          0,-1, 0,
	          0, 0,-1),
	// BF_POS_Z  = float3x3::identity()
	float3x3( 1, 0, 0,
		      0, 1, 0,
		      0, 0, 1),
};

struct SkyboxGraphics {

	struct Vertex {
		float3 world_dir;

		static void bind (Attributes& a) {
			a.add<decltype(world_dir)>(0, "world_dir", sizeof(Vertex), offsetof(Vertex, world_dir));
		}
	};

	Shader shader = { "skybox" };

	Mesh<Vertex> mesh; // a inward facing cube of size 1

	SkyboxGraphics ();

	void draw ();
};

struct BlockHighlightGraphics {

	struct Vertex {
		float3	pos_model;
		lrgba	color;

		static void bind (Attributes& a) {
			a.add<decltype(pos_model)>(0, "pos_model", sizeof(Vertex), offsetof(Vertex, pos_model));
			a.add<decltype(color    )>(1, "color"    , sizeof(Vertex), offsetof(Vertex, color    ));
		}
	};

	Shader shader = { "block_highlight" };

	Mesh<Vertex> mesh;

	BlockHighlightGraphics ();

	void draw (float3 pos, BlockFace face);
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
