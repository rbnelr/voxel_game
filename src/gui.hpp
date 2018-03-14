
static ImVec2 imgui_v2 (iv2 v) {	return ImVec2(v.x,v.y); }
static ImVec2 imgui_v2 (v2 v) {		return ImVec2(v.x,v.y); }

static GLint IMGUI_TEX_UNIT = 0;

static Shader* shad_imgui;
static Shader* shad_texture_window_tex;

static GLuint tex_sampler_nearest;

static void begin_texture_window_tex (const ImDrawList* parent_list, const ImDrawCmd* cmd);
static void end_texture_window_tex (const ImDrawList* parent_list, const ImDrawCmd* cmd);

struct Imgui_Vbo {
	GLuint						vbo_vert;
	GLuint						vbo_indx;
	
	void init () {
		glGenBuffers(1, &vbo_vert);
		glGenBuffers(1, &vbo_indx);
		
	}
	~Imgui_Vbo () {
		glDeleteBuffers(1, &vbo_vert);
		glDeleteBuffers(1, &vbo_indx);
	}
};
Imgui_Vbo imgui_vbo;

static void imgui_init () {
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	
	io.KeyMap[ ImGuiKey_Tab			 ] = GLFW_KEY_TAB;
	io.KeyMap[ ImGuiKey_LeftArrow	 ] = GLFW_KEY_LEFT;
	io.KeyMap[ ImGuiKey_RightArrow	 ] = GLFW_KEY_RIGHT;
	io.KeyMap[ ImGuiKey_UpArrow		 ] = GLFW_KEY_UP;
	io.KeyMap[ ImGuiKey_DownArrow	 ] = GLFW_KEY_DOWN;
	io.KeyMap[ ImGuiKey_PageUp		 ] = GLFW_KEY_PAGE_UP;
	io.KeyMap[ ImGuiKey_PageDown	 ] = GLFW_KEY_PAGE_DOWN;
	io.KeyMap[ ImGuiKey_Home		 ] = GLFW_KEY_HOME;
	io.KeyMap[ ImGuiKey_End			 ] = GLFW_KEY_END;
	io.KeyMap[ ImGuiKey_Insert		 ] = GLFW_KEY_INSERT;
	io.KeyMap[ ImGuiKey_Delete		 ] = GLFW_KEY_DELETE;
	io.KeyMap[ ImGuiKey_Backspace	 ] = GLFW_KEY_BACKSPACE;
	io.KeyMap[ ImGuiKey_Space		 ] = GLFW_KEY_SPACE;
	io.KeyMap[ ImGuiKey_Enter		 ] = GLFW_KEY_ENTER;
	io.KeyMap[ ImGuiKey_Escape		 ] = GLFW_KEY_ESCAPE;
	io.KeyMap[ ImGuiKey_A			 ] = GLFW_KEY_A;
	io.KeyMap[ ImGuiKey_C			 ] = GLFW_KEY_C;
	io.KeyMap[ ImGuiKey_V			 ] = GLFW_KEY_V;
	io.KeyMap[ ImGuiKey_X			 ] = GLFW_KEY_X;
	io.KeyMap[ ImGuiKey_Y			 ] = GLFW_KEY_Y;
	io.KeyMap[ ImGuiKey_Z			 ] = GLFW_KEY_Z;
	
	Texture2D* font_atlas = new Texture2D("imgui_font_atlas");
	
	shad_imgui = new_shader("imgui.vert", "imgui.frag", {UCOM}, {{IMGUI_TEX_UNIT,"tex"}});
	shad_texture_window_tex =	new_shader("texture_window_tex.vert", "texture_window_tex.frag",		{UCOM, UBOOL("view_col"),UBOOL("view_alpha")}, {{IMGUI_TEX_UNIT,"tex"}});
	
	{
		glGenSamplers(1, &tex_sampler_nearest);
		
		glSamplerParameteri(tex_sampler_nearest, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(tex_sampler_nearest, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	
	imgui_vbo.init();
	{
		u8* pixels;
		s32 w, h;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
		
		font_atlas->alloc_cpu_single_mip(PT_SRGB8_LA8, iv2(w,h));
		
		memcpy(font_atlas->mips[0].data, pixels, font_atlas->mips[0].size);
		
		font_atlas->upload();
		
		io.Fonts->TexID = (void*)font_atlas;
	}
}
static void imgui_destroy () {
    ImGui::DestroyContext();
}

static std::vector<Texture2D*>				imgui_showable_textures;

union texture_window_tex_mode {
	void* raw;
	struct {
		bool view_col : 1;
		bool view_alpha : 1;
	};
};

struct Imgui_Texture_Window {
	bool		open = true;
	
	Texture2D*	tex = nullptr;
	
	bool		view_col = true;
	bool		view_alpha = true;
	
	void imgui () {
		if (ImGui::BeginCombo("texture", tex ? tex->name.c_str() : "<select texture>")) {
			for (auto* t : imgui_showable_textures) {
				bool is_selected = t == tex;
				if (ImGui::Selectable(t->name.c_str(), is_selected))	tex = t;
				if (is_selected)										ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::Checkbox("RGB", &view_col);
		ImGui::SameLine(); ImGui::Checkbox("A", &view_alpha);
		
		if (tex) {
			v2 size = ImGui::GetContentRegionAvailWidth();
			size.y *= tex->dim.y / tex->dim.x;
			
			texture_window_tex_mode mode = {};
			mode.view_col = view_col;
			mode.view_alpha = view_alpha;
			
			ImGui::GetWindowDrawList()->AddCallback(begin_texture_window_tex, mode.raw);
			
			ImGui::Image((ImTextureID)tex, imgui_v2(size));
			
			ImGui::GetWindowDrawList()->AddCallback(end_texture_window_tex, nullptr);
		}
	}
};
static std::vector<Imgui_Texture_Window>	imgui_texture_windows;

static void imgui_texture_window_open_new_window () {
	for (auto& w : imgui_texture_windows) {
		if (!w.open) {
			w.open = true;
			return;
		}
	}
	
	imgui_texture_windows.push_back({/*default constructor*/});
}

static void imgui_begin (f32 dt, iv2 wnd_dim, iv2 mcursor_pos_px, flt mouse_wheel_diff, bool lmb_down, bool rmb_down) {
	ImGuiIO& io = ImGui::GetIO();
	
	io.DisplaySize.x = (flt)wnd_dim.x;
	io.DisplaySize.y = (flt)wnd_dim.y;
	io.DeltaTime = dt;
	io.MousePos.x = (flt)mcursor_pos_px.x;
	io.MousePos.y = (flt)mcursor_pos_px.y;
	io.MouseDown[0] = lmb_down;
	io.MouseDown[1] = rmb_down;
	io.MouseWheel = mouse_wheel_diff;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	
	ImGui::NewFrame();
	ImGui::Begin("Dev options", nullptr);
	
	{
		if (ImGui::Button("show texture window"))
			imgui_texture_window_open_new_window();
		
	}
	
	if (ImGui::TreeNode("Graphics")) {
		graphics_settings.imgui();
		
		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Player")) {
		
		// fly
		// noclip (activate fly so we dont fall through map)
		
		// position
		// orientation
		// velocity
		// 
		
		// collision response
		
		// collision cylinder
		
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Seperate_Camera")) {
		
		// seperate camera checkbox -> on uncheck camera gets moved to player
		// still control player
		
		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("World Generation")) {
		
		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("World")) {
		// Gravity
		
		// Sky colors
		
	}
	
	// texture_windows
	for (s32 i=0; i<(s32)imgui_texture_windows.size(); ++i) {
		auto& w = imgui_texture_windows[i];
		
		if (!w.open) continue;
		if (ImGui::Begin(prints("imgui_texture_window %d", i).c_str(), &w.open, ImGuiWindowFlags_NoScrollbar)) w.imgui();
		ImGui::End();
	}
	
	ImGui::ShowDemoWindow();
}

static bool option_group (strcr name, bool* open=nullptr) {
	return false;
}

static bool option (strcr name, bool* val, bool* open=nullptr) {
	return ImGui::Checkbox(name.c_str(), val);
}
static bool option (strcr name, s32 (*get)(), void (*set)(s32)=nullptr, bool* open=nullptr) {
	return false;
}
static bool option (strcr name, s32* val, bool* open=nullptr) {
	return ImGui::DragInt(name.c_str(), val);
}
static bool option (strcr name, s64* val, bool* open=nullptr) {
	return false;
}
static bool option (strcr name, u64* val, bool* open=nullptr) {
	return false;
}
static bool option (strcr name, f32* val, bool* open=nullptr) {
	return ImGui::DragFloat(name.c_str(), val);
}
static bool option (strcr name, v2* val, bool* open=nullptr) {
	return false;
}
static bool option (strcr name, v3* val, bool* open=nullptr) {
	return false;
}

static bool option_deg (strcr name, f32* val, bool* open=nullptr) {
	return false;
}
static bool option_deg (strcr name, v2* val, bool* open=nullptr) {
	return false;
}


static void begin_texture_window_tex (const ImDrawList* parent_list, const ImDrawCmd* cmd) {
	shad_texture_window_tex->bind();
	
	texture_window_tex_mode mode;
	mode.raw = cmd->UserCallbackData;
	
	shad_texture_window_tex->set_unif("view_col", mode.view_col);
	shad_texture_window_tex->set_unif("view_alpha", mode.view_alpha);
	
	glBindSampler(IMGUI_TEX_UNIT, tex_sampler_nearest);
}
static void end_texture_window_tex (const ImDrawList* parent_list, const ImDrawCmd* cmd) {
	shad_imgui->bind();
	glBindSampler(IMGUI_TEX_UNIT, 0);
}

static void draw_imgui (iv2 wnd_dim) {
	ImGui::End();
	ImGui::Render();
	
	if (!shad_imgui->valid()) return;
	
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_SCISSOR_TEST);
	
	shad_imgui->bind();
	shad_imgui->set_unif("screen_dim", (v2)wnd_dim);
	
	ImDrawData* draw_data = ImGui::GetDrawData();
	
	struct Imgui_Vertex {
		v2		pos_screen;
		v2		uv;
		u32		col;
	};
	
	glBindBuffer(GL_ARRAY_BUFFER, imgui_vbo.vbo_vert);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imgui_vbo.vbo_indx);
	
	GLint pos_loc =	glGetAttribLocation(shad_imgui->prog, "pos_screen");
	GLint uv_loc =	glGetAttribLocation(shad_imgui->prog, "uv");
	GLint col_loc =	glGetAttribLocation(shad_imgui->prog, "col");
	
	glEnableVertexAttribArray(pos_loc);
	glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, sizeof(Imgui_Vertex), (void*)offsetof(Imgui_Vertex, pos_screen));
	
	glEnableVertexAttribArray(uv_loc);
	glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, sizeof(Imgui_Vertex), (void*)offsetof(Imgui_Vertex, uv));
	
	glEnableVertexAttribArray(col_loc);
	glVertexAttribPointer(col_loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Imgui_Vertex), (void*)offsetof(Imgui_Vertex, col));
	
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		auto* cmd_list = draw_data->CmdLists[n];
		
		const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;  // vertex buffer generated by ImGui
		const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;   // index buffer generated by ImGui
		
		auto vertex_size = cmd_list->VtxBuffer.size() * sizeof(ImDrawVert);
		auto index_size = cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx);
		
		glBufferData(GL_ARRAY_BUFFER, vertex_size, NULL, GL_STREAM_DRAW);
		glBufferData(GL_ARRAY_BUFFER, vertex_size, vtx_buffer, GL_STREAM_DRAW);
		
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size, NULL, GL_STREAM_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size, idx_buffer, GL_STREAM_DRAW);
		
		const ImDrawIdx* cur_idx_buffer = idx_buffer;
		
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback) {
				pcmd->UserCallback(cmd_list, pcmd);
			} else {
				bind_texture_unit(IMGUI_TEX_UNIT, (Texture2D*)pcmd->TextureId);
				
				flt y0 = (flt)wnd_dim.y -pcmd->ClipRect.w;
				flt y1 = (flt)wnd_dim.y -pcmd->ClipRect.y;
				
				
				
				glScissor((int)pcmd->ClipRect.x, y0, (int)(pcmd->ClipRect.z -pcmd->ClipRect.x), (int)(y1 -y0));
				
				// Render 'pcmd->ElemCount/3' indexed triangles.
				// By default the indices ImDrawIdx are 16-bits, you can change them to 32-bits if your engine doesn't support 16-bits indices.
				glDrawElements(GL_TRIANGLES, pcmd->ElemCount, GL_UNSIGNED_SHORT,
					(GLvoid const*)((u8 const*)cur_idx_buffer -(u8 const*)idx_buffer));
			}
			cur_idx_buffer += pcmd->ElemCount;
		}
	}
	
	glScissor(0,0, wnd_dim.x,wnd_dim.y);
	
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}
