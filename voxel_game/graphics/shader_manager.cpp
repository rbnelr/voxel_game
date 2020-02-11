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

std::unique_ptr<ShaderManager> shader_manager = nullptr;
