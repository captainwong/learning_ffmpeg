/**
2020年6月12日
Jack
通过各系统的音频输入设备采集音频并写入pcm文件

*/


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


int get_format_from_sample_fmt(const char** fmt,
							   enum AVSampleFormat sample_fmt)
{
	struct sample_fmt_entry {
		enum AVSampleFormat sample_fmt; const char* fmt_be, * fmt_le;
	} sample_fmt_entries[] = {
		{ AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
		{ AV_SAMPLE_FMT_S16, "s16be", "s16le" },
		{ AV_SAMPLE_FMT_S32, "s32be", "s32le" },
		{ AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
		{ AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
	};
	*fmt = NULL;

	for (int i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
		struct sample_fmt_entry* entry = &sample_fmt_entries[i];
		if (sample_fmt == entry->sample_fmt) {
			*fmt = AV_NE(entry->fmt_be, entry->fmt_le);
			return 0;
		}
	}

	fprintf(stderr,
			"sample format %s is not supported as output format\n",
			av_get_sample_fmt_name(sample_fmt));
	return -1;
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
	char msg[1024];
	int ret = avformat_open_input(&ic, device_name, ifmt, NULL);
	if (ret < 0) {
		av_strerror(ret, msg, sizeof(msg));
		fprintf(stderr, "Failed to open audio device [%s]:%s\n", device_name, msg);
		return ret;
	}

	av_dump_format(ic, 0, device_name, 0);

	AVCodecContext* cctx = nullptr;
	FILE* fp = nullptr;
	AVPacket pkt;

	av_init_packet(&pkt);
	pkt.data = nullptr; pkt.size = 0;

	do {
		if (!(fp = fopen(pcm_file, "wb"))) {
			ret = errno;
			fprintf(stderr, "Failed to open file '%s':[%d]%s\n", pcm_file, ret, strerror(ret));
			break;
		};

		assert(ic->nb_streams == 1);
		AVSampleFormat sample_format = (AVSampleFormat)ic->streams[0]->codecpar->format;
		int channel_count = ic->streams[0]->codecpar->channels;
		int sample_rate = ic->streams[0]->codecpar->sample_rate;
		const char* fmt = nullptr;
		get_format_from_sample_fmt(&fmt, sample_format);		

		printf("Press Q to stop record\n");
		bool running = true;
		std::thread t([&running]() {
			while (running) {
				int c = getchar();
				if (c == 'Q' || c == 'q') {
					running = false;
					break;
				} else {
					printf("Press Q to stop record\n");
				}
			}
		});

		while ((av_read_frame(ic, &pkt) >= 0) && running) {
			printf(".");
			fwrite(pkt.data, 1, pkt.size, fp);
			av_packet_unref(&pkt);
		}

		running = false;
		t.join();

		if (fmt) {
			printf("Record stopped, play the output audio file with the command:\n"
				   "ffplay -f %s -ac %d -ar %d %s\n",
				   fmt, channel_count, sample_rate, pcm_file);
		}

	} while (0);

	if (cctx) {
		avcodec_free_context(&cctx);
	}

	if (fp) {
		fclose(fp);
		fp = nullptr;
	}

	if (ic) {
		avformat_close_input(&ic);
	}

	return ret;
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