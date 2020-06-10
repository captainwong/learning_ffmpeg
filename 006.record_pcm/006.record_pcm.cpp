#include <stdio.h>
#include <thread>
#include <string>
#ifdef _WIN32
#include <jlib/win32/UnicodeTool.h>
#endif
#include <stdint.h>

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

const char* exe = "";


void usage()
{
	printf("Usage:%s indevice audio_device output.pcm\n"
		   "indevice     The input device, for Windows it's usually dshow or vfwcap, for Mac its avfoundation, Linux is alsa\n"
		   "audio_device The dshow audio device name like \"Microphone (3- USB2.0 MIC)\", i.e.\n"
		   "               on Windows, you can run `ffmpeg -f dshow --list-devices true -i \"dummy\"` to find out which device to use.\n"
		   "               on MacOS, its `ffmpeg -f avfoundation --list-devices true -i \"dummy\"`\n"
		   "               on Linux, its `cat /proc/asound/cards` or `cat /proc/asound/devices`\n"
		   "output.pcm   The recorded audio file\n"
		   "Example:\n"
		   "Windows:     %s dshow \"audio=麦克风(Realtek High Definition Audio)\" output.pcm\n"
		   "MacOS:       %s avfoundation :0 output.pcm\n"
		   "Linux:       %s alsa hw:0 output.pcm\n"
		   , exe, exe, exe, exe);
}

int record_pcm(const char* indevice, const char* device_name, const char* pcm_file)
{
	avdevice_register_all(); // 这一句必须有，否则下面返回的 ifmt 是 NULL
	AVInputFormat* ifmt = av_find_input_format(indevice);
	if (!ifmt) {
		fprintf(stderr, "Failed to find input format for input device '%s'\n", indevice);
		return -1;
	}
	AVFormatContext* ic = NULL;
	int ret = avformat_open_input(&ic, device_name, ifmt, NULL);
	if (ret < 0) {
		char msg[1024];
		av_strerror(ret, msg, sizeof(msg));
		fprintf(stderr, "Failed to open audio device [%s]:%s\n", device_name, msg);
		return ret;
	}

	av_dump_format(ic, 0, device_name, 0);


}

int main(int argc, char** argv)
{
	exe = argv[0];

#if defined(_WIN32) && defined(_DEBUG)
	const char* indevice = "dshow";
	const char* audio_device = "audio=麦克风 (Realtek High Definition Audio)";
	const char* output_file = "output.pcm";
#else
	if (argc < 4) {
		usage(); return 0;
	}
	const char* indevice = argv[1];
	const char* audio_device = argv[2];
	const char* output_file = argv[3];
#endif

	std::string audio_device_name = audio_device;	
#ifdef _WIN32
	audio_device_name = jlib::win32::mbcs_to_utf8(audio_device);
#endif
	return record_pcm(indevice, audio_device_name.data(), output_file);
}