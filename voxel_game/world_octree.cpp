#include "world_octree.hpp"
#include "dear_imgui.hpp"
#include "chunks.hpp"
#include <stack>

namespace world_octree {
	void WorldOctree::imgui () {
		if (!imgui_push("WorldOctree")) return;

		ImGui::Checkbox("debug_draw_octree", &debug_draw_octree);

		imgui_pop();
	}

	struct NodeCoord {
		int3		pos;
		int			scale;
	};

	void insert_node (WorldOctree& oct, NodeCoord insert_node) {
		if (any(insert_node.pos < 0))
			return;

		// start with root node
		int scale = tree_scale;
		uint32_t children_ptr = 0;
		
		OctreeNode* node_data = nullptr;

		for (;;) {
			// get child that contains insert_node
			scale--;
			
			int child_idx = (((insert_node.pos.x >> scale) & 1) << 0) |
							(((insert_node.pos.y >> scale) & 1) << 1) |
							(((insert_node.pos.z >> scale) & 1) << 2);

			// 'recurse' into child node
			node_data = &oct.trunk.nodes[children_ptr].data[child_idx];

			if (scale == insert_node.scale)
				break;

			if (node_data->has_children) {
				children_ptr = node_data->data;
			} else {
				children_ptr = (uint32_t)oct.trunk.nodes.size();

				// write ptr to children into this nodes slot in parents children array
				*node_data = { true, children_ptr };

				// alloc children for node
				oct.trunk.nodes.push_back({0});
			}
		}

		*node_data = { false, 1 };
	}

	std::vector<lrgba> colors = [] (int count) {
		Gradient grad = Gradient({
			{ 0.00f,	srgba(0x010934) },
			{ 0.12f,	srgba(0x0030c8) },
			{ 0.24f,	srgba(0x02f9ff) },
			{ 0.36f,	srgba(0x3eff64) },
			{ 0.47f,	srgba(0x63ff00) },
			{ 0.60f,	srgba(0xfaff27) },
			{ 0.73f,	srgba(0xffbc06) },
			{ 0.86f,	srgba(0xff5715) },
			{ 1.00f,	srgba(0xff00b5) },
		});

		std::vector<lrgba> cols;
		for (int i=0; i<count; ++i) {
			float t = (float)i / (count -1);
			cols.push_back( grad.calc(t) );
		}

		return cols;
	} (tree_scale + 1);

	void recurse_draw (WorldOctree& oct, OctreeNode node, int3 pos, int scale) {
		float size = (float)(1 << scale);

		auto col = colors[scale];
		//if (!node.has_children && node.data == 0)
		//	col *= lrgba(.5f,.5f,.5f, .6f);
		//debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.995f, col);
		if (node.has_children || node.data != 0)
			debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.995f, col);

		if (node.has_children) {
			OctreeChildren children = oct.trunk.nodes[node.data];
			int child_scale = scale - 1;

			for (int i=0; i<8; ++i) {
				int3 child_pos = pos | (int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) << child_scale);

				recurse_draw(oct, children.data[i], child_pos, child_scale);
			}
		}
	}

	void debug_draw (WorldOctree& oct) {
		recurse_draw(oct, { true, 0 }, 0, tree_scale);
	}

	void WorldOctree::update () {
		if (trunk.nodes.size() == 0) {
			trunk.nodes.push_back({});
		}

		if (debug_draw_octree) {
			debug_draw(*this);
		}
	}

	void WorldOctree::add_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;
		
		insert_node(*this, { pos, CHUNK_DIM_SHIFT });
	}

	void WorldOctree::remove_chunk (Chunk& chunk) {

	}


	void WorldOctree::add_block (Chunk& chunk, int3 block) {

	}

	void WorldOctree::remove_block (Chunk& chunk, int3 block) {
	
	}
}
