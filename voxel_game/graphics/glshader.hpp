#pragma once
#include "globjects.hpp"
#include "../kissmath.hpp"
#include "../string.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include "assert.h"

struct ShaderManager {
	struct Uniform {
		GLint			loc = -1;
		gl::data_type	type;

		Uniform () {}
		Uniform (gl::data_type t): type{t} {}
	};
	struct _Shader {
		GLuint shad = 0; // 0 == shader no loaded successfully

		// all files used in the shader, ie. the source itself and all included files
		std::vector<std::string> sources;

		// all uniforms found in the shader
		std::unordered_map<kiss::map_string, Uniform> uniforms;

		bool get_uniform (std::string_view name, Uniform* type);

		~_Shader () {
			glDeleteProgram(shad);
		}
	};

	GLuint load_shader_part (const char* type, GLenum gl_type, std::string const& source, std::string_view path, _Shader* shader, std::string const& name, bool* error);
	std::unique_ptr<ShaderManager::_Shader> _load_shader (std::string const& name);
	
	const std::string shaders_directory = "shaders/";

	std::unordered_map<std::string, std::unique_ptr<_Shader>> shaders; // string is shader name, ie. source file path without .glsl

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
	_Shader* load_shader (std::string name);

	bool check_file_changes ();
};

// global ShaderManager
extern std::unique_ptr<ShaderManager> shader_manager;

// User facing shader
struct Shader {
	// lifetime is ok because shader manager keeps shaders alive until it gets destroyed with the gl context, at which point using shaders is not safe anyway
	ShaderManager::_Shader* shader;

	Shader (std::string name) {
		shader = shader_manager->load_shader(std::move(name));
	}

	// check if the shader is successfully loaded
	operator bool () { return shader->shad != 0; }

	void set_uniform (std::string_view name, float v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_FLT);
			glUniform1f(u.loc, v);
		}
	}
	void set_uniform (std::string_view name, float2 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_V2);
			glUniform2fv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, float3 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_V3);
			glUniform3fv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, float4 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_V4);
			glUniform4fv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, int v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_INT);
			glUniform1i(u.loc, v);
		}
	}
	void set_uniform (std::string_view name, int2 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_IV2);
			glUniform2iv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, int3 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_IV3);
			glUniform3iv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, int4 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_IV4);
			glUniform4iv(u.loc, 1, &v.x);
		}
	}
	void set_uniform (std::string_view name, float3x3 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_M3);
			glUniformMatrix3fv(u.loc, 1, GL_FALSE, &v.arr[0][0]);
		}
	}
	void set_uniform (std::string_view name, float4x4 v) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_M4);
			glUniformMatrix4fv(u.loc, 1, GL_FALSE, &v.arr[0][0]);
		}
	}
	void set_uniform (std::string_view name, bool b) {
		ShaderManager::Uniform u;
		if (shader->get_uniform(name, &u)) {
			assert(u.type == gl::T_BOOL);
			glUniform1i(u.loc, (int)b);
		}
	}
};
