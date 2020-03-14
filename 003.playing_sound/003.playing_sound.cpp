#include "../ffmpeg.h"
#include "../sdl.h"
#include <assert.h>

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

int main()
{
	//const char* file_path = R"(Z:\BodyCombat20171007200236.mp4)";
	//const char* file_path = R"(Z:\winter10.mkv)";
	const char* file_path = "F:/CloudMusic/MV/a.mp4";

	//av_register_all();

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

	mutex = SDL_CreateMutex();
	cond = SDL_CreateCond();

	// allocate audio frame
	AVFrame* frame = av_frame_alloc();
	AVFrame* frameYUV = av_frame_alloc();
	AVPacket* packet = av_packet_alloc();


	// determine required buffer size and allocate buffer
	int videoBuffSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codecContext->width, codecContext->height, 1);
	uint8_t* videoBuff = (uint8_t*)av_malloc(videoBuffSize * sizeof(uint8_t));

	// assign appropriate parts of buffer to image planes in frameYUV
	// Note that frameYUV is an AVFrame, but AVFrame is a superset of AVPicture
	av_image_fill_arrays(frameYUV->data, frameYUV->linesize, videoBuff, AV_PIX_FMT_YUV420P, codecContext->width, codecContext->height, 1);

	// initialize SWS context for software scaling
	SwsContext* swsContext = sws_getContext(codecContext->width,
											codecContext->height,
											codecContext->pix_fmt,
											codecContext->width,
											codecContext->height,
											AV_PIX_FMT_YUV420P,
											SWS_BICUBIC,
											nullptr,
											nullptr,
											nullptr);

	SDL_Window* screen = SDL_CreateWindow("outputing to screen",
										  SDL_WINDOWPOS_UNDEFINED,
										  SDL_WINDOWPOS_UNDEFINED,
										  codecContext->width,
										  codecContext->height,
										  SDL_WINDOW_OPENGL);
	if (!screen) {
		fprintf(stderr, "SDL: could not create window - exiting:%s\n", SDL_GetError());
		exit(1);
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(screen, -1, 0);
	SDL_Texture* texture = SDL_CreateTexture(renderer,
											 SDL_PIXELFORMAT_IYUV,
											 SDL_TEXTUREACCESS_STREAMING,
											 codecContext->width,
											 codecContext->height);

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

	auto decodeVideoPacket = [codecContext, swsContext, frame, frameYUV, renderer, texture, &rect](AVPacket* packet) {
		// decode video frame
		int ret = avcodec_send_packet(codecContext, packet);
		if (ret < 0) {
			fprintfAVErrorString(ret, "Error while sending a packet to the decoder");
			return ret;
		}

		while (ret >= 0) {
			ret = avcodec_receive_frame(codecContext, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				fprintfAVErrorString(ret, "Error while receiving a frame from the decoder");
				break;
			}

			if (ret >= 0) {
				// convert the image from its native format to YUV
				sws_scale(swsContext,
						  frame->data, frame->linesize,
						  0, codecContext->height,
						  frameYUV->data, frameYUV->linesize);

				SDL_UpdateYUVTexture(texture, &rect,
									 frameYUV->data[0], frameYUV->linesize[0],
									 frameYUV->data[1], frameYUV->linesize[1],
									 frameYUV->data[2], frameYUV->linesize[2]);
				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, nullptr, &rect);
				SDL_RenderPresent(renderer);
				//SDL_Delay(33);
				//av_frame_unref(frame);
			}
		}

		return 0;
	};

	auto decodeAudioPacket = [&aCodecContext, &swrContext, &frame, &audioBuffer, &audioBufferSize, maxAudioFrameSize](AVPacket* packet) {
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
				ret = swr_convert(swrContext, &audioBuffer, maxAudioFrameSize, (const uint8_t**)frame->data, frame->nb_samples);
				if (ret < 0) {
					fprintfAVErrorString(ret, "swr_convert error");
					return ret;
				}
				audioPos = audioChunk = audioBuffer;
				audioLen = audioBufferSize;
				SDL_UnlockMutex(mutex);
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
			if (decodeAudioPacket(packet) < 0) {
				break;
			}
		}
			
		av_packet_unref(packet); // or av_free_packet
		if (SDL_PollEvent(&ev) && ev.type == SDL_QUIT) {
			break;
		}
	}

	// flush cached frames
	decodeVideoPacket(nullptr);
	decodeAudioPacket(nullptr);

	SDL_Quit();

	av_free(videoBuff);
	av_free(audioBuffer);
	av_frame_free(&frameYUV);
	av_frame_free(&frame); 
	av_packet_unref(packet);
	avcodec_free_context(&codecContext);
	avcodec_free_context(&aCodecContext);
	avformat_close_input(&fmtContext);
}