
#if RZ_PLATF == RZ_PLATF_GENERIC_WIN && RZ_DBG
	
	#if 0
		#define _USING_V110_SDK71_ 1
		#define WIN32_LEAN_AND_MEAN 1
		#include "windows.h"
	#else
		#define WINAPI __stdcall
		
		typedef unsigned long	DWORD;
		typedef int				BOOL;
		
		// For debugging
		extern "C" __declspec(dllimport) BOOL WINAPI IsDebuggerPresent(void);
		
		extern "C" __declspec(dllimport) void WINAPI Sleep(
		  DWORD dwMilliseconds
		);
	#endif
	
	static void dbg_sleep (f32 sec) {
		Sleep( (DWORD)(sec * 1000.0f) );
	}
	
	#define IS_DEBUGGER_PRESENT()			IsDebuggerPresent()
	#define DBGBREAK_IF_DEBUGGER_PRESENT	if (IS_DEBUGGER_PRESENT()) { DBGBREAK; }
	#define BREAK_IF_DEBUGGING_ELSE_STALL	if (IS_DEBUGGER_PRESENT()) { DBGBREAK; } else { dbg_sleep(0.1f); }
	
#endif

#if RZ_DBG
	
	static void _failed_dbg_assert (cstr cond) {
		fprintf(stderr,	ANSI_COLOUR_CODE_RED
						"dbg_assert failed!\n"
						"  \"%s\"\n" ANSI_COLOUR_CODE_NC, cond);
		
		BREAK_IF_DEBUGGING_ELSE_STALL;
	}
	static void _failed_dbg_assert (cstr cond, cstr msg_format, ...) {
		
		fprintf(stderr,	ANSI_COLOUR_CODE_RED
						"dbg_assert failed!\n"
						"  \"%s\"\n  ", cond);
		
		va_list vl;
		va_start(vl, msg_format);
		
		vfprintf(stderr, msg_format, vl);
		
		va_end(vl);
		
		fprintf(stderr,	"\n" ANSI_COLOUR_CODE_NC);
		
		BREAK_IF_DEBUGGING_ELSE_STALL;
	}
	
	#if RZ_COMP == RZ_COMP_MSVC
		#define MSVC_FUNC_NAME ":" __FUNCTION__
	#else
		#define MSVC_FUNC_NAME
	#endif
	
	#define dbg_assert(cond, ...)	if (!(cond)) _failed_dbg_assert(__FILE__ ":" TO_STRING(__LINE__) MSVC_FUNC_NAME ":\n    '" STRINGIFY(cond), ##__VA_ARGS__)
	
#else
	
	#define dbg_assert(cond, ...)	do { (void)(cond); } while (0)
	
#endif

static void dbg_warning (cstr format, ...) {
	std::string str;
	
	va_list vl;
	va_start(vl, format);
	
	_prints(&str, format, vl);
	
	va_end(vl);
	
	printf(ANSI_COLOUR_CODE_YELLOW "%s\n" ANSI_COLOUR_CODE_NC, str.c_str());
}