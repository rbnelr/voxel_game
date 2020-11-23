#pragma once
#include "vulkan_helper.hpp"
#include "kisslib/stl_extensions.hpp"
#include "kisslib/serialization.hpp"
#include "kisslib/string.hpp"
#include "imgui/dear_imgui.hpp"
using kiss::prints;
using kiss::indexof;

namespace nlohmann {
	NLOHMANN_JSON_SERIALIZE_ENUM( shaderc_shader_kind, {
		{ shaderc_vertex_shader		, "vertex" },
		{ shaderc_fragment_shader	, "fragment" },
	})
}

namespace vk {
	static constexpr const char* SHADERC_STAGE_NAME[] = {
		"vertex", // shaderc_vertex_shader
		"fragment", // shaderc_fragment_shader
	};
	static constexpr const char* SHADERC_STAGE_MACRO[] = {
		"_VERTEX", // shaderc_vertex_shader
		"_FRAGMENT", // shaderc_fragment_shader
	};
	static constexpr VkShaderStageFlagBits SHADERC_STAGE_BITS_MAP[] = {
		VK_SHADER_STAGE_VERTEX_BIT, // shaderc_vertex_shader
		VK_SHADER_STAGE_FRAGMENT_BIT, // shaderc_fragment_shader
	};

	struct Shader {
		struct Stage {
			SERIALIZE(Stage, stage, override_src, compiler_msg)

			shaderc_shader_kind	stage;
			// override source file for this stage, if empty then defaults to <name>.glsl
			// (all stages can exist in one file by using  #if _VERTEX  #endif  blocks)
			std::string			override_src = "";

			// Output (errors / warnings) of last compilation of this shader stage
			std::string			compiler_msg;

			VkShaderModule		module = nullptr;
		};
		struct SrcFile {
			SERIALIZE(SrcFile, filename)

			std::string			filename;
			// file hash? to detect if cached binary can be used
		};

		SERIALIZE(Shader, name, valid, stages, src_files)

		std::string				name;
		bool					valid = false;
		
		std::vector<Stage>		stages = {
			{ shaderc_vertex_shader },
			{ shaderc_fragment_shader }
		};

		std::vector<SrcFile>	src_files; // all shader files that make up the shader
	};

	struct ShaderManager {
		SERIALIZE(ShaderManager, shaders_dir, shaders)

		std::string shaders_file = "shaders.json";
		std::string shaders_dir = "shaders/";

		std::vector<std::unique_ptr<Shader>> shaders;

		shaderc_compiler_t shaderc = nullptr;

		void save_json () {
			save(shaders_file.c_str(), *this);
		}

		void init (VkDevice dev) {
			//load(shaders_file.c_str(), this);
			//
			//for (auto& s : shaders) {
			//	load_shader(dev, s.get(), );
			//}
			//
			//save_json(); // cache potentially compiled shaders
		}
		void destroy (VkDevice dev) {
			for (auto& s : shaders) {
				destroy_shader(dev, s.get());
				s = nullptr;
			}

			if (shaderc)
				shaderc_compiler_release(shaderc);
		}

		typedef std::initializer_list<std::pair<char const*, char const*>> macro_list;
		std::string decorate_name (std::string_view name, macro_list macros) {
			std::string s;
			s = name;

			for (auto& m : macros) {
				s += prints(", %s:%s", m.first, m.second);
			}

			return s;
		}

		Shader* get (VkDevice dev, std::string_view name, macro_list macros={}) {
			std::string dname = decorate_name(name, macros);

			int i = indexof(shaders, dname, [] (std::unique_ptr<Shader>& l, std::string& r) { return l->name == r; });
			if (i >= 0)
				return shaders[i].get();

			// create default configured shader object if a unknown shader is requested
			auto ptr = std::make_unique<Shader>();
			ptr->name = std::move(name);

			load_shader(dev, ptr.get(), macros);

			auto* s = ptr.get();
			shaders.push_back(std::move(ptr));

			save_json(); // cache potentially compiled shader
			return s;
		}

		////
		bool load_shader (VkDevice dev, Shader* shader, macro_list macros) {
			std::string default_src = prints("%s.glsl", shader->name.c_str());

			shader->src_files.clear();

			shader->valid = true;
			for (auto& stage : shader->stages) {
				shader->valid = load_shader_stage(dev, shader, &stage, default_src.c_str(), macros) && shader->valid;
			}

			return shader->valid;
		}
		
		struct IncludeResult {
			shaderc_include_result res;

			std::string filepath;
			std::string source_code;
		};

		static shaderc_include_result* shaderc_include_resolve (
				void* user_data, const char* requested_source, int type,
				const char* requesting_source, size_t include_depth) {
			Shader* shader = (Shader*)user_data;

			// New buffer so it stays valid until shaderc_include_result_release
			auto* res = new IncludeResult();
			
			auto path = kiss::get_path(requesting_source);
			res->filepath = prints("%.*s%s", path.size(), path.data(), requested_source);

			shader->src_files.push_back({ res->filepath });

			bool file_found = kiss::load_text_file(res->filepath.c_str(), &res->source_code);

			if (file_found) {
				res->res.source_name = res->filepath.data();
				res->res.source_name_length = file_found ? res->filepath.size() : 0; // empty source_name signals include failure
				res->res.content = res->source_code.data();
				res->res.content_length = res->source_code.size();
			} else {
				res->source_code = prints("Source file \"%s\" could not be loaded!", res->filepath.c_str()); // error message instead of source code in failure case
				
				// empty source name signals include failure
				res->res.source_name = nullptr;
				res->res.source_name_length = 0;

				res->res.content = res->source_code.data();
				res->res.content_length = res->source_code.size();
			}

			return (shaderc_include_result*)res;
		}
		static void shaderc_include_result_release (void* user_data, shaderc_include_result* include_result) {
			delete ((IncludeResult*)include_result);
		}

		bool load_shader_stage (VkDevice dev, Shader* shader, Shader::Stage* stage, char const* default_src, macro_list macros) {
			stage->module = VK_NULL_HANDLE;

			VkShaderModuleCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			
			if (true) {
				char const* src_filename = !stage->override_src.empty() ? stage->override_src.c_str() : default_src;
				std::string src_filepath = prints("%s%s", shaders_dir.c_str(), src_filename);

				// compile from source
				shaderc_compile_options_t opt = shaderc_compile_options_initialize();
				shaderc_compile_options_set_optimization_level(opt, shaderc_optimization_level_performance);
				shaderc_compile_options_add_macro_definition(opt, SHADERC_STAGE_MACRO[stage->stage], strlen(SHADERC_STAGE_MACRO[stage->stage]), "", 0);

				for (auto& m : macros) {
					shaderc_compile_options_add_macro_definition(opt, m.first, strlen(m.first), m.second, strlen(m.second));
				}

				shaderc_compile_options_set_include_callbacks(opt, shaderc_include_resolve, shaderc_include_result_release, shader);

				if (!shaderc)
					shaderc = shaderc_compiler_initialize();

				std::string source;
				if (!kiss::load_text_file(src_filepath.c_str(), &source)) {
					stage->compiler_msg = prints("Shader file not found \"%s\"!", src_filepath.c_str());
					fprintf(stderr, "[SHADER] %s\n", stage->compiler_msg.c_str());
					clog(ERROR, "[SHADER] %s", stage->compiler_msg.c_str());
					return false;
				}

				shader->src_files.push_back({ src_filepath });
				
				auto res = shaderc_compile_into_spv(shaderc, source.data(), source.size(), stage->stage, src_filepath.c_str(), "main", opt);

				auto status = shaderc_result_get_compilation_status(res);
				auto message = shaderc_result_get_error_message(res);
				if (status != shaderc_compilation_status_success) {
					stage->compiler_msg = prints("Compiler error:\n%s", message);
					fprintf(stderr, "[SHADER] %s\n", stage->compiler_msg.c_str());
					clog(ERROR, "[SHADER] %s", stage->compiler_msg.c_str());
					return false;
				} else if (message && *message != '\0') {
					stage->compiler_msg = prints("Compiler message:\n%s", message);
					fprintf(stderr, "[SHADER] %s\n", stage->compiler_msg.c_str());
					clog(WARNING, "[SHADER] %s", stage->compiler_msg.c_str());
				}

				stage->compiler_msg = message;

				info.codeSize = shaderc_result_get_length(res);
				info.pCode = (uint32_t*)shaderc_result_get_bytes(res);

				if (vkCreateShaderModule(dev, &info, nullptr, &stage->module) != VK_SUCCESS) {
					stage->compiler_msg = prints("vkCreateShaderModule failed!");
					fprintf(stderr, "%s\n", stage->compiler_msg.c_str());
					clog(ERROR, stage->compiler_msg.c_str());
					return false;
				}

				shaderc_result_release(res);
				shaderc_compile_options_release(opt);

				return true;
			} else {
				// load cached SPIR-V

				return false;
			}
		}
		static void destroy_shader (VkDevice dev, Shader* s) {
			for (auto& s : s->stages) {
				if (s.module)
					vkDestroyShaderModule(dev, s.module, nullptr);
			}
		}
		
	};
}
