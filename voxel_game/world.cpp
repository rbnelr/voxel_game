#include "world.hpp"

SelectedBlock World::raycast_solid_blocks (Ray ray, float max_dist, float* hit_dist) {
	SelectedBlock b;
	float _dist;
	auto hit_block = [&] (bpos bp, int face, float dist) {
		Chunk* chunk;
		b.block = chunks.query_block(bp, &chunk);
		if (chunk && b.block && block_props[b.block->type].collision == CM_SOLID) {
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

void World::update () {

	chunks.update_chunks_load(*this, world_gen, player);

}

void World::apply_damage (SelectedBlock const& block, float damage) {
	assert(block);
	Chunk* chunk;
	Block* b = chunks.query_block(block.pos, &chunk);
	assert(chunk && block_props[b->type].collision == CM_SOLID);

	b->hp -= min((uint8)ceili(damage * 255), b->hp);

	if (b->hp > 0) {
		chunk->block_only_texture_changed(block.pos);
	} else {

		b->hp = 0;
		b->type = BT_AIR;

		chunk->block_changed(chunks, block.pos);

		break_sound.play();
	}
}

bool World::try_place_block (bpos pos, block_type bt) {
	Chunk* chunk;
	Block* b = chunks.query_block(pos, &chunk);

	if (chunk && b && block_props[b->type].collision != CM_SOLID) { // can replace liquid and gas blocks
		b->type = bt;
		b->hp = 255;

		chunk->block_changed(chunks, pos);
		return true;
	}
	return false;
}
