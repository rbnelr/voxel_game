#pragma once
#include "stdafx.hpp"
#include "glshader.hpp"
#include "util/string.hpp"

struct ShaderManager {

	const std::string shaders_directory = "shaders/";

	// string is shader name, ie. source file path without .glsl
	std::unordered_map<std::string, std::unique_ptr<gl::Shader>> shaders;

	DirectoyChangeNotifier notify = DirectoyChangeNotifier(shaders_directory);

	// Loads a shader file with filename "{name}.glsl" in shaders_directory which gets preprocessed into vertex, fragment, etc. shaders by only including
	// $if {type}
	//    ...
	// $endif
	// sections for the matching type (ie. $if vertex gets included in vertex shader but preprocessed out in fragment shader
	// vertex and fragment $if are required the other shader types are optional and will cause their respective shader type to not be compiled
	// code outside if $if $endif is included in all shaders
	// $include "filepath" includes a file relative to the path of the current file
	// $if $endif get processed at the same time as the $if $endif, so a $if fragment part in a file only included inside a $if vertex will never be included in the fragment shader
	// Shaders gets stored into shaders hashmap and never removed until program exit
	gl::Shader* load_shader (std::string name, std::initializer_list<SharedUniformsInfo> UBOs);

	std::unordered_map<std::string, bool> shader_windows;
	void imgui () {
		if (!imgui_push("ShaderManager")) return;

		for (auto it=shaders.begin(); it!=shaders.end(); ++it) {
			bool open = shader_windows[it->first];
			if (ImGui::Button(it->first.c_str()))
				open = true;

			if (open && ImGui::Begin(it->first.c_str(), &open)) {
				auto s = it->second.get();

				std::string filename = kiss::prints("shaders/%s.glslasm", it->first.c_str());

				if (ImGui::Button(kiss::prints("Write Disasm To File (%s)###(dump)", filename.c_str()).c_str())) {
					uint64_t size;
					auto data = it->second->get_program_binary(&size);

					kiss::save_binary_file(filename.c_str(), data.get(), size);
				}

				int i=0;
				for (auto pp : s->preprocessed_sources) {
					if (ImGui::TreeNodeEx(kiss::prints("%d", i++).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {

						ImGui::Text(pp.c_str());

						ImGui::TreePop();
					}
				}

				ImGui::End();
			}

			shader_windows[it->first] = open;
		}

		imgui_pop();
	}

	void reload_shaders_on_change () {
		auto changed_files = notify.poll_changes();

		for (auto& it : shaders) {
			if (changed_files.contains_any(it.second->sources, FILE_ADDED|FILE_MODIFIED|FILE_RENAMED_NEW_NAME)) {
				clog(INFO, "reloading shader \"%s\" due to changed file", it.first.c_str());

				auto new_shader = gl::load_shader(it.first, shaders_directory.c_str(), it.second->UBOs);

				if (new_shader->shad != 0)
					gl::swap(*it.second, *new_shader); // replace old shader with new shader without changing pointer
			}
		}
	}
};

// global ShaderManager
extern std::unique_ptr<ShaderManager> shaders;

// User facing shader
struct Shader {
	// lifetime is ok because shader manager keeps shaders alive until it gets destroyed with the gl context, at which point using shaders is not safe anyway
	gl::Shader* shader = nullptr;

	Shader () {}
	Shader (std::string name, std::initializer_list<SharedUniformsInfo> UBOs={}) {
		shader = shaders->load_shader(std::move(name), UBOs);
	}

	void bind () {
		glUseProgram(shader->shad);
	}

	// check if the shader is successfully loaded
	operator bool () { return shader->shad != 0; }

	void set_uniform (std::string_view name, float v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::FLOAT);
			glUniform1f(u.loc, v);
		}
	}
	void set_uniform (std::string_view name, float2 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::FLOAT2);
			glUniform2fv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, float3 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::FLOAT3);
			glUniform3fv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, float4 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::FLOAT4);
			glUniform4fv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, int v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::INT);
			glUniform1i(u.loc, v);
		}
	}
	void set_uniform (std::string_view name, int2 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::INT2);
			glUniform2iv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, int3 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::INT3);
			glUniform3iv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, int4 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::INT4);
			glUniform4iv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, float3x3 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::MAT3);
			glUniformMatrix3fv(u.loc, 1, GL_FALSE, &v.arr[0][0]);
		}
	}
	void set_uniform (std::string_view name, float4x4 v) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::MAT4);
			glUniformMatrix4fv(u.loc, 1, GL_FALSE, &v.arr[0][0]);
		}
	}
	void set_uniform (std::string_view name, bool b) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::BOOL);
			glUniform1i(u.loc, (int)b);
		}
	}

	void set_uniform_pointer (std::string_view name, GLuint64EXT ptr) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::POINTER);
			glUniformui64NV(u.loc, ptr);
		}
	}

	void set_texture_unit (std::string_view name, int unit) {
		gl::Uniform u;
		if (shader->get_uniform(name, &u)) {
			if (!gl::is_sampler_type(u.type))
				clog(WARNING, "Uniform \"%s\" is not a sampler type!", name.data());
			glUniform1i(u.loc, unit);
		} else {
			//logf(WARNING, "Texture \"%s\" does not exist in shader!", name.data());
		}
	}
};
