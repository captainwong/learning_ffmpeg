#include "../ffmpeg.h"
#include "../sdl.h"


int main()
{
	// test_yuv420p_320x180.yuv
	//const char* file_path = R"(C:\Users\Jack\Videos\2020-01-16_20-13-52.mkv)";
	//const char* file_path = R"(test_yuv420p_320x180.yuv)";
	const char* file_path = R"(Z:\BodyCombat20171007200236.mp4)";

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

	int videoStream = -1;
	AVCodecContext* codecContext = openCodecContext(fmtContext, AVMEDIA_TYPE_VIDEO, videoStream);
	if (!codecContext) {
		fprintf(stderr, "Could not find video stream\n");
		exit(1);
	}

	av_dump_format(fmtContext, 0, file_path, 0);

	// allocate video frame
	AVFrame* frame = av_frame_alloc();
	AVFrame* frameYUV = av_frame_alloc();

	// determine required buffer size and allocate buffer
	int nbytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codecContext->width, codecContext->height, 1);
	uint8_t* buffer = (uint8_t*)av_malloc(nbytes * sizeof(uint8_t));

	// assign appropriate parts of buffer to image planes in frameYUV
	// Note that frameYUV is an AVFrame, but AVFrame is a superset of AVPicture
	av_image_fill_arrays(frameYUV->data, frameYUV->linesize, buffer, AV_PIX_FMT_YUV420P, codecContext->width, codecContext->height, 1);

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

	AVPacket packet;
	SDL_Event ev;
	while (av_read_frame(fmtContext, &packet) >= 0) {
		if (packet.stream_index == videoStream) {
			// decode video frame
			int ret = avcodec_send_packet(codecContext, &packet);
			if (ret < 0) {
				fprintfAVErrorString(ret, "Error while sending a packet to the decoder");
				break;
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
					SDL_Delay(33);
					//av_frame_unref(frame);
				}
			}
		}
		av_packet_unref(&packet); // or av_free_packet
		if (SDL_PollEvent(&ev) && ev.type == SDL_QUIT) {
			break;
		}
	}

	SDL_Quit();

	av_free(buffer);
	av_frame_free(&frameYUV);
	av_frame_free(&frame);
	avcodec_free_context(&codecContext);
	avformat_close_input(&fmtContext);
}
