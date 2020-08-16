#include "stdafx.hpp"
#include "world.hpp"

SelectedBlock World::raycast_breakable_blocks (Ray ray, float max_dist, bool hit_at_max_dist, float* hit_dist) {
	SelectedBlock b;
	float _dist;
	auto hit_block = [&] (bpos bp, int face, float dist, bool force_hit) {
		Chunk* chunk;
		b.block = chunks.query_block(bp, &chunk);
		if (chunk && (blocks.breakable(b.block.id) || force_hit)) {
			//hit.pos_world = ray.pos + ray.dir * dist;
			b.valid = true;
			b.pos = bp;
			b.face = (BlockFace)face;
			_dist = dist;
			return true;
		}
		return false;
	};

	if (!raycast_voxels(ray, max_dist, hit_block, nullptr, hit_at_max_dist)) {
		return {};
	}

	if (hit_dist) *hit_dist = _dist;
	return b;
}

void World::apply_damage (SelectedBlock& block, Item& item, bool creative_mode) {
	assert(block);
	auto tool_props = item.get_props();

	Chunk* chunk;
	bpos bpos_in_chunk;
	Block b = chunks.query_block(block.pos, &chunk, &bpos_in_chunk);
	auto hardness = blocks.hardness[b.id];

	if (!chunk || !blocks.breakable(b.id))
		return;

	if (hardness == 0) {
		b.hp = 0;
	} else {
		float damage_multiplier = (float)tool_props.hardness / (float)hardness;
		if (tool_props.tool == blocks.tool[b.id])
			damage_multiplier *= TOOL_MATCH_BONUS_DAMAGE;

		int dmg = ceili(tool_props.damage * damage_multiplier);

		if (creative_mode)
			dmg = INT_MAX;

		b.hp -= (uint8)min(dmg, (int)b.hp);
	}

	if (b.hp <= 0) {
		b = B_AIR;

		break_sound.play();
	}

	chunk->set_block(chunks, bpos_in_chunk, b);
}

bool World::try_place_block (bpos pos, block_id id) {
	Chunk* chunk;
	bpos bpos_in_chunk;
	Block b = chunks.query_block(pos, &chunk, &bpos_in_chunk);

	if (chunk && !blocks.breakable(b.id)) { // non-breakable blocks are solids and gasses
		chunk->set_block(chunks, bpos_in_chunk, id);
		return true;
	}
	return false;
}
