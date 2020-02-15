#include "world.hpp"

SelectedBlock World::raycast_solid_blocks (Ray ray, float max_dist, float* hit_dist) {
	SelectedBlock b;
	float _dist;
	auto hit_block = [&] (bpos bp, int face, float dist) {
		Chunk* chunk;
		b.block = chunks.query_block(bp, &chunk);
		if (chunk && b.block && BLOCK_PROPS[b.block->id].collision == CM_SOLID) {
			//hit.pos_world = ray.pos + ray.dir * dist;
			b.pos = bp;
			b.face = (BlockFace)face;
			_dist = dist;
			return true;
		}
		return false;
	};

	if (!raycast_voxels(ray, max_dist, hit_block)) {
		return { nullptr };
	}

	if (hit_dist) *hit_dist = _dist;
	return b;
}

void World::apply_damage (SelectedBlock const& block, Item& item) {
	assert(block);
	auto tool_props = item.get_props();

	Chunk* chunk;
	Block* b = chunks.query_block(block.pos, &chunk);
	auto bprops = BLOCK_PROPS[b->id];

	assert(chunk && BLOCK_PROPS[b->id].collision == CM_SOLID);

	float damage_multiplier = (float)tool_props.hardness / (float)bprops.hardness;
	if (tool_props.tool == bprops.tool)
		damage_multiplier *= TOOL_MATCH_BONUS_DAMAGE;

	b->hp -= min((uint8)ceili(tool_props.damage * damage_multiplier), b->hp);

	if (b->hp > 0) {
		chunk->block_only_texture_changed(block.pos);
	} else {

		b->hp = 0;
		b->id = B_AIR;

		chunk->block_changed(chunks, block.pos);

		break_sound.play();
	}
}

bool World::try_place_block (bpos pos, block_id bt) {
	Chunk* chunk;
	Block* b = chunks.query_block(pos, &chunk);

	if (chunk && b && BLOCK_PROPS[b->id].collision != CM_SOLID) { // can replace liquid and gas blocks
		b->id = bt;
		b->hp = 255;

		chunk->block_changed(chunks, pos);
		return true;
	}
	return false;
}
