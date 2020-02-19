#include "world.hpp"

SelectedBlock World::raycast_breakable_blocks (Ray ray, float max_dist, float* hit_dist) {
	SelectedBlock b;
	float _dist;
	auto hit_block = [&] (bpos bp, int face, float dist) {
		Chunk* chunk;
		b.block = chunks.query_block(bp, &chunk);
		if (chunk && breakable(b.block.id)) {
			//hit.pos_world = ray.pos + ray.dir * dist;
			b.valid = true;
			b.pos = bp;
			b.face = (BlockFace)face;
			_dist = dist;
			return true;
		}
		return false;
	};

	if (!raycast_voxels(ray, max_dist, hit_block)) {
		return {};
	}

	if (hit_dist) *hit_dist = _dist;
	return b;
}

void World::apply_damage (SelectedBlock const& block, Item& item) {
	assert(block);
	auto tool_props = item.get_props();

	Chunk* chunk;
	bpos bpos_in_chunk;
	Block b = chunks.query_block(block.pos, &chunk, &bpos_in_chunk);
	auto bprops = BLOCK_PROPS[b.id];

	if (!chunk || !breakable(b.id))
		return;

	if (bprops.hardness == 0) {
		b.hp = 0;
	} else {
		float damage_multiplier = (float)tool_props.hardness / (float)bprops.hardness;
		if (tool_props.tool == bprops.tool)
			damage_multiplier *= TOOL_MATCH_BONUS_DAMAGE;

		b.hp -= (uint8)min(ceili(tool_props.damage * damage_multiplier), (int)b.hp);
	}

	if (b.hp <= 0) {
		b = { B_AIR };

		break_sound.play();
	}

	chunk->set_block(chunks, bpos_in_chunk, b);
}

bool World::try_place_block (bpos pos, block_id bt) {
	Chunk* chunk;
	bpos bpos_in_chunk;
	Block b = chunks.query_block(pos, &chunk, &bpos_in_chunk);

	if (chunk && !breakable(b.id)) { // non-breakable blocks are solids and gasses
		chunk->set_block(chunks, bpos_in_chunk, { bt });
		return true;
	}
	return false;
}
