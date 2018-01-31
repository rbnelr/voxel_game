
//
static GLFWwindow*	wnd;
static void toggle_fullscreen ();

static void glfw_key_event (GLFWwindow* window, int key, int scancode, int action, int mods);
static void glfw_mouse_button_event (GLFWwindow* window, int button, int action, int mods);
static void glfw_mouse_scroll (GLFWwindow* window, double xoffset, double yoffset);
static void glfw_cursor_move_relative (GLFWwindow* window, double dx, double dy);

static void start_mouse_look () {
	glfwSetInputMode(wnd, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}
static void stop_mouse_look () {
	glfwSetInputMode(wnd, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

//

int vsync_mode = 1;
static void set_vsync (int mode) {
	vsync_mode = mode;
	glfwSwapInterval(mode);
}

struct Rect {
	iv2 pos;
	iv2 dim;
};

static bool			fullscreen = false;

static GLFWmonitor*	primary_monitor;
static Rect			window_positioning;

#define WINDOW_POSITIONING_FILE	"saves/window_positioning.bin"

static void get_window_positioning () {
	auto& r = window_positioning;
	glfwGetWindowPos(wnd, &r.pos.x,&r.pos.y);
	glfwGetWindowSize(wnd, &r.dim.x,&r.dim.y);
}
static void position_window () {
	auto& r = window_positioning;
	
	if (fullscreen) {
		auto* vm = glfwGetVideoMode(primary_monitor);
		glfwSetWindowMonitor(wnd, primary_monitor, 0,0, vm->width,vm->height, vm->refreshRate);
		
	} else {
		glfwSetWindowMonitor(wnd, NULL, r.pos.x,r.pos.y, r.dim.x,r.dim.y, GLFW_DONT_CARE);
	}
	
}
static void toggle_fullscreen () {
	if (!fullscreen) {
		get_window_positioning();
	}
	fullscreen = !fullscreen;
	
	position_window();
	
	set_vsync(vsync_mode);
}

#define GL_VER_MAJOR 3
#define GL_VER_MINOR 3

#define GL_VAOS_REQUIRED	GL_VER_MAJOR >= 3 && GL_VER_MINOR >= 2

#if GL_VAOS_REQUIRED
GLuint	vao;
#endif

static void glfw_error_proc (int err, const char* msg) {
	dbg_assert(false, "GLFW Error! 0x%x '%s'\n", err, msg);
}

static void APIENTRY ogl_debuproc (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, cstr message, void const* userParam) {
	
	//if (source == GL_DEBUG_SOURCE_SHADER_COMPILER_ARB) return;
	
	switch (id) {
		case 131185: // Buffer detailed info (where the memory lives which is supposed to depend on the usage hint)
		case 1282: // using shader that was not compiled successfully
		
		case 2: // API_ID_RECOMPILE_FRAGMENT_SHADER performance warning has been generated. Fragment shader recompiled due to state change.
		case 131218: // Program/shader state performance warning: Fragment shader in program 3 is being recompiled based on GL state.
		
		//case 131154: // Pixel transfer sync with rendering warning
		
		//case 1282: // Wierd error on notebook when trying to do texture streaming
		//case 131222: // warning with unused shadow samplers ? (Program undefined behavior warning: Sampler object 0 is bound to non-depth texture 0, yet it is used with a program that uses a shadow sampler . This is undefined behavior.), This might just be unused shadow samplers, which should not be a problem
		//case 131218: // performance warning, because of shader recompiling based on some 'key'
			return; 
	}
	
	cstr src_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_SOURCE_API_ARB:				src_str = "GL_DEBUG_SOURCE_API_ARB";				break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:		src_str = "GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB";		break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:	src_str = "GL_DEBUG_SOURCE_SHADER_COMPILER_ARB";	break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:		src_str = "GL_DEBUG_SOURCE_THIRD_PARTY_ARB";		break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB:		src_str = "GL_DEBUG_SOURCE_APPLICATION_ARB";		break;
		case GL_DEBUG_SOURCE_OTHER_ARB:				src_str = "GL_DEBUG_SOURCE_OTHER_ARB";				break;
	}
	
	cstr type_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_TYPE_ERROR_ARB:				type_str = "GL_DEBUG_TYPE_ERROR_ARB";				break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:	type_str = "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB";	break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:	type_str = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB";	break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB:			type_str = "GL_DEBUG_TYPE_PORTABILITY_ARB";			break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB:			type_str = "GL_DEBUG_TYPE_PERFORMANCE_ARB";			break;
		case GL_DEBUG_TYPE_OTHER_ARB:				type_str = "GL_DEBUG_TYPE_OTHER_ARB";				break;
	}
	
	cstr severity_str = "<unknown>";
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH_ARB:			severity_str = "GL_DEBUG_SEVERITY_HIGH_ARB";		break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB:			severity_str = "GL_DEBUG_SEVERITY_MEDIUM_ARB";		break;
		case GL_DEBUG_SEVERITY_LOW_ARB:				severity_str = "GL_DEBUG_SEVERITY_LOW_ARB";			break;
	}
	
	if (severity == GL_DEBUG_SEVERITY_HIGH_ARB && 0) {
		dbg_assert(false, "OpenGL debug proc: severity: %s src: %s type: %s id: %d  %s\n",
				severity_str, src_str, type_str, id, message);
	} else {
		dbg_warning("OpenGL debug proc: severity: %s src: %s type: %s id: %d  %s\n",
				severity_str, src_str, type_str, id, message);
	}
}

static void platform_setup_context_and_open_window (cstr inital_wnd_title, iv2 default_wnd_dim) {
	
	glfwSetErrorCallback(glfw_error_proc);
	
	dbg_assert( glfwInit() );
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,	GL_VER_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,	GL_VER_MINOR);
	if (GL_VER_MAJOR >= 3 && GL_VER_MINOR >= 2) {
		glfwWindowHint(GLFW_OPENGL_PROFILE,		GLFW_OPENGL_CORE_PROFILE);
	}
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT,	1);
	
	{ // open and postion window
		primary_monitor = glfwGetPrimaryMonitor();
		
		bool pos_restored = read_entire_file(WINDOW_POSITIONING_FILE, &window_positioning, sizeof(window_positioning));
		if (!pos_restored) {
			window_positioning.dim = default_wnd_dim;
		}
		
		glfwWindowHint(GLFW_VISIBLE, 0);
		
		wnd = glfwCreateWindow(window_positioning.dim.x,window_positioning.dim.y, inital_wnd_title, NULL, NULL);
		dbg_assert(wnd);
		
		if (pos_restored) {
			printf("window_positioning restored from \"" WINDOW_POSITIONING_FILE "\".\n");
		} else {
			get_window_positioning();
			printf("window_positioning could not be restored from \"" WINDOW_POSITIONING_FILE "\", using default.\n");
		}
		
		position_window();
		
		glfwShowWindow(wnd);
	}
	
	glfwSetKeyCallback(wnd,					glfw_key_event);
	glfwSetMouseButtonCallback(wnd,			glfw_mouse_button_event);
	glfwSetScrollCallback(wnd,				glfw_mouse_scroll);
	glfwSetCursorPosRelativeCallback(wnd,	glfw_cursor_move_relative);
	
	glfwMakeContextCurrent(wnd);
	
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
	if (GLAD_GL_ARB_debug_output) {
		glDebugMessageCallbackARB(ogl_debuproc, 0);
		//DEBUG_OUTPUT_SYNCHRONOUS_ARB this exists -> if ogl_debuproc needs to be thread safe
	}
	
	if (GL_VAOS_REQUIRED) {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
	}
	
}
static void platform_terminate () {
	
	{
		if (!fullscreen) {
			get_window_positioning();
		}
		
		bool pos_saved = overwrite_file(WINDOW_POSITIONING_FILE, &window_positioning, sizeof(window_positioning));
		if (pos_saved) {
			printf("window_positioning saved to \"" WINDOW_POSITIONING_FILE "\".\n");
		} else {
			dbg_warning("could not write \"" WINDOW_POSITIONING_FILE "\", window_positioning won't be restored on next launch.");
		}
	}
	
	glfwDestroyWindow(wnd);
	glfwTerminate();
}
