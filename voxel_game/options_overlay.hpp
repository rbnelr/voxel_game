
#include "font.hpp"

static Shader*		shad_font;
static Vbo			vbo_overlay_font;

static font::Font*	overlay_font;
static float			overlay_font_line_y;

static void begin_overlay_text () {
	vbo_overlay_font.clear();
	
	overlay_font_line_y = overlay_font->ascent_plus_gap;
}
static void overlay_line (std::string const& s, float3 col=srgb(255,220,120)*0.8f, float3 outline_col=0, int cursor_pos=-1) {
	overlay_font->emit_line(&vbo_overlay_font.vertecies, &overlay_font_line_y, shad_font, utf8_to_utf32(s), float4(col,1), float4(outline_col,1), cursor_pos);
}

//
static float3 option_col =			255;
static float3 option_highl_col =	srgb(255,240,90)*0.95f;
static float3 option_edit_col =		srgb(255,255,90)*1.0f;

static enum { OPT_OVERLAY_DISABLED=0, OPT_SELECTING, OPT_EDITING } opt_mode = OPT_SELECTING;
static bool opt_value_edit_flag = false;
static bool opt_toggle_open = false;

static int selected_option = 0;
static int cur_option;

static std::string opt_val_str;
static int opt_cur_char = 0;

static void begin_options () {
	cur_option = 0;
}

static bool parse_sint (std::string const& std::string, int* val) {
	char* end = nullptr;
	*val = strtol(std::string.c_str(), &end, 10);
	return end;
}
static bool parse_sint (std::string const& std::string, int64_t* val) {
	char* end = nullptr;
	*val = strtoll(std::string.c_str(), &end, 10);
	return end;
}
static bool parse_f32 (std::string const& std::string, float* val) {
	char* end = nullptr;
	*val = strtof(std::string.c_str(), &end);
	return end;
}
static bool parse_v2 (std::string const& std::string, float2* val) {
	using namespace parse_n;
	
	char* cur = (char*)std::string.c_str();
	
	whitespace(&cur);
	
	val->x = strtof(cur, &cur);
	if (!cur) return false;
	
	whitespace(&cur);
	char_(&cur, ',');
	
	whitespace(&cur);
	
	val->y = strtof(cur, &cur);
	if (!cur) return false;
	
	return true;
}
static bool parse_v3 (std::string const& std::string, float3* val) {
	using namespace parse_n;
	
	char* cur = (char*)std::string.c_str();
	
	whitespace(&cur);
	
	val->x = strtof(cur, &cur);
	if (!cur) return false;
	
	whitespace(&cur);
	char_(&cur, ',');
	
	whitespace(&cur);
	
	val->y = strtof(cur, &cur);
	if (!cur) return false;
	
	whitespace(&cur);
	char_(&cur, ',');
	
	whitespace(&cur);
	
	val->z = strtof(cur, &cur);
	if (!cur) return false;
	
	return true;
}

static bool option_group (std::string const& name, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	
	bool option_highl = opt_mode && cur_option++ == selected_option;
	bool option_edit = option_highl && opt_mode == OPT_EDITING;
	if (option_highl && opt_toggle_open) {
		if (open) *open = !(*open);
	}
	
	if (option_edit && opt_value_edit_flag) { // started editing
		opt_value_edit_flag = false;
	}
	
	float3 col = option_col;
	if (option_highl) {
		col = option_edit ? option_edit_col : option_highl_col;
	}
	
	overlay_line(prints("%s:%s", name.c_str(), open && !(*open) ? " {...}":""), col,0, option_edit ? name.size() +2 +opt_cur_char : -1);
	
	if (option_highl && !option_edit && opt_value_edit_flag) { // finished editing
		opt_value_edit_flag = false;
		return true;
	}
	return false;
}

static bool _option (std::string const& name, std::string& opt_str, bool* open) {
	bool option_highl = opt_mode && cur_option++ == selected_option;
	bool option_edit = option_highl && opt_mode == OPT_EDITING;
	if (option_highl && opt_toggle_open) if (open) *open = !(*open);
	
	if (option_edit && opt_value_edit_flag) { // started editing
		opt_val_str = opt_str;
	}
	
	if (option_edit) {
		opt_str = opt_val_str;
	}
	
	float3 col = option_col;
	if (option_highl) {
		col = option_edit ? option_edit_col : option_highl_col;
	}
	
	size_t name_w = 30;
	
	overlay_line(prints("%s: %*s%s", name.c_str(), name_w -name.size() -2 +opt_str.size(), opt_str.c_str(), open && !(*open) ? " {...}":""), col,0, option_edit ? max(name.size() +2, (size_t)name_w) +opt_cur_char : -1);
	
	if (option_highl && !option_edit && opt_value_edit_flag) { // finished editing
		return true;
	}
	return false;
}

static bool option (std::string const& name, bool* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8d", *val ? 1 : 0);
	int64_t tmp;
	if (_option(name, opt_str, open) && parse_sint(opt_val_str, &tmp)) {
		*val = tmp != 0;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (std::string const& name, int (*get)(), void (*set)(int)=nullptr, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8d", (int)get());
	int64_t tmp;
	if (_option(name, opt_str, open) && parse_sint(opt_val_str, &tmp) && set) {
		set((int)tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (std::string const& name, int* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8d", *val);
	int tmp;
	if (_option(name, opt_str, open) && parse_sint(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (std::string const& name, int64_t* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8d", (int)*val);
	int64_t tmp;
	if (_option(name, opt_str, open) && parse_sint(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (std::string const& name, uint64_t* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8d", (uint32_t)*val);
	int64_t tmp;
	if (_option(name, opt_str, open) && parse_sint(opt_val_str, &tmp) && safe_cast(uint64_t, tmp)) {
		*val = (uint64_t)tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (std::string const& name, float* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8.7g", *val);
	float tmp;
	if (_option(name, opt_str, open) && parse_f32(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (std::string const& name, float2* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8.7g, %8.7g", val->x,val->y);
	float2 tmp;
	if (_option(name, opt_str, open) && parse_v2(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (std::string const& name, float3* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8.7g, %8.7g, %8.7g", val->x,val->y,val->z);
	float3 tmp;
	if (_option(name, opt_str, open) && parse_v3(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}

static bool option_deg (std::string const& name, float* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8.7g", to_deg(*val));
	float tmp;
	if (_option(name, opt_str, open) && parse_f32(opt_val_str, &tmp)) {
		*val = to_rad(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option_deg (std::string const& name, float2* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	std::string opt_str = prints("%8.7g, %8.7g", to_deg(val->x),to_deg(val->y));
	float2 tmp;
	if (_option(name, opt_str, open) && parse_v2(opt_val_str, &tmp)) {
		*val = to_rad(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
