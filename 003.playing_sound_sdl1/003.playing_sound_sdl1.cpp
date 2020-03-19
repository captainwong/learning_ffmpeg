#include "../ffmpeg3.h"
#include "../sdl1.h"
#include <assert.h>
#include <queue>

constexpr int SDL_AUDIO_BUFFER_SIZE = 1024;
constexpr int MAX_AUDIO_FRAME_SIZE = 192000;

struct PacketQueue {
	std::queue<AVPacket*> q = {};
	int size = 0;
	int quit = 0;
	SDL_mutex* mutex = nullptr;
	SDL_cond* cond = nullptr;

	void init() {
		mutex = SDL_CreateMutex();
		cond = SDL_CreateCond();
	}

	int put(AVPacket* pkt) {
		AVPacket* pkt1 = (AVPacket*)av_malloc(sizeof(AVPacket));
		if (av_packet_ref(pkt1, pkt) < 0) { return -1; }
		*pkt1 = *pkt;
		assert(0 == memcmp(pkt1, pkt, sizeof(AVPacket)));
		SDL_LockMutex(mutex);
		q.push(pkt1);
		size += pkt1->size;
		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
		return 0;
	}

	int get(AVPacket* pkt, int block) {
		int ret = -1;
		SDL_LockMutex(mutex);
		while (1) {
			if (quit) { ret = -1; break; }

			if (!q.empty()) {
				auto pkt1 = q.front(); q.pop();
				*pkt = *pkt1;
				assert(0 == memcmp(pkt1, pkt, sizeof(AVPacket)));
				size -= pkt1->size;
				//av_free(pkt1);
				ret = 1;
				break;
			} else if (!block) {
				ret = 0;
				break;
			} else {
				SDL_CondWait(cond, mutex);
			}
		}
		SDL_UnlockMutex(mutex);
		return ret;
	}
};

PacketQueue audioQ = {};

SwrContext* swrContext = nullptr;

int audioDecodeFrame(AVCodecContext* aCodecContext, uint8_t* audioBuf, int bufSize)
{
	static AVPacket pkt;
	static uint8_t* audioPktData = nullptr;
	static int audioPktSize = 0;
	static AVFrame frame;

	while (1) {
		if (audioQ.quit) {
			return -1;
		}

		if (audioQ.get(&pkt, 1) < 0) {
			return -1;
		}

		int ret = avcodec_send_packet(aCodecContext, &pkt);
		if (ret < 0) {
			fprintfAVErrorString(ret, "Error while sending a packet to the decoder");
			return ret;
		}

		while (ret >= 0) {
			ret = avcodec_receive_frame(aCodecContext, &frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				fprintfAVErrorString(ret, "Error while receiving a frame from the decoder");
				break;
			}

			if (ret >= 0) {

				ret = swr_convert(swrContext, &audioBuffer, maxAudioFrameSize, (const uint8_t**)frame->data, frame->nb_samples);
				if (ret < 0) {
					fprintfAVErrorString(ret, "swr_convert error");
					return ret;
				}
				audioPos = audioChunk = audioBuffer;
				audioLen = audioBufferSize;

			}
		}

		while (audioPktSize > 0) {
			int gotFrame = 0;
			int len = avcodec_decode_audio4(codecCtx, &frame, &gotFrame, &pkt);
			if (len < 0) {
				audioPktSize = 0;
				break;
			}
			audioPktData += len;
			audioPktSize -= len;
			int dataSize = 0;
			if (gotFrame) {
				dataSize = av_samples_get_buffer_size(nullptr, codecCtx->channels, frame.nb_samples, codecCtx->sample_fmt, 1);
				assert(dataSize <= bufSize);
				memcpy(audioBuf, frame.data[0], dataSize);
			}
			if (dataSize <= 0) {
				continue;
			}
			return dataSize;
		}

		if (pkt.data) {
			av_packet_unref(&pkt);
		}

		if (audioQ.quit) {
			return -1;
		}

		if (audioQ.get(&pkt, 1) < 0) {
			return -1;
		}

		audioPktData = pkt.data;
		audioPktSize = pkt.size;
	}
}

void audioCallback(void* userdata, Uint8* stream, int len)
{
	static uint8_t audioBuf[MAX_AUDIO_FRAME_SIZE * 3 / 2];
	static unsigned int audioBufSize = 0;
	static unsigned int audioBufIdx = 0;

	auto codecCtx = (AVCodecContext*)userdata;
	while (len > 0) {
		if (audioBufIdx >= audioBufSize) {
			int audioSize = audioDecodeFrame(codecCtx, audioBuf, sizeof(audioBuf));
			if (audioSize < 0) {
				audioBufSize = 1024;
				memset(audioBuf, 0, audioBufSize);
			} else {
				audioBufSize = audioSize;
			}
			audioBufIdx = 0;
		}
		int len1 = __min(len, audioBufSize - audioBufIdx);
		memcpy(stream, audioBuf + audioBufIdx, len1);
		len -= len1;
		stream += len1;
		audioBufIdx += len1;
	}
}


int main()
{
	//const char* file_path = R"(Z:\BodyCombat20171007200236.mp4)";
	const char* file_path = R"(Z:\winter10.mkv)";
	//const char* file_path = "F:/CloudMusic/MV/a.mp4";
	//const char* file_path = "Z:/ONeal.mkv";
	//const char* file_path = "Z:/8guangboticao.mp4";
	//const char* file_path = R"(Z:\winter.mkv)";

	av_register_all();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

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

	int videoStream = -1, audioStream = -1;
	AVCodecContext* codecContext = openCodecContext(fmtContext, AVMEDIA_TYPE_VIDEO, videoStream);
	if (!codecContext) {
		fprintf(stderr, "Could not find video stream\n");
		exit(1);
	}
	AVCodecContext* aCodecContext = openCodecContext(fmtContext, AVMEDIA_TYPE_AUDIO, audioStream);
	if (!aCodecContext) {
		fprintf(stderr, "Could not find audio stream\n");
		exit(1);
	}

	av_dump_format(fmtContext, 0, file_path, 0);

	// allocate audio frame
	//AVFrame* frame = av_frame_alloc();
	AVPacket* packet = av_packet_alloc();

	// initialize SWS context for software scaling
	SwsContext* swsContext = sws_getContext(codecContext->width,
											codecContext->height,
											codecContext->pix_fmt,
											codecContext->width,
											codecContext->height,
											AV_PIX_FMT_YUV420P,
											SWS_BILINEAR,
											nullptr,
											nullptr,
											nullptr);

	SDL_Surface* screen = SDL_SetVideoMode(codecContext->width, codecContext->height, 0, 0);
	if (!screen) {
		fprintf(stderr, "SDL: could not create window - exiting:%s\n", SDL_GetError());
		exit(1);
	}

	SDL_Overlay* bmp = SDL_CreateYUVOverlay(codecContext->width, codecContext->height, SDL_YV12_OVERLAY, screen);
	

	SDL_Rect rect;
	rect.x = rect.y = 0;
	rect.w = codecContext->width;
	rect.h = codecContext->height;

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

	// determine required buffer size and allocate buffer
	int audioBufferSize = av_samples_get_buffer_size(nullptr, outNbChannels, outNbSamples, outSampleFormat, 1);
	uint8_t* audioBuffer = (uint8_t*)av_malloc(maxAudioFrameSize * 2); // 双声道

	SDL_AudioSpec wantedSpec;
	wantedSpec.freq = outSampleRate;
	wantedSpec.format = AUDIO_S16SYS;
	wantedSpec.channels = outNbChannels;
	wantedSpec.silence = 0;
	wantedSpec.samples = outNbSamples;
	wantedSpec.callback = audioCallback;
	wantedSpec.userdata = aCodecContext;

	if (SDL_OpenAudio(&wantedSpec, nullptr) < 0) {
		fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
		exit(1);
	}

	swrContext = swr_alloc_set_opts(nullptr,
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

	audioQ.init();

	auto decodeVideoPacket = [codecContext, swsContext, bmp, &rect](AVPacket* packet) {
		// decode video frame
		int ret = avcodec_send_packet(codecContext, packet);
		if (ret < 0) {
			fprintfAVErrorString(ret, "Error while sending a packet to the decoder");
			return ret;
		}

		AVFrame frame;
		while (ret >= 0) {
			ret = avcodec_receive_frame(codecContext, &frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				fprintfAVErrorString(ret, "Error while receiving a frame from the decoder");
				break;
			}

			if (ret >= 0) {
				// convert the image from its native format to YUV
				SDL_LockYUVOverlay(bmp);
				AVPicture pict;
				pict.data[0] = bmp->pixels[0];
				pict.data[1] = bmp->pixels[1];
				pict.data[2] = bmp->pixels[2];
				pict.linesize[0] = bmp->pitches[0];
				pict.linesize[1] = bmp->pitches[1];
				pict.linesize[2] = bmp->pitches[2];

				sws_scale(swsContext, (const uint8_t* const*)frame.data, frame.linesize, 0, codecContext->height, pict.data, pict.linesize);
				SDL_UnlockYUVOverlay(bmp);

				rect.x = rect.y = 0;
				rect.w = codecContext->width;
				rect.h = codecContext->height;
				SDL_DisplayYUVOverlay(bmp, &rect);
			}
		}

		return 0;
	};

	SDL_Event ev;
	SDL_PauseAudio(0);
	while (av_read_frame(fmtContext, packet) >= 0) {
		if (packet->stream_index == videoStream) {
			if (decodeVideoPacket(packet) < 0) {
				break;
			}
		} else if (packet->stream_index == audioStream) {
			audioQ.put(packet);
		} else {
			av_packet_unref(packet);
		}

		if (SDL_PollEvent(&ev) && ev.type == SDL_QUIT) {
			audioQ.quit = 1;
			break;
		}
	}

	SDL_Quit();

	//av_frame_free(&frame);
	av_packet_unref(packet);
	avcodec_free_context(&codecContext);
	avcodec_free_context(&aCodecContext);
	avformat_close_input(&fmtContext);
}