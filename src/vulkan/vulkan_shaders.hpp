#pragma once
#include "vulkan_helper.hpp"
#include "vulkan_window.hpp"
#include "imgui/dear_imgui.hpp"
using kiss::prints;
using kiss::indexof;

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

struct MacroDefinition {
	std::string name;
	std::string value;
};
typedef std::vector<MacroDefinition> macro_list;

struct ShaderStageConfig {
	shaderc_shader_kind	stage;
	// can override source file for this stage, if empty then defaults to <source_file>.glsl
	// (all stages can exist in one file by using  #if _VERTEX  #endif  blocks)
	std::string			stage_source_file = "";
};

// Simplified options for pipeline creation
struct PipelineOptions {
	int					msaa = 1;

	bool				alpha_blend = false;
	bool				depth_test = true;
	bool				depth_clamp = false;
	VkCullModeFlagBits	cull_mode = VK_CULL_MODE_BACK_BIT;
	VkPrimitiveTopology	primitive_mode = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode		polygon_mode = VK_POLYGON_MODE_FILL;
};

// All pipeline create settings in one struct exposed to caller to allow custom overrides not contained in ShaderConfig
struct PipelineConfig {
	//// Basic pipeline config

	// base name of shader source ie. all shaders get loaded from <source_file>.glsl
	std::string			shader;

	// Still need to manually create layouts for now
	VkPipelineLayout	layout;

	VkRenderPass		render_pass;
	int					subpass;

	VertexAttributes	attribs;

	macro_list			macros;

	std::vector<ShaderStageConfig>	shader_stages;

	//// All pipeline options (filled with defaults based on BasicOptions, but can be further modified if required before passing into compile_pipeline)

	VkPipelineInputAssemblyStateCreateInfo	input_assembly = {};
	VkPipelineRasterizationStateCreateInfo	rasterizer = {};
	VkPipelineMultisampleStateCreateInfo	multisampling = {};
	VkPipelineDepthStencilStateCreateInfo	depth_stencil = {};
	VkPipelineColorBlendAttachmentState		color_blend_attachment = {};

	std::vector<VkDynamicState>				dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkViewport								viewport = { 0, 0, 1920, 1080, 0, 1 }; // ignored with VK_DYNAMIC_STATE_VIEWPORT
	VkRect2D								scissor = { 0, 0, 1920, 1080 }; // ignored with VK_DYNAMIC_STATE_SCISSOR

	PipelineConfig () {}
	PipelineConfig (std::string_view shader, VkPipelineLayout layout, VkRenderPass render_pass, int subpass,
			PipelineOptions const& opt={}, VertexAttributes const& attribs={}, macro_list const& macros={},
			std::vector<ShaderStageConfig> const& shader_stages={{shaderc_vertex_shader}, {shaderc_fragment_shader}}) {
		this->shader = shader;
		this->layout = layout;
		this->render_pass = render_pass;
		this->subpass = subpass;
		this->attribs = attribs;
		this->macros = macros;
		this->shader_stages = shader_stages;
		init_cfg_details(opt);
	}

	void init_cfg_details (PipelineOptions const& opt) {

		input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly.topology = opt.primitive_mode;
		input_assembly.primitiveRestartEnable = VK_FALSE;

		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = opt.depth_clamp;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = opt.polygon_mode;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = opt.cull_mode;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		//rasterizer.depthBiasConstantFactor = 0.0f;
		//rasterizer.depthBiasClamp = 0.0f;
		//rasterizer.depthBiasSlopeFactor = 0.0f;

		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = (VkSampleCountFlagBits)opt.msaa;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil.depthTestEnable = opt.depth_test;
		depth_stencil.depthWriteEnable = opt.depth_test;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // use reverse depth
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.minDepthBounds = 0.0f;
		depth_stencil.maxDepthBounds = 1.0f;
		depth_stencil.stencilTestEnable = VK_FALSE;
		//depth_stencil.front = {};
		//depth_stencil.back = {};

		color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.blendEnable = opt.alpha_blend;

		if (opt.alpha_blend) {
			color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
			color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		} else {
			color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
			color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		}
	}
};

// a compiled pipeline that can be optionally recompiled
struct Pipeline {
	std::string					name; // mainly for debug identification

	VkPipeline					pipeline = VK_NULL_HANDLE;

	std::vector<VkShaderModule>	shader_modules;
	std::vector<std::string>	shader_sources;

	PipelineConfig				cfg;

	void destroy (VkDevice dev) {
		for (auto& m : shader_modules) {
			if (m)
				vkDestroyShaderModule(dev, m, nullptr);
		}
		if (pipeline)
			vkDestroyPipeline(dev, pipeline, nullptr);
	}
};

struct PipelineManager {
	
	std_string shaders_dir = "shaders/";

	std::vector<std::unique_ptr<Pipeline>> pipelines;

	shaderc_compiler_t shaderc = nullptr;

	void init (VkDevice dev) {
		shaderc = shaderc_compiler_initialize();
	}
	void destroy (VkDevice dev) {
		for (auto& p : pipelines)
			p->destroy(dev);
		pipelines.clear();

		if (shaderc)
			shaderc_compiler_release(shaderc);
	}

	void reload_changed_shaders (VulkanWindowContext& ctx, kiss::ChangedFiles& changed_files) {
		if (changed_files.any()) {
			for (auto& p : pipelines) {
				if (changed_files.contains_any(p->shader_sources, FILE_ADDED|FILE_MODIFIED|FILE_RENAMED_NEW_NAME)) {
					// any source file was changed
					recompile_pipeline(ctx, *p);
				}
			}
		}
	}

	Pipeline* create_pipeline (VulkanWindowContext& ctx, std::string name, PipelineConfig const& cfg) {
		auto p = std::make_unique<Pipeline>();
		p->name = std::move(name);
		p->cfg = cfg;
		_compile_pipeline(ctx, *p);

		auto* ptr = p.get();
		pipelines.push_back(std::move(p));
		return ptr;
	}
	bool recompile_pipeline (VulkanWindowContext& ctx, Pipeline& pipeline) {
		Pipeline new_pipeline;
		new_pipeline.name = pipeline.name;
		new_pipeline.cfg = pipeline.cfg;

		if (_compile_pipeline(ctx, new_pipeline)) { // only apply new pipeline if there is no compiler error to avoid vulkan freaking out while I fix errors in the edited shader
			
			vkQueueWaitIdle(ctx.queues.graphics_queue);

			pipeline.destroy(ctx.dev);
			pipeline = new_pipeline;
			
			clog(INFO, "[PipelineManager] Reloaded Pipeline due to shader source change (\"%s\")", pipeline.name.c_str());
			return true;
		}
		return false;
	}

	bool _compile_pipeline (VulkanWindowContext& ctx, Pipeline& pipeline) {
		auto& cfg = pipeline.cfg;
		
		pipeline.shader_modules.resize(cfg.shader_stages.size());
		std::vector<VkPipelineShaderStageCreateInfo> stages_info (cfg.shader_stages.size());

		bool error = false;
		{
			std::string default_src = prints("%s.glsl", cfg.shader.c_str());

			for (size_t i=0; i<cfg.shader_stages.size(); ++i) {
				auto& stage = cfg.shader_stages[i];

				std::string source_file = prints("%s%s", shaders_dir.c_str(),
					stage.stage_source_file.empty() ? default_src.c_str() : stage.stage_source_file.c_str());

				pipeline.shader_modules[i] = compile_shader_stage(ctx, stage.stage, source_file.c_str(), cfg.macros, pipeline.shader_sources);

				if (pipeline.shader_modules[i] == VK_NULL_HANDLE)
					error = true;

				stages_info[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stages_info[i].stage = SHADERC_STAGE_BITS_MAP[stage.stage];
				stages_info[i].module = pipeline.shader_modules[i];
				stages_info[i].pName = "main";
			}
		}

		if (error) {
			pipeline.pipeline = VK_NULL_HANDLE;
			return false;
		}

		// Declare some of the VkGraphicsPipelineCreateInfo inputs here because they pretty much just contain pointers to things we already have as arrays

		VkPipelineVertexInputStateCreateInfo	vertex_input = {};
		vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		if (!cfg.attribs.attribs.empty()) {
			vertex_input.vertexBindingDescriptionCount = 1;
			vertex_input.pVertexBindingDescriptions = &cfg.attribs.descr;
			vertex_input.vertexAttributeDescriptionCount = (uint32_t)cfg.attribs.attribs.size();
			vertex_input.pVertexAttributeDescriptions = cfg.attribs.attribs.data();
		}

		VkPipelineDynamicStateCreateInfo dynamic_state = {};
		dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state.dynamicStateCount = (uint32_t)cfg.dynamic_states.size();
		dynamic_state.pDynamicStates = cfg.dynamic_states.data();

		VkPipelineViewportStateCreateInfo		viewport_state = {};
		viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.viewportCount = 1;
		viewport_state.pViewports = &cfg.viewport;
		viewport_state.scissorCount = 1;
		viewport_state.pScissors = &cfg.scissor;

		VkPipelineColorBlendStateCreateInfo		color_blending = {};
		color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blending.logicOpEnable = VK_FALSE;
		color_blending.logicOp = VK_LOGIC_OP_COPY;
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &cfg.color_blend_attachment;
		//color_blending.blendConstants[0] = 0.0f;
		//color_blending.blendConstants[1] = 0.0f;
		//color_blending.blendConstants[2] = 0.0f;
		//color_blending.blendConstants[3] = 0.0f;

		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.stageCount				= (uint32_t)stages_info.size();
		info.pStages				= stages_info.data();
		info.pVertexInputState		= &vertex_input;
		info.pInputAssemblyState	= &cfg.input_assembly;
		info.pViewportState			= &viewport_state;
		info.pRasterizationState	= &cfg.rasterizer;
		info.pMultisampleState		= &cfg.multisampling;
		info.pDepthStencilState		= &cfg.depth_stencil;
		info.pColorBlendState		= &color_blending;
		info.pDynamicState			= &dynamic_state;
		info.layout					= cfg.layout;
		info.renderPass				= cfg.render_pass;
		info.subpass				= cfg.subpass;
		info.basePipelineHandle		= VK_NULL_HANDLE;
		info.basePipelineIndex		= -1;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(ctx.dev, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline.pipeline));
		GPU_DBG_NAME(ctx, pipeline.pipeline, pipeline.name.c_str());

		return true;
	}

	//// Shader compliation

	struct IncludeResult {
		shaderc_include_result res;

		std::string filepath;
		std::string source_code;
	};

	static shaderc_include_result* shaderc_include_resolve (
		void* user_data, const char* requested_source, int type,
		const char* requesting_source, size_t include_depth) {
		std::vector<std::string>* sources = (std::vector<std::string>*)user_data;

		// New buffer so it stays valid until shaderc_include_result_release
		auto* res = new IncludeResult();

		auto path = kiss::get_path(requesting_source);
		res->filepath = prints("%.*s%s", path.size(), path.data(), requested_source);

		sources->emplace_back(res->filepath);

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

	VkShaderModule compile_shader_stage (VulkanWindowContext& ctx, shaderc_shader_kind stage, std::string const& source_file, macro_list const& macros, std::vector<std::string>& sources) {
		// compile from source
		shaderc_compile_options_t opt = shaderc_compile_options_initialize();
		shaderc_compile_options_set_optimization_level(opt, shaderc_optimization_level_performance);

		shaderc_compile_options_add_macro_definition(opt, SHADERC_STAGE_MACRO[stage], strlen(SHADERC_STAGE_MACRO[stage]), "", 0);
		for (auto& m : macros)
			shaderc_compile_options_add_macro_definition(opt, m.name.data(), m.name.size(), m.value.data(), m.value.size());

		shaderc_compile_options_set_include_callbacks(opt, shaderc_include_resolve, shaderc_include_result_release, &sources);

		std::string source;
		if (!kiss::load_text_file(source_file.c_str(), &source)) {
			clog(ERROR, "[SHADER] Shader file not found \"%s\"!", source_file.c_str());
			return VK_NULL_HANDLE;
		}

		sources.emplace_back(source_file);

		auto res = shaderc_compile_into_spv(shaderc, source.data(), source.size(), stage, source_file.c_str(), "main", opt);

		auto status = shaderc_result_get_compilation_status(res);
		auto message = shaderc_result_get_error_message(res);
		if (status != shaderc_compilation_status_success) {
			clog(ERROR, "[SHADER] Compiler error:\n%s", message);
			return VK_NULL_HANDLE;
		} else if (message && *message != '\0') {
			clog(WARNING, "[SHADER] Compiler message:\n%s", message);
		}

		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = shaderc_result_get_length(res);
		info.pCode = (uint32_t*)shaderc_result_get_bytes(res);

		VkShaderModule module = VK_NULL_HANDLE;
		if (vkCreateShaderModule(ctx.dev, &info, nullptr, &module) != VK_SUCCESS) {
			clog(ERROR, "vkCreateShaderModule failed!");
			return VK_NULL_HANDLE;
		}
		GPU_DBG_NAME(ctx, module, source_file.c_str());

		shaderc_result_release(res);
		shaderc_compile_options_release(opt);

		return module;
	}
};

} // namespace vk
