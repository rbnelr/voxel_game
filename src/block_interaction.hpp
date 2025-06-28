#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "items.hpp"
#include "game.hpp"

struct VoxelHit {
	int3		pos;
	block_id	bid;
	BlockFace	face; // -1 == no face
	float		dist;
};
struct SelectedBlock {
	VoxelHit	hit;
	bool		is_selected = false;

	float		damage = 0; // damage is accumulated from prev frame if was_selected then and is_selected now and if pos is the same

	operator bool () const {
		return is_selected;
	}
};

struct BlockInteraction {
	static void apply_block_damage (Chunks& chunks, SelectedBlock& block, Item& item, bool creative_mode) {
		assert(block);
		auto& tool_props = item.get_props();

		auto hardness = g->assets->block_types[block.hit.bid].hardness;

		if (!g->assets->block_types.block_breakable(block.hit.bid))
			return;

		float dmg = 0;
		if (hardness == 0) {
			dmg = 1.0f;
		} else if (hardness == 255) {
			dmg = 0.0f;
		} else {
			float damage_multiplier = (float)tool_props.hardness / (float)hardness;
			if (tool_props.tool == g->assets->block_types[block.hit.bid].tool)
				damage_multiplier *= TOOL_MATCH_BONUS_DAMAGE;

			dmg = ((float)tool_props.damage / 255.0f) * damage_multiplier;

			if (creative_mode)
				dmg = 1.0f;
		}
		block.damage += dmg;

		if (block.damage >= 1) {
			g->assets->break_sound.play_once();

			g->chunks->write_block(block.hit.pos.x, block.hit.pos.y, block.hit.pos.z, g->assets->block_types.air_id);
		}
	}
	static bool try_place_block (Chunks& chunks, int3 pos, block_id id) {
		// Can't place solid blocks inside player, but can do with non-player collision blocks
		auto& bt = g->assets->block_types[id];
		if (entity_in_block(pos, bt) && bt.collision == CM_SOLID) return false;

		auto cur_block = g->chunks->read_block(pos.x, pos.y, pos.z);

		if (!g->assets->block_types.block_replaceable(cur_block)) // can only place inside air or liquid
			return false;

		g->chunks->write_block(pos.x, pos.y, pos.z, id);
		return true;
	}

	static bool entity_in_block (int3 block_place_pos, BlockTypes::Block const& bt);
	
	static void aimed_block_selection (SelectedBlock& block, Camera_View& view, float reach) {
		Ray ray;
		ray.dir = (float3x3)view.cam_to_world * float3(0,0,-1);
		ray.pos = view.cam_to_world * float3(0,0,0);

		VoxelHit hit;
		block.is_selected = raycast_breakable_blocks(*g->chunks, ray, reach, hit, g->creative_mode);
		if (block.is_selected) block.hit = hit;
	}
	
	static bool raycast_breakable_blocks (Chunks& chunks, Ray const& ray, float max_dist, VoxelHit& hit, bool hit_at_max_dist) {
		ZoneScoped;

		bool did_hit = false;

		raycast_voxels(ray, [&] (int3 const& pos, int axis, float dist) -> bool {
			//g_debugdraw.wire_cube((float3)pos+0.5f, 1, lrgba(1,0,0,1));

			hit.dist = dist;
			hit.pos = pos;
			hit.bid = chunks.read_block(pos.x, pos.y, pos.z);

			if (dist > max_dist) {
				if (hit_at_max_dist) {
					hit.face = (BlockFace)-1; // select block itself instead of face (for creative mode block placing)
					did_hit = true;
				}
				return true;
			}

			if (g->assets->block_types.block_breakable(hit.bid)) {
				hit.face = (BlockFace)face_from_stepmask(axis, ray.dir);
				did_hit = true;
				return true;
			}
			return false;
		});

		return did_hit;
	}
};

struct BlockBreakAnim {

	float anim_speed = 4;
	float damage = 0.25f;
	float reach = 4.5f;

	float anim_t = 0;
	bool anim_triggered;

	void imgui (const char* name=nullptr) {
		if (!ImGui::TreeNode("Fists", name)) return;

		ImGui::DragFloat("anim_speed", &anim_speed, 0.05f);
		ImGui::DragFloat("damage", &damage, 0.05f);
		ImGui::DragFloat("reach", &reach, 0.05f);

		ImGui::TreePop();
	}
	void update (Input& I, Item& item, SelectedBlock& sel, ButtonState const& attack) {
		auto& button = I.buttons[MOUSE_BUTTON_LEFT];
		bool inp = sel ? attack.is_down : attack.went_down;

		float anim_hit_t = 0;

		//log(INFO, "[BreakBlock] anim_t: %f", anim_t);

		if (anim_t > 0 || inp) {
			anim_t += anim_speed * I.dt;
		}
		if (!anim_triggered && anim_t > anim_hit_t && inp) {
			//log(INFO, "[BreakBlock] anim hit");
			if (sel) {
				BlockInteraction::apply_block_damage(*g->chunks, sel, item, g->creative_mode);
				g->assets->hit_sound.play_once(1, random.uniformf(0.95f, 1.05f));
			}
			anim_triggered = true;
		}
		if (anim_t >= 1) {
			//log(INFO, "[BreakBlock] anim over");
			anim_t = 0;
			anim_triggered = false;
		}
	}
};

struct BlockPlaceAnim {

	float repeat_speed = 5.5f;
	float anim_speed = 4;
	float reach = 4.5f;

	float anim_t = 0;

	void imgui (const char* name=nullptr) {
		if (!ImGui::TreeNode("BlockPlace", name)) return;

		ImGui::DragFloat("anim_speed", &anim_speed, 0.05f);
		ImGui::DragFloat("reach", &reach, 0.05f);

		ImGui::TreePop();
	}
	void update (Input& I, Item& item, SelectedBlock& sel, ButtonState const& build) {
		bool can_place = item.is_block() && item.block.count > 0;

		bool inp = build.is_down && can_place;
		if (inp && anim_t >= anim_speed / repeat_speed) {
			anim_t = 0;
		}
		bool trigger = inp && anim_t == 0;

		if (trigger && sel && can_place) {
			int3 offs = 0;
			if (sel.hit.face >= 0)
				offs[sel.hit.face / 2] = (sel.hit.face % 2) ? +1 : -1;

			int3 block_place_pos = sel.hit.pos + offs;

			if (!BlockInteraction::try_place_block(*g->chunks, block_place_pos, (block_id)item.id)) {
				trigger = false;
			}
		}
	
		if (trigger || anim_t > 0) {
			anim_t += anim_speed * I.dt;

			if (anim_t >= 1) {
				anim_t = 0;
			}
		}
	}
};
