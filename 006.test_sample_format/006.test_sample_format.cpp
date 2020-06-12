

#include <stdio.h>
#include <string>
#ifdef _WIN32
#include <jlib/win32/UnicodeTool.h>
#endif

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
#ifdef _WIN32
	const char* device = "dshow";
	std::string audio_ = jlib::win32::mbcs_to_utf8("audio=麦克风 (Realtek High Definition Audio)");
	const char* audio = audio_.data();
#elif defined(__APPLE__)
	const char* device = "avfoundation";
	const char* audio = ":0";
#else
#error todo
#endif

	avdevice_register_all(); // 这一句必须有，否则下面返回的 ifmt 是 NULL
	AVInputFormat* ifmt = av_find_input_format(device);
	if (!ifmt) {
		fprintf(stderr, "Failed to find input format for input device '%s'\n", device);
		return -1;
	}
	AVFormatContext* ic = NULL;
	char msg[1024];
	int ret = avformat_open_input(&ic, audio, ifmt, NULL);
	if (ret < 0) {
		av_strerror(ret, msg, sizeof(msg));
		fprintf(stderr, "Failed to open audio device '%s':%s\n", audio, msg);
		return ret;
	}

	av_dump_format(ic, 0, audio, 0);
	avformat_close_input(&ic);

	return 0;
}