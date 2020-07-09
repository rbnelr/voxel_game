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

	// process parent node and it's children
	void _recurse (WorldOctree& oct, int3 pos, int scale, int3 insert_pos, int insert_scale, uint32_t node_data_ptr) {

		uint32_t children_i = (uint32_t)oct.trunk.nodes.size();
		oct.trunk.nodes.emplace_back();

		if (node_data_ptr != (uint32_t)-1)
			*(&oct.trunk.nodes[0].data[0] + node_data_ptr) = { true, children_i };

		int child_scale = scale - 1;

		for (int i=0; i<8; ++i) {
			int3 child_pos = pos | (int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) << child_scale);

			auto* child_node_data = &oct.trunk.nodes[children_i].data[i];

			bool is_leaf = any(child_pos != 0) || child_scale == insert_scale;
			if (is_leaf) {
				*child_node_data = { false, all(child_pos == 0) ? 1u : 0u };
			} else {
				_recurse(oct, child_pos, child_scale, insert_pos, insert_scale, (uint32_t)(child_node_data - &oct.trunk.nodes[0].data[0]));
			}
		}
	}

	void insert_node (WorldOctree& oct, int3 pos, int scale) {
		if (any(pos < 0))
			return;
		
		int3 parent_pos = 0;
		int parent_scale = tree_scale;
		
		_recurse(oct, parent_pos, parent_scale, pos, scale, (uint32_t)-1);
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
			cols.push_back( grad.calc(1.0f - t) );
		}

		return cols;
	} (tree_scale + 1);

	void recurse_draw (WorldOctree& oct, OctreeNode node, int3 pos, int scale) {
		float size = (float)(1 << scale);

		auto col = colors[scale];
		if (!node.has_children && node.data == 0)
			col *= lrgba(.5f,.5f,.5f, .6f);
		debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.995f, col,
			!node.has_children && node.data == 0 ? GM_STRIPED : GM_FILL);

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
			insert_node(*this, 0, CHUNK_DIM_SHIFT);
		}

		if (debug_draw_octree) {
			debug_draw(*this);
		}
	}

	void WorldOctree::add_chunk (Chunk& chunk) {
		//int3 pos = chunk.coord * CHUNK_DIM;
		//
		//insert_node(*this, pos, CHUNK_DIM_SHIFT+1);
	}

	void WorldOctree::remove_chunk (Chunk& chunk) {

	}


	void WorldOctree::add_block (Chunk& chunk, int3 block) {

	}

	void WorldOctree::remove_block (Chunk& chunk, int3 block) {
	
	}
}
