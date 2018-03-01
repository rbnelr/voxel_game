@echo off & setlocal enabledelayedexpansion
	
	cls
	color 07
	
	rem "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
	rem "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64"
	
	rem MSVC should be always in path
	set LLVM=D:/Cproj/_llvm/LLVM/bin/
	if [!GCC!] == [] set GCC=D:/pt_proj/tdm_gcc/bin/
	
	set ROOT=%~dp0
	set SRC=!ROOT!src/
	set DEPS=!ROOT!deps/
	set GLFW=!DEPS!glfw-3.2.1.bin.WIN64/
	set GLAD=!DEPS!glad/
	set STB=!DEPS!stb/
	
	set GLFW_SRC=!DEPS!glfw-3.3/
	
	rem can do standart compile all link all build with these
	set GLFW_SOURCES=-D_GLFW_WIN32=1 ^
		!GLFW_SRC!src/init.c ^
		!GLFW_SRC!src/window.c ^
		!GLFW_SRC!src/context.c ^
		!GLFW_SRC!src/input.c ^
		!GLFW_SRC!src/monitor.c ^
		!GLFW_SRC!src/wgl_context.c ^
		!GLFW_SRC!src/egl_context.c ^
		!GLFW_SRC!src/win32_init.c ^
		!GLFW_SRC!src/win32_monitor.c ^
		!GLFW_SRC!src/win32_time.c ^
		!GLFW_SRC!src/win32_window.c ^
		!GLFW_SRC!src/win32_tls.c ^
		!GLFW_SRC!src/win32_joystick.c ^
		!GLFW_SRC!src/vulkan.c
	
	set GLFW_OBJECTS=^
		!ROOT!init.o ^
		!ROOT!window.o ^
		!ROOT!context.o ^
		!ROOT!input.o ^
		!ROOT!monitor.o ^
		!ROOT!wgl_context.o ^
		!ROOT!egl_context.o ^
		!ROOT!win32_init.o ^
		!ROOT!win32_monitor.o ^
		!ROOT!win32_time.o ^
		!ROOT!win32_window.o ^
		!ROOT!win32_tls.o ^
		!ROOT!win32_joystick.o ^
		!ROOT!vulkan.o
	
	rem this is better (build all of glfw thats needed for windows as one source file)
	set GLFW_ONE_SRC_FILE=!SRC!glfw_one_source_file.c
	
	set func=vs
	if not [%1] == []		set func=%1
	
	set mode=dbg
	if [%2] == [dbg]		set mode=dbg
	if [%2] == [opt]		set mode=opt
	if [%2] == [release]	set mode=release
	
	set proj=voxel_game
	if not [%3] == []		set proj=%3
	
	set succ=0
	call :%func%
	
	if [succ] == [0] (
		echo fail.
	) else (
		echo success.
	)
	exit /b
rem /main

:vs
	del !ROOT!!proj!.exe
	
	if [!mode!] == [dbg] (
		set dbg=/Od /EHsc /Ob1 /MDd /Zi /DRZ_DBG=1 /DRZ_DEV=1
	) else if [!mode!] == [opt] (
		set dbg=/O2 /EHsc /Ob2 /MD /Zi /Zo /Oi /DRZ_DBG=0 /DRZ_DEV=1
	) else if [!mode!] == [release] (
		set dbg=/O2 /EHsc /Ob2 /MD /Zi /Zo /Oi /DRZ_DBG=0 /DRZ_DEV=0
	)
	
	set opt=!dbg! /fp:fast /GS-
	
	set warn=/wd4577 /wd4005
	rem /wd4005 sal_supp.h(57): warning C4005: '__useHeader': macro redefinition in windows header
	rem /wd4577 noexcept used with no exceptions enabled
	
	rem /showIncludes
	
	rem glfw dll
	rem cl.exe -nologo /source-charset:utf-8 /DRZ_PLATF=1 /DRZ_ARCH=1 !opt! !warn! /I!SRC!include /I!GLFW!include /I!GLAD! /I!STB! !SRC!!proj!.cpp /Fe!ROOT!!proj!.exe /link KERNEL32.lib OPENGL32.lib !GLFW!lib-vc2015/glfw3dll.lib /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OPT:REF
	
	rem glfw static link
	cl.exe -nologo !opt! !warn! /I!GLFW_SRC!include /I!GLFW_SRC!src /c !GLFW_ONE_SRC_FILE!
	
	cl.exe -nologo /DRZ_PLATF=1 /DRZ_ARCH=1 !opt! !warn! /I!SRC!include /I!GLAD! /I!STB! /Ideps/noise/ /I!GLFW_SRC!include !SRC!!proj!.cpp glfw_one_source_file.obj /Fe!ROOT!!proj!.exe /link KERNEL32.lib USER32.lib GDI32.lib OPENGL32.lib SHELL32.lib WINMM.lib /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OPT:REF
	
	rem WINMM.lib for simple audio playing (https://msdn.microsoft.com/en-us/library/windows/desktop/dd743680(v=vs.85).aspx)
	
	del *.obj
	
	exit /b
rem /vs

:gcc
	del !ROOT!!proj!.exe
	
	if [!mode!] == [dbg] (
		set dbg=-O0 -DRZ_DBG=1 -DRZ_DEV=1
	) else if [!mode!] == [opt] (
		set dbg=-O3 -DRZ_DBG=1 -DRZ_DEV=1
	) else if [!mode!] == [release] (
		set dbg=-O3 -DRZ_DBG=0 -DRZ_DEV=0
	)
	
	set opt=!dbg! -mmmx -msse -msse2
	rem -msse3 -mssse3 -msse4.1 -msse4.2 -mpopcnt
	
	set warn=-Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-tautological-compare
	rem -fmax-errors=5
	rem -Wno-unused-variable
	rem -Wno-unused-function
	rem -Wtautological-compare		constant if statements
	
	rem !GCC!g++ -std=c++11 -m64 -DRZ_PLATF=1 -DRZ_ARCH=1 !opt! !warn! -I!SRC!include -I!GLFW!include -I!GLAD! -I!STB! -o !ROOT!!proj!.exe !SRC!!proj!.cpp -L!GLFW!lib-mingw-w64 -lglfw3dll
	
	rem glfw static link
	!GCC!gcc -m64 !opt! !warn! -I!GLFW_SRC!include -I!GLFW_SRC!src -c !GLFW_ONE_SRC_FILE!
	
	!GCC!g++ -std=c++11 -m64 -DRZ_PLATF=1 -DRZ_ARCH=1 !opt! !warn! -I!SRC!include -I!GLAD! -I!STB! -Ideps/noise/ -I!GLFW_SRC!include -o !ROOT!!proj!.exe !SRC!!proj!.cpp glfw_one_source_file.o -lKERNEL32 -lUSER32 -lGDI32 -lOPENGL32 -lSHELL32
	
	del *.o
	
	exit /b
rem /llvm

:llvm
	del !ROOT!!proj!.exe
	
	if [!mode!] == [dbg] (
		set dbg=/Od /EHsc /Ob1 /MDd /Zi /DRZ_DBG=1 /DRZ_DEV=1
	) else if [!mode!] == [opt] (
		set dbg=/O2 /EHsc /Ob2 /MD /Zi /Zo /Oi /DRZ_DBG=0 /DRZ_DEV=1
	) else if [!mode!] == [release] (
		set dbg=/O2 /EHsc /Ob2 /MD /Zi /Zo /Oi /DRZ_DBG=0 /DRZ_DEV=0
	)
	
	set opt=!dbg! /fp:fast /GS-
	
	set warn=/wd4577 /wd4005 -D_CRT_SECURE_NO_WARNINGS
	rem /wd4005 sal_supp.h(57): warning C4005: '__useHeader': macro redefinition in windows header
	rem /wd4577 noexcept used with no exceptions enabled
	
	rem /showIncludes
	
	rem glfw dll
	rem cl.exe -nologo /source-charset:utf-8 /DRZ_PLATF=1 /DRZ_ARCH=1 !opt! !warn! /I!SRC!include /I!GLFW!include /I!GLAD! /I!STB! !SRC!!proj!.cpp /Fe!ROOT!!proj!.exe /link KERNEL32.lib OPENGL32.lib !GLFW!lib-vc2015/glfw3dll.lib /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OPT:REF
	
	rem glfw static link
	!LLVM!clang-cl.exe -m64 -nologo !opt! !warn! /I!GLFW_SRC!include /I!GLFW_SRC!src /c !GLFW_ONE_SRC_FILE!
	
	!LLVM!clang-cl.exe -m64 -nologo /source-charset:utf-8 /DRZ_PLATF=1 /DRZ_ARCH=1 !opt! !warn! /I!SRC!include /I!GLAD! /I!STB! /I!GLFW_SRC!include !SRC!!proj!.cpp glfw_one_source_file.obj /Fe!ROOT!!proj!.exe /link KERNEL32.lib USER32.lib GDI32.lib OPENGL32.lib SHELL32.lib /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /OPT:REF
	
	del *.obj
	
	exit /b
rem /llvm

:disasm
	dumpbin /ALL /DISASM /SYMBOLS /out:!ROOT!!proj!.exe.asm.tmp !ROOT!!proj!.exe
	undname !ROOT!!proj!.exe.asm.tmp > !ROOT!!proj!.exe.asm
	del !ROOT!!proj!.exe.asm.tmp
	
	rem dumpbin /ALL /DISASM /SYMBOLS /out:!ROOT!!proj!.exe.asm !ROOT!!proj!.exe
	
	exit /b
rem /a

rem :copy_built
rem 	copy D:\Cproj\lib_engine\src\glfw-3.2.1.src\src\Release\glfw3.lib				D:\Cproj\lib_engine\src\glfw-3.2.1.built\glfw3_release.lib
rem 	copy D:\Cproj\lib_engine\src\glfw-3.2.1.src\src\glfw.dir\Release\glfw.pdb		D:\Cproj\lib_engine\src\glfw-3.2.1.built\glfw3_release.pdb
rem 	
rem 	copy D:\Cproj\lib_engine\src\glfw-3.2.1.src\src\Debug\glfw3.lib					D:\Cproj\lib_engine\src\glfw-3.2.1.built\glfw3_dbg.lib
rem 	copy D:\Cproj\lib_engine\src\glfw-3.2.1.src\src\glfw.dir\Debug\glfw.pdb			D:\Cproj\lib_engine\src\glfw-3.2.1.built\glfw3_dbg.pdb
rem 	
rem 	exit /b
rem rem /a
