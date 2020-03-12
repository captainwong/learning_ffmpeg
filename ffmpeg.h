#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#pragma warning(disable:4996)

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
}

#define FFMPEG_LIB_PATH R"(E:\dev_ffmpeg\ffmpeg-20200311-36aaee2-win32-dev\lib\)"

#pragma comment(lib, FFMPEG_LIB_PATH "avcodec.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avformat.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avutil.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "swscale.lib")

