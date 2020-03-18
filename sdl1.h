#pragma once

#include "E:\dev_ffmpeg\SDL-devel-1.2.15-VC\SDL-1.2.15\include\SDL.h"
#include "E:\dev_ffmpeg\SDL-devel-1.2.15-VC\SDL-1.2.15\include\SDL_thread.h"

#define SDL_LIB_PATH R"(E:\dev_ffmpeg\SDL-devel-1.2.15-VC\SDL-1.2.15\lib\x86\)"

#pragma comment(lib, SDL_LIB_PATH "SDLmain.lib")
#pragma comment(lib, SDL_LIB_PATH "SDL.lib")

// undef SDL's micro
#ifdef main
#undef main
#endif
