#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// supress deprecated errors
// #pragma warning(disable:4996)

// Add ffmpeg path to your project's C++ path
// e.g. E:\dev_ffmpeg\ffmpeg-20200311-36aaee2-win32-dev\

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#define FFMPEG_LIB_PATH R"(E:\dev_ffmpeg\ffmpeg-20200311-36aaee2-win32-dev\lib\)"

#pragma comment(lib, FFMPEG_LIB_PATH "avcodec.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avformat.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "swscale.lib")
#pragma comment(lib, FFMPEG_LIB_PATH "avutil.lib")

// av_err2str cannot compile
inline void fprintfAVErrorString(int err, const char* prefix = "error") {
	char str[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, err);
	fprintf(stderr, prefix);
	fprintf(stderr, ": %s\n", str);
}


/**
* @brief Open codec context with given format context and media type
* @param[in] fmtCtx Opened AVFormatContext
* @param[in] type AVMediaType
* @param[in|out] stream_idx If ok, identifies the stream index of type
* @return AVCodecContext* Return nullptr if failed, otherwise OK
* @note It's caller's duty to free the returned context if not null
* @note Equivalent snippet is listed below.
*/
inline AVCodecContext* openCodecContext(AVFormatContext* fmtCtx, AVMediaType type, int& streamIdx)
{
	AVCodecContext* codecContext = nullptr;

	do {
		// find the best stream
		AVCodec* codec = nullptr;
		streamIdx = av_find_best_stream(fmtCtx, type, -1, -1, &codec, 0);
		if (streamIdx < 0) {
			fprintf(stderr, "Counld not find %s stream\n", av_get_media_type_string(type));
			break;
		}

		// allocate context
		if (!(codecContext = avcodec_alloc_context3(codec))) {
			fprintf(stderr, "Failed to allocate %s codec\n", av_get_media_type_string(type));
			break;
		}

		// copy context
		if (avcodec_parameters_to_context(codecContext, fmtCtx->streams[streamIdx]->codecpar)) {
			fprintf(stderr, "Failed to copy %s codec parameters to codec context\n", av_get_media_type_string(type));
			break;
		}

		// open codec
		if (avcodec_open2(codecContext, codec, nullptr) < 0) {
			fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
			break;
		}

		return codecContext;

	} while (0);

	if (codecContext) {
		avcodec_free_context(&codecContext);
	}

	return nullptr;
}

/*
Equivalent old style

// find the first video stream
int videoStream = -1;
for (int i = 0; i < fmtContext->nb_streams; i++) {
	if (fmtContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
		videoStream = i;
		break;
	}
}
if (videoStream == -1) {
	fprintf(stderr, "Could not find video stream\n");
	exit(1);
}

// get a pointer to the codec context for the video stream
AVCodecContext* codecContextOrigin = fmtContext->streams[videoStream]->codec;
// find the decoder for the video stream
AVCodec* codec = avcodec_find_decoder(codecContextOrigin->codec_id);
if (!codec) {
	fprintf(stderr, "Unsupported codec\n");
	exit(1);
}

// copy context
AVCodecContext* codecContext = avcodec_alloc_context3(codec);
if (avcodec_copy_context(codecContext, codecContextOrigin) != 0) {
	fprintf(stderr, "Couldn't copy codec context");
	return -1;
}

// open codec
if (avcodec_open2(codecContext, codec, nullptr) < 0) {
	fprintf(stderr, "Couldn't open codec");
	return -1;
}

*/
