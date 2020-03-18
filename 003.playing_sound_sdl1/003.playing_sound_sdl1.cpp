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
		if (av_dup_packet(pkt) < 0) { return -1; }
		AVPacket* t = (AVPacket*)av_malloc(sizeof(AVPacket));
		*t = *pkt;

		SDL_LockMutex(mutex);
		q.push(t);
		size += t->size;
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
				size -= pkt1->size;
				av_free(pkt1);
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

int audioDecodeFrame(AVCodecContext* codecCtx, uint8_t* audioBuf, int bufSize)
{
	static AVPacket pkt;
	static uint8_t* audioPktData = nullptr;
	static int audioPktSize = 0;
	static AVFrame frame;

	while (1) {
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
			av_free_packet(&pkt);
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
	//const char* file_path = R"(Z:\winter10.mkv)";
	//const char* file_path = "F:/CloudMusic/MV/a.mp4";
	//const char* file_path = "Z:/ONeal.mkv";
	//const char* file_path = "Z:/8guangboticao.mp4";
	const char* file_path = R"(Z:\winter.mkv)";

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
	AVFrame* frame = av_frame_alloc();
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
	SDL_AudioSpec wantedSpec;
	wantedSpec.freq = aCodecContext->sample_rate;
	wantedSpec.format = AUDIO_S16SYS;
	wantedSpec.channels = aCodecContext->channels;
	wantedSpec.silence = 0;
	wantedSpec.samples = SDL_AUDIO_BUFFER_SIZE;
	wantedSpec.callback = audioCallback;
	wantedSpec.userdata = aCodecContext;

	if (SDL_OpenAudio(&wantedSpec, nullptr) < 0) {
		fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
		exit(1);
	}

	audioQ.init();

	SDL_Event ev;
	SDL_PauseAudio(0);
	while (av_read_frame(fmtContext, packet) >= 0) {
		if (packet->stream_index == videoStream) {
			int gotFrame = 0;
			avcodec_decode_video2(codecContext, frame, &gotFrame, packet);
			if (gotFrame) {
				SDL_LockYUVOverlay(bmp);
				AVPicture pict;
				pict.data[0] = bmp->pixels[0];
				pict.data[1] = bmp->pixels[1];
				pict.data[2] = bmp->pixels[2];
				pict.linesize[0] = bmp->pitches[0];
				pict.linesize[1] = bmp->pitches[1];
				pict.linesize[2] = bmp->pitches[2];

				sws_scale(swsContext, (const uint8_t* const*)frame->data, frame->linesize, 0, codecContext->height, pict.data, pict.linesize);
				SDL_UnlockYUVOverlay(bmp);

				rect.x = rect.y = 0;
				rect.w = codecContext->width;
				rect.h = codecContext->height;
				SDL_DisplayYUVOverlay(bmp, &rect);
				av_free_packet(packet);
			}
		} else if (packet->stream_index == audioStream) {
			
		} else {
			av_packet_unref(packet);
		}

		if (SDL_PollEvent(&ev) && ev.type == SDL_QUIT) {
			break;
		}
	}

	SDL_Quit();

	av_frame_free(&frame);
	av_packet_unref(packet);
	avcodec_free_context(&codecContext);
	avcodec_free_context(&aCodecContext);
	avformat_close_input(&fmtContext);
}