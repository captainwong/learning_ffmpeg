#include <stdio.h>
#include "../ffmpeg.h"


void saveFrame(AVFrame* pFrame, int width, int height, int index)
{
	// Open file
	char szFilename[32];
	sprintf(szFilename, "frame%d.ppm", index);
	FILE* f = fopen(szFilename, "wb");

	if (!f)
		return;

	// Write header
	fprintf(f, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (int y = 0; y < height; y++) {
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, f);
	}

	fclose(f);
}

int main()
{
	//const char* file_path = "F:/CloudMusic/MV/a.mp4";
	const char* file_path = R"(C:\Users\Jack\Videos\2020-01-16_20-13-52.mkv)";

	av_register_all();

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
	av_dump_format(fmtContext, 0, file_path, 0);

	// find the first video stream
	int videoStream = -1;
	for (int i = 0; i < fmtContext->nb_streams; i++) {
		if (fmtContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
	if (videoStream == -1) {
		fprintf(stderr, "Could not find video stream\n");
		exit(1);
	}

	// get a pointer to the codec context for the video stream
	AVCodecContext* codecContextOrigin = fmtContext->streams[videoStream]->codec;
	// find the decoder for the video stream
	AVCodec* codec = avcodec_find_decoder(codecContextOrigin->codec_id);
	if (!codec) {
		fprintf(stderr, "Unsupported codec\n");
		exit(1);
	}

	// copy context
	AVCodecContext* codecContext = avcodec_alloc_context3(codec);
	if (avcodec_copy_context(codecContext, codecContextOrigin) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1;
	}

	// open codec
	if (avcodec_open2(codecContext, codec, nullptr) < 0) {
		fprintf(stderr, "Couldn't open codec");
		return -1;
	}

	// allocate video frame
	AVFrame* frame = av_frame_alloc();
	AVFrame* frameRGB = av_frame_alloc();

	// determine required buffer size and allocate buffer
	int nbytes = avpicture_get_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height);
	uint8_t* buffer = (uint8_t*)av_malloc(nbytes * sizeof(uint8_t));

	// assign appropriate parts of buffer to image planes in frameRGB
	// Note that frameRGB is an AVFrame, but AVFrame is a superset of AVPicture
	avpicture_fill((AVPicture*)frameRGB, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height);

	// initialize SWS context for software scaling
	SwsContext* swsContext = sws_getContext(codecContext->width, 
											codecContext->height,
											codecContext->pix_fmt,
											codecContext->width, 
											codecContext->height,
											AV_PIX_FMT_RGB24,
											SWS_BILINEAR,
											nullptr,
											nullptr,
											nullptr);

	// read frames and save first five frames to disk
	AVPacket packet;
	while (av_read_frame(fmtContext, &packet) >= 0) {
		if (packet.stream_index == videoStream) {
			// decode video frame
			int gotPicture = 0;
			avcodec_decode_video2(codecContext, frame, &gotPicture, &packet);
			if (gotPicture) {
				// convert the image from its native format to RGB
				sws_scale(swsContext, 
						  frame->data, frame->linesize, 
						  0, codecContext->height,
						  frameRGB->data, frameRGB->linesize);
				static int count = 0;
				if (++count <= 5) {
					saveFrame(frameRGB, codecContext->width, codecContext->height, count);
				} else {
					break;
				}
			}
		}
		av_free_packet(&packet);
	}

	av_free(buffer);
	av_frame_free(&frameRGB);
	av_frame_free(&frame);
	avcodec_close(codecContext);
	avcodec_close(codecContextOrigin);
	avformat_close_input(&fmtContext);
}
