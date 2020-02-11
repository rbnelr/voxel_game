#pragma once
#include "glshader.hpp"

struct ShaderManager {

	const std::string shaders_directory = "shaders/";

	// string is shader name, ie. source file path without .glsl
	std::unordered_map<std::string, std::unique_ptr<gl::Shader>> shaders;

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
	gl::Shader* load_shader (std::string name);

	bool check_file_changes ();
};

// global ShaderManager
extern std::unique_ptr<ShaderManager> shader_manager;

// User facing shader
struct Shader {
	// lifetime is ok because shader manager keeps shaders alive until it gets destroyed with the gl context, at which point using shaders is not safe anyway
	gl::Shader* shader = nullptr;

	Shader () {}
	Shader (std::string name, std::initializer_list<SharedUniformsInfo> uniforms={}) {
		shader = shader_manager->load_shader(std::move(name));
		for (auto& u : uniforms)
			shader->bind_uniform_block(u);
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
};
