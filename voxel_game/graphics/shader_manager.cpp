#include "shader_manager.hpp"

gl::Shader* ShaderManager::load_shader (std::string name) {
	auto ptr = gl::load_shader(name, shaders_directory.c_str());
	gl::Shader* s = ptr.get();
	shaders.emplace(std::move(name), std::move(ptr));

	return s;
}

bool ShaderManager::check_file_changes () {
	return false;
}

bool gl::Shader::get_uniform (std::string_view name, Uniform* u) {
	auto ret = uniforms.find(name);
	if (ret == uniforms.end())
		return false;
	*u = ret->second;
	return true;
}

std::unique_ptr<ShaderManager> shader_manager = nullptr;
