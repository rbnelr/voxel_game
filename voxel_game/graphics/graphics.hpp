#pragma once
#include "../kissmath.hpp"
#include "glshader.hpp"
#include "globjects.hpp"
#include "debug_graphics.hpp"
#include "../blocks.hpp"

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

		static constexpr Vertex_Layout<1> layout = {
			Attribute{ "world_dir", gl::T_V3, 3*4, 0 },
		};
	};
	static_assert(sizeof(Vertex) == 3*4);

	Shader shader = Shader("skybox");

	gl::Vbo mesh; // a inward facing cube of size 1

	SkyboxGraphics ();

	void draw (Camera_View& view);
};

struct BlockHighlightGraphics {

	struct Vertex {
		float3	pos_model;
		lrgba	color;

		static constexpr Vertex_Layout<2> layout = {
			Attribute{ "pos_model", gl::T_V3, 7*4, 0 },
			Attribute{ "color",		gl::T_V4, 7*4, 3*4 }
		};
	};
	static_assert(sizeof(Vertex) == 7*4);

	Shader shader = Shader("block_highlight");

	gl::Vbo mesh; // a inward facing cube of size 1
	int vertices_count;

	BlockHighlightGraphics ();

	void draw (Camera_View& view, float3 pos, BlockFace face);
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