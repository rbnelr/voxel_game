
#include "font.hpp"

static Shader*		shad_font;
static Vbo			vbo_overlay_font;

static font::Font*	overlay_font;
static f32			overlay_font_line_y;

static void begin_overlay_text () {
	vbo_overlay_font.clear();
	
	overlay_font_line_y = overlay_font->ascent_plus_gap;
}
static void overlay_line (strcr s, v3 col=srgb(255,220,120)*0.8f, v3 outline_col=0, s32 cursor_pos=-1) {
	overlay_font->emit_line(&vbo_overlay_font.vertecies, &overlay_font_line_y, shad_font, utf8_to_utf32(s), v4(col,1), v4(outline_col,1), cursor_pos);
}

//
static v3 option_col =			255;
static v3 option_highl_col =	srgb(255,240,90)*0.95f;
static v3 option_edit_col =		srgb(255,255,90)*1.0f;

static enum { OPT_OVERLAY_DISABLED=0, OPT_SELECTING, OPT_EDITING } opt_mode = OPT_SELECTING;
static bool opt_value_edit_flag = false;
static bool opt_toggle_open = false;

static s32 selected_option = 0;
static s32 cur_option;

static str opt_val_str;
static s32 opt_cur_char = 0;

static void begin_options () {
	cur_option = 0;
}

static bool parse_s32 (strcr str, s32* val) {
	char* end = nullptr;
	*val = strtol(str.c_str(), &end, 10);
	return end;
}
static bool parse_f32 (strcr str, f32* val) {
	char* end = nullptr;
	*val = strtof(str.c_str(), &end);
	return end;
}
static bool parse_v2 (strcr str, v2* val) {
	using namespace parse_n;
	
	char* cur = (char*)str.c_str();
	
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
static bool parse_v3 (strcr str, v3* val) {
	using namespace parse_n;
	
	char* cur = (char*)str.c_str();
	
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

static bool option_group (strcr name, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	
	bool option_highl = opt_mode && cur_option++ == selected_option;
	bool option_edit = option_highl && opt_mode == OPT_EDITING;
	if (option_highl && opt_toggle_open) {
		opt_toggle_open = false;
		if (open) *open = !(*open);
	}
	
	if (option_edit && opt_value_edit_flag) { // started editing
		opt_value_edit_flag = false;
	}
	
	v3 col = option_col;
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

static bool _option (strcr name, str& opt_str, bool* open) {
	bool option_highl = opt_mode && cur_option++ == selected_option;
	bool option_edit = option_highl && opt_mode == OPT_EDITING;
	if (option_highl) if (open) *open = !(*open);
	
	if (option_edit && opt_value_edit_flag) { // started editing
		opt_val_str = opt_str;
	}
	
	if (option_edit) {
		opt_str = opt_val_str;
	}
	
	v3 col = option_col;
	if (option_highl) {
		col = option_edit ? option_edit_col : option_highl_col;
	}
	
	overlay_line(prints("%s: %s%s", name.c_str(), opt_str.c_str(), open && !(*open) ? " {...}":""), col,0, option_edit ? name.size() +2 +opt_cur_char : -1);
	
	if (option_highl && !option_edit && opt_value_edit_flag) { // finished editing
		return true;
	}
	return false;
}

static bool option (strcr name, bool* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%1d", *val ? 1 : 0);
	s32 tmp;
	if (_option(name, opt_str, open))	if (parse_s32(opt_val_str, &tmp)) {
		*val = tmp != 0;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, s32 (*get)(), void (*set)(s32)=nullptr, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%4d", get());
	s32 tmp;
	if (_option(name, opt_str, open))	if (parse_s32(opt_val_str, &tmp) && set) {
		set(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, s32* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%4d", *val);
	s32 tmp;
	if (_option(name, opt_str, open))	if (parse_s32(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, f32* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%8.7g", *val);
	f32 tmp;
	if (_option(name, opt_str, open))	if (parse_f32(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, v2* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%8.7g, %8.7g", val->x,val->y);
	v2 tmp;
	if (_option(name, opt_str, open))	if (parse_v2(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option (strcr name, v3* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%8.7g, %8.7g, %8.7g", val->x,val->y,val->z);
	v3 tmp;
	if (_option(name, opt_str, open))	if (parse_v3(opt_val_str, &tmp)) {
		*val = tmp;
		opt_str = opt_val_str;
		return true;
	}
	return false;
}

static bool option_deg (strcr name, f32* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%8.7g", to_deg(*val));
	f32 tmp;
	if (_option(name, opt_str, open))	if (parse_f32(opt_val_str, &tmp)) {
		*val = to_rad(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
static bool option_deg (strcr name, v2* val, bool* open=nullptr) {
	if (opt_mode == OPT_OVERLAY_DISABLED) return false;
	str opt_str = prints("%8.7g, %8.7g", to_deg(val->x),to_deg(val->y));
	v2 tmp;
	if (_option(name, opt_str, open))	if (parse_v2(opt_val_str, &tmp)) {
		*val = to_rad(tmp);
		opt_str = opt_val_str;
		return true;
	}
	return false;
}
