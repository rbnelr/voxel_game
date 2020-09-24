#pragma once
#include "stdafx.hpp"
#include "worldgen_dll.hpp"
#include "threading.hpp"
#include "svo.hpp"
#include "serialization.hpp"
#include "graphics/shaders.hpp"

namespace worldgen {
	struct WorldGenerator {
		// worldgen settings are loaded from and saved to <filename>.json
		std::string filename;

		// seed string, uint64_t seed is the hash of this string
		std::string seed_str = "test";
		uint64_t get_seed () const {
			auto str = kiss::trim(seed_str);

			if (str.size() == 0) // "" -> random seed
				return std::hash<uint64_t>()(random.uniform_u64());

			return std::hash<std::string_view>()(str);
		}

		struct NoiseSetting {
			std::string name = "";
			float period = 10.0f; // inverse of frequency
			float strength = 100.0f; // proportional to period
			std::vector<float> octaves;
			float param0 = 0.0f;
			float param1 = 0.0f;

			SERIALIZE(NoiseSetting, name, period, strength, octaves, param0, param1)
		};

		// array of noise settings
		std::vector<NoiseSetting> noises;

		struct GenericSetting {
			std::string name;
			float value;

			SERIALIZE(GenericSetting, name, value)
		};

		// generic float uniforms
		std::vector<GenericSetting> settings;

		SERIALIZE(WorldGenerator, seed_str, noises, settings)

		static WorldGenerator load (std::string filename) {
			auto wg = ::load<WorldGenerator>(prints("%s.json", filename.c_str()).c_str());
			wg.filename = std::move(filename);
			return wg;
		}
		void save () const {
			::save(prints("%s.json", filename.c_str()).c_str(), *this);
		}

		void imgui () {
			using namespace ImGui;
			if (!imgui_push("WorldGenerator")) return;

			InputText("Generator Name", &filename);

			InputText("seed str", &seed_str);
			Text("seed code: 0x%016p", get_seed());

			if (TreeNode("Noise Layers")) {
				if (ImGui::Button("Save"))		save();
				SameLine();
				if (ImGui::Button("Reload"))	*this = load(std::move(filename));

				Columns(6, "Noise Layers", false);
				Text("Name");			NextColumn();
				Text("period");			NextColumn();
				Text("strength [%]");	NextColumn();
				Text("param0");			NextColumn();
				Text("param1");			NextColumn();
				NextColumn();
				SetColumnWidth(0, 160);
				Separator();

				for (int i=0; i<(int)noises.size(); ++i) {
					auto& n = noises[i];
					TreePush(&n);
					SetNextItemWidth(-20);
					InputText(prints("[%d]###name", i).c_str(), &n.name); NextColumn();
					SetNextItemWidth(-1);
					DragFloat("##period", &n.period, 0.1f, 0.0001f, 10000, "%.2f", ImGuiSliderFlags_Logarithmic);	NextColumn();
					SetNextItemWidth(-1);
					DragFloat("##strength", &n.strength, 0.1f);							NextColumn();
					SetNextItemWidth(-1);
					DragFloat("##param0", &n.param0, 0.1f);								NextColumn();
					SetNextItemWidth(-1);
					DragFloat("##param1", &n.param1, 0.1f);								NextColumn();

					if (SmallButton("-")) {
						noises.erase(noises.begin() +i);
						--i;
					}
					SameLine();
					if (SmallButton("+"))
						noises.emplace(noises.begin() +i);
					SameLine();
					if (i > 0 && SmallButton("^"))
						std::swap(noises[i-1], noises[i]);
					SameLine();
					if (i < (int)noises.size()-1 && SmallButton("v"))
						std::swap(noises[i], noises[i+1]);
					NextColumn();

					TreePop();
				}

				if (SmallButton("+"))
					noises.emplace_back();
				NextColumn();

				Columns(1);
				TreePop();
			}

			imgui_pop();
		}

		void set_uniforms (Shader& shad) {
			for (int i=0; i<(int)noises.size(); ++i) {
				glUniform1f(glGetUniformLocation(shad.shader->shad, prints("nfreq[%d]",   i).c_str()), 1.0f / noises[i].period);
				glUniform1f(glGetUniformLocation(shad.shader->shad, prints("namp[%d]",    i).c_str()), noises[i].period * noises[i].strength / 100.0f);
				glUniform1f(glGetUniformLocation(shad.shader->shad, prints("nparam0[%d]", i).c_str()), noises[i].param0);
				glUniform1f(glGetUniformLocation(shad.shader->shad, prints("nparam1[%d]", i).c_str()), noises[i].param1);
			}

			for (auto& s : settings) {
				shad.set_uniform(s.name.c_str(), s.value);
			}
		}

		////
		generate_chunk_dll_fp generate_chunk_dll = nullptr;

		void generate_chunk (svo::Chunk* chunk, svo::SVO& svo) const {
			ZoneScoped;

			block_id* blocks;
			{
				ZoneScopedN("alloc buffer");
				blocks = (block_id*)calloc(1, sizeof(block_id) * CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE);
			}

			if (generate_chunk_dll)
				generate_chunk_dll(blocks, chunk->pos, chunk->scale - CHUNK_SCALE, get_seed());

			svo.chunk_to_octree(chunk, blocks);

			{
				ZoneScopedN("free buffer");
				free(blocks);
			}
		}
	};
}
using worldgen::WorldGenerator;
