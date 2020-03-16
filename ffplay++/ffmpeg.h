#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#pragma warning(disable:4996)

#include "config.h"

extern "C" {
#include "compat/va_copy.h"
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
#include "libavresample/avresample.h"
#include "libpostproc/postprocess.h"
#include "libavutil/attributes.h"
#include "libavutil/bprint.h"
#include "libavutil/display.h"
#include "libavutil/libm.h"
#include "libavutil/cpu.h"
#include "libavutil/ffversion.h"
#include "libavutil/version.h"

#if CONFIG_NETWORK
#include "libavformat/network.h"
#endif

#if CONFIG_AVFILTER
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif
}


#define FFMPEG_LIB_PATH R"(E:\dev_ffmpeg\ffmpeg-20200311-36aaee2-win32-dev\lib\)"

// According to http://ffmpeg.org/faq.html
// You must specify the libraries in dependency order:
// -lavdevice must come before -lavformat
// -lavutil must come after everything else

#pragma comment(lib, FFMPEG_LIB_PATH "avcodec.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avdevice.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avfilter.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avformat.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "swresample.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "swscale.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avutil.lib")

#ifdef _WIN32
#undef main /* We don't want SDL to override our main() */
#endif

