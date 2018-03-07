#pragma once
#include <string>
#include <cstdarg>

#define ANSI_COLOUR_CODE_RED	"\033[1;31m"
#define ANSI_COLOUR_CODE_YELLOW	"\033[1;33m"
#define ANSI_COLOUR_CODE_NC		"\033[0m"

// Printf that outputs to a std::string
void vprints (std::string* s, cstr format, va_list vl);
void prints (std::string* s, cstr format, ...);
std::string prints (cstr format, ...);
