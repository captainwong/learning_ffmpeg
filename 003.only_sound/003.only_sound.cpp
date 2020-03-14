#include "../ffmpeg.h"
#include "../sdl.h"
#include <assert.h>
#include <stdlib.h>

size_t audioLen = 0;
uint8_t* audioChunk = nullptr;
uint8_t* audioPos = nullptr;
SDL_mutex* mutex = nullptr;
SDL_cond* cond = nullptr;

void audio_callback(void* userdata, Uint8* stream, int len)
{
	SDL_memset(stream, 0, len);
	if (len == 0) { return; }

	SDL_LockMutex(mutex);
	//printf("audio_callbck in\n");
	len = __min(len, audioLen);
	SDL_MixAudio(stream, audioPos, len, SDL_MIX_MAXVOLUME);
	audioPos += len;
	audioLen -= len;
	//printf("audio_callbck out\n");
	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
}

// 2020年3月14日21:31:40
// 播放MP3可以，播放视频文件的音频流就不行了，听不出来音乐了，都是杂音

int main()
{
	//const char* file_path = R"(Z:\BodyCombat20171007200236.mp4)"; 
	//const char* file_path = R"(Z:\winter10.mkv)";
	//const char* file_path = R"(F:\CloudMusic\111.mp3)";
	const char* file_path = R"(F:\CloudMusic\TsunaAwakes.mp3)";

	//av_register_all();

	/*if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}*/

	// open video file
	AVFormatContext* fmtContext = nullptr;
	if (avformat_open_input(&fmtContext, file_path, nullptr, nullptr) != 0) {
		fprintf(stderr, "Could not open source file %s\n", file_path);
		exit(1);
	}

	// retrieve stream information
	if (avformat_find_stream_info(fmtContext, nullptr) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		exit(1);
	}

	int audioStream = -1;
	AVCodecContext* aCodecContext = openCodecContext(fmtContext, AVMEDIA_TYPE_AUDIO, audioStream);
	if (!aCodecContext) {
		fprintf(stderr, "Could not find audio stream\n");
		exit(1);
	}

	av_dump_format(fmtContext, 0, file_path, 0);

	mutex = SDL_CreateMutex();
	cond = SDL_CreateCond();

	// set audio settings from codec info

	// 输出的通道布局为双通道
	constexpr auto outChannellayout = AV_CH_LAYOUT_STEREO;
	// 输出的声音格式
	constexpr auto outSampleFormat = AV_SAMPLE_FMT_S16;
	// 输出的采样率
	constexpr auto outSampleRate = 44100;
	// 单个通道中的采样数
	const auto outNbSamples = aCodecContext->frame_size;
	// 输出的声道数
	const auto outNbChannels = av_get_channel_layout_nb_channels(outChannellayout);
	// 缓冲区长度 bytes, 48kHz 32bit, 48000 * 32 / 8
	constexpr auto maxAudioFrameSize = 192000;

	// allocate audio frame
	AVFrame* frame = av_frame_alloc();
	AVPacket* packet = av_packet_alloc();

	// determine required buffer size and allocate buffer
	int bufferSize = av_samples_get_buffer_size(nullptr, outNbChannels, outNbSamples, outSampleFormat, 1);
	uint8_t* buffer = (uint8_t*)av_malloc(maxAudioFrameSize * 2); // 双声道

	SDL_AudioSpec wantedSpec;
	wantedSpec.freq = outSampleRate;
	wantedSpec.format = AUDIO_S16SYS;
	wantedSpec.channels = outNbChannels;
	wantedSpec.silence = 0;
	wantedSpec.samples = outNbSamples;
	wantedSpec.callback = audio_callback;
	wantedSpec.userdata = aCodecContext;

	if (SDL_OpenAudio(&wantedSpec, nullptr) < 0) {
		fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
		exit(1);
	}

	SwrContext* swrContext = swr_alloc_set_opts(nullptr,
												outChannellayout,
												outSampleFormat,
												outSampleRate,
												av_get_default_channel_layout(aCodecContext->channels),
												aCodecContext->sample_fmt,
												aCodecContext->sample_rate,
												0,
												nullptr);
	int ret = swr_init(swrContext);
	if (ret < 0) {
		fprintfAVErrorString(ret, "swr_init failed");
		exit(1);
	}

	auto decodeAudioPacket = [&aCodecContext, &swrContext, &frame, &buffer, &bufferSize, maxAudioFrameSize](AVPacket* packet) {
		// decode audio frame
		int ret = avcodec_send_packet(aCodecContext, packet);
		if (ret < 0) {
			fprintfAVErrorString(ret, "Error while sending a packet to the decoder");
			return ret;
		}

		while (ret >= 0) {
			ret = avcodec_receive_frame(aCodecContext, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				fprintfAVErrorString(ret, "Error while receiving a frame from the decoder");
				break;
			}

			if (ret >= 0) {
				SDL_LockMutex(mutex);
				SDL_CondWait(cond, mutex); // 等待音频播放完毕
				ret = swr_convert(swrContext, &buffer, maxAudioFrameSize, (const uint8_t**)frame->data, frame->nb_samples);
				if (ret < 0) {
					fprintfAVErrorString(ret, "swr_convert error");
					return ret;
				}
				audioPos = audioChunk = buffer;
				audioLen = bufferSize;
				SDL_UnlockMutex(mutex);
			}
		}

		return 0;
	};

	SDL_Event ev;
	SDL_PauseAudio(0);
	while (av_read_frame(fmtContext, packet) >= 0) {
		if (packet->stream_index == audioStream) {			
			if (decodeAudioPacket(packet) < 0) {
				av_packet_unref(packet);
				break;
			}
		}
		av_packet_unref(packet);
		if (SDL_PollEvent(&ev) && ev.type == SDL_QUIT) {			
			break;
		}
	}

	// flush cached frames
	decodeAudioPacket(nullptr);

	SDL_Quit();

	av_free(buffer);
	av_frame_free(&frame);
	avcodec_free_context(&aCodecContext);
	avformat_close_input(&fmtContext);
}
