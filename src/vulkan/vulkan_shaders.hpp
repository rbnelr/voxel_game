#pragma once
#include "vulkan_helper.hpp"

namespace vk {
	static constexpr const char* SHADER_STAGE_MACRO[] = {
		"_VERTEX", // shaderc_vertex_shader
		"_FRAGMENT", // shaderc_fragment_shader
	};

	struct Shader {
		std::string name;

		std::vector<std::string> src_files; // all shader files that make up the shader

		struct Stage {
			shaderc_shader_kind	stage;
			// override source file for this stage, if empty then defaults to <name>.glsl
			// (all stages can exist in one file by using  #if _VERTEX  #endif  blocks)
			std::string			override_src;

			VkShaderModule		module;
		};

		std::vector<Stage> stages;
	};

	inline shaderc_include_result* shaderc_include_resolve (
		void* user_data, const char* requested_source, int type,
		const char* requesting_source, size_t include_depth) {
		printf("");

		auto* res = (shaderc_include_result*)malloc(sizeof(shaderc_include_result));
		return res;
	}

	inline void shaderc_include_result_release (void* user_data, shaderc_include_result* include_result) {
		free(include_result);
	}

	inline VkShaderModule compile_shader_stage (VkDevice device, shaderc_compiler_t shaderc, shaderc_shader_kind stage, std::string code) {
		VkShaderModule mod = VK_NULL_HANDLE;

		shaderc_compile_options_t opt = shaderc_compile_options_initialize();
		shaderc_compile_options_set_optimization_level(opt, shaderc_optimization_level_performance);
		shaderc_compile_options_add_macro_definition(opt, SHADER_STAGE_MACRO[stage], strlen(SHADER_STAGE_MACRO[stage]), "", 0);

		shaderc_compile_options_set_include_callbacks(opt, shaderc_include_resolve, shaderc_include_result_release, nullptr);

		auto res = shaderc_compile_into_spv(shaderc, code.data(), code.size(), stage, SHADER_STAGE_MACRO[stage], "main", opt);

		auto status = shaderc_result_get_compilation_status(res);
		auto message = shaderc_result_get_error_message(res);
		if (status != shaderc_compilation_status_success)
			fprintf(stderr, "vertex shader compilation error:\n%s\n", message);

		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = shaderc_result_get_length(res);
		info.pCode = (uint32_t*)shaderc_result_get_bytes(res);
		if (vkCreateShaderModule(device, &info, nullptr, &mod) != VK_SUCCESS) {
			fprintf(stderr, "vkCreateShaderModule failed!\n");
		}

		shaderc_result_release(res);
		shaderc_compile_options_release(opt);
		return mod;
	}

	//inline bool load_shader (Shader* s, ) {
	//	
	//}
}
