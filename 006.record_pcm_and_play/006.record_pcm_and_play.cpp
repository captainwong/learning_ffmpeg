/**
2020年6月12日
Jack
通过各系统的音频输入设备采集音频并写入pcm文件，同时通过SDL2播放采集到的音频

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

/**
*
*/
int open_codec_context(AVFormatContext* ic,
					   AVMediaType type,
					   int& stream_idx,
					   AVCodecContext*& codec_ctx)
{
	int stream_index = -1;
	for (unsigned int i = 0; i < ic->nb_streams; i++) {
		if (ic->streams[i]->codecpar->codec_type == type) {
			stream_index = (int)i;
			break;
		}
	}

	if (stream_index == -1) {
		fprintf(stderr, "Failed to find stream type %s\n",
				av_get_media_type_string(type));
		return -1;
	}

	AVStream* stream = ic->streams[stream_index];
	AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
	if (!codec) {
		fprintf(stderr, "Failed to find decoder for %s, codec_id=%08x\n",
				av_get_media_type_string(type),
				stream->codecpar->codec_id);
		return -1;
	}

	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx) {
		fprintf(stderr, "Failed to allocate codec context for %s\n",
				av_get_media_type_string(type));
		return -1;
	}

	char msg[1024];
	int ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
	if (ret < 0) {
		av_strerror(ret, msg, sizeof(msg));
		fprintf(stderr, "Failed to copy %s codec parameters to decoder context:%s\n",
				av_get_media_type_string(type),
				msg);
		return ret;
	}

	if ((ret = avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
		fprintf(stderr, "Failed to open codec %s\n", av_get_media_type_string(type));;
		return ret;
	}

	stream_idx = stream_index;
	return 0;
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
	AVFrame* frame = nullptr;
	AVPacket pkt;

	av_init_packet(&pkt);
	pkt.data = nullptr; pkt.size = 0;

	do {
		int stream_index = -1;
		if ((ret = open_codec_context(ic, AVMEDIA_TYPE_AUDIO, stream_index, cctx)) < 0) {
			break;
		}

		if (!(fp = fopen(pcm_file, "wb"))) {
			ret = errno;
			fprintf(stderr, "Failed to open file '%s':[%d]%s\n", pcm_file, ret, strerror(ret));
			break;
		};

		if (!(frame = av_frame_alloc())) {
			fprintf(stderr, "Failed to allocate AVFrame\n");
			ret = -1;
			break;
		}

		while ((av_read_frame(ic, &pkt) >= 0)) {
			printf("got packet\n");
			ret = avcodec_send_packet(cctx, &pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {

			} else if (ret < 0) {
				av_strerror(ret, msg, sizeof(msg));
				fprintf(stderr, "Failed on submitting packet to decoder:%s\n", msg);
				return ret;
			} else {
				while (ret >= 0) {
					ret = avcodec_receive_frame(cctx, frame);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
						break;
					} else if (ret < 0) {
						av_strerror(ret, msg, sizeof(msg));
						fprintf(stderr, "Failed on decoding:%s\n", msg);
						return ret;
					} else {
						printf("got frame\n");
						int frame_size = av_get_bytes_per_sample(cctx->sample_fmt);

					}
				}

			}
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