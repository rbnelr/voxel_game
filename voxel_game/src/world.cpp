#include "stdafx.hpp"
#include "world.hpp"

void World::raycast_breakable_blocks (SelectedBlock& block, Ray ray, float max_dist, bool hit_at_max_dist, float* hit_dist) {
	block.is_selected = false;

	float _dist;
	auto hit_block = [&] (int3 bp, int face, float dist, bool force_hit) {
		block_id bid = voxels.query_block(bp);
		if ((blocks.breakable(bid) || force_hit)) {
			//hit.pos_world = ray.pos + ray.dir * dist;
			block.is_selected = true;
			block.block = bid;
			block.pos = bp;
			block.face = (BlockFace)face;
			_dist = dist;
			return true;
		}
		return false;
	};

	if (!raycast_voxels(ray, max_dist, hit_block, nullptr, hit_at_max_dist)) {
		return;
	}

	if (hit_dist) *hit_dist = _dist;
}

void World::apply_damage (SelectedBlock& block, Item& item, bool creative_mode) {
	assert(block);
	auto tool_props = item.get_props();
	
	auto hardness = blocks.hardness[block.block];
	
	if (!blocks.breakable(block.block))
		return;
	
	float dmg = 0;
	if (hardness == 0) {
		dmg = 1.0f;
	} else if (hardness == 255) {
		dmg = 0.0f;
	} else {
		float damage_multiplier = (float)tool_props.hardness / (float)hardness;
		if (tool_props.tool == blocks.tool[block.block])
			damage_multiplier *= TOOL_MATCH_BONUS_DAMAGE;
	
		dmg = tool_props.damage * damage_multiplier;
	
		if (creative_mode)
			dmg = 1;
	}
	block.damage += dmg;
	
	if (block.damage >= 1) {
		block.block = B_AIR;
	
		break_sound.play();

		voxels.set_block(block.pos, block.block);
	}
}

bool World::try_place_block (int3 pos, block_id id) {
	block_id b = voxels.query_block(pos);
	
	if (!blocks.breakable(b)) { // non-breakable blocks are solids and gasses
		voxels.set_block(pos, id);
		return true;
	}
	return false;
}
