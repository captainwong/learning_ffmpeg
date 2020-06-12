

#include <stdio.h>
#include <thread>
#include <string>
#ifdef _WIN32
#include <jlib/win32/UnicodeTool.h>
#endif
#include <stdint.h>
#include <assert.h>

#pragma warning(disable:4819)
#pragma warning(disable:4996)

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#ifdef _WIN32
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#endif

int main()
{
	avdevice_register_all(); // 这一句必须有，否则下面返回的 ifmt 是 NULL
	AVInputFormat* ifmt = av_find_input_format("avfoundation");
	if (!ifmt) {
		fprintf(stderr, "Failed to find input format for input device 'avfoundation'\n");
		return -1;
	}
	AVFormatContext* ic = NULL;
	char msg[1024];
	int ret = avformat_open_input(&ic, ":0", ifmt, NULL);
	if (ret < 0) {
		av_strerror(ret, msg, sizeof(msg));
		fprintf(stderr, "Failed to open audio device %s\n", msg);
		return ret;
	}

	av_dump_format(ic, 0, ":0", 0);
}