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

void usage(const char* path) {
	printf("Usage: %s input_file image_count image_format\n"
		   "input_file   The file to extract images from\n"
		   "image_limit  Max image files to be extracted, 0 for unlimited\n"
		   "image_format Images' format, available formats are:\n"
		   "\tppm     AV_PIX_FMT_RGB24\n"
		   "\tyuv420p AV_PIX_FMT_YUV420P\n"
		   "\trgb     AV_PIX_FMT_RGB24\n"
		   "\tbgr     AV_PIX_FMT_BGR24\n"
		   , path);
}

int main(int argc, char** argv)
{
	if (argc < 4) { usage(argv[0]); return 1; }

	const auto infile = argv[1];
	int file_limit = atoi(argv[2]);
	if (file_limit < 0) { usage(argv[0]); return 1; }
	else if (file_limit == 0) { file_limit = -1; }
	AVPixelFormat out_pixel_fmt = AV_PIX_FMT_NONE;
	const char* outfile_ext = nullptr;
	if (0 == strcmp("ppm", argv[3])) {
		out_pixel_fmt = AV_PIX_FMT_RGB24;
		outfile_ext = "ppm";
	} else if (0 == strcmp("yuv420p", argv[3])) {
		out_pixel_fmt = AV_PIX_FMT_YUV420P;
		outfile_ext = "yuv420p";
	} else if (0 == strcmp("rgb", argv[3])) {
		out_pixel_fmt = AV_PIX_FMT_RGB24;
		outfile_ext = "rgb";
	} else if (0 == strcmp("bgr", argv[3])) {
		out_pixel_fmt = AV_PIX_FMT_BGR24;
		outfile_ext = "bgr";
	} else { usage(argv[0]); return 1; }

	printf("Extracting images from '%s', max images is %s, out file format is %s, ext is %s\n", 
		   infile, 
		   file_limit < 0 ? "unlimited" : argv[2], 
		   argv[3], 
		   outfile_ext);

	//const char* file_path = "F:/CloudMusic/MV/a.mp4";
	//const char* file_path = R"(C:\Users\Jack\Videos\2020-01-16_20-13-52.mkv)";

	//av_register_all();

	// open video file
	AVFormatContext* fmtContext = nullptr;
	if (avformat_open_input(&fmtContext, infile, nullptr, nullptr) != 0) {
		fprintf(stderr, "Could not open source file %s\n", infile);
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
	av_dump_format(fmtContext, 0, infile, 0);

	// allocate video frame
	AVFrame* frame = av_frame_alloc();
	AVFrame* frameRGB = av_frame_alloc();

	// determine required buffer size and allocate buffer
	int nbytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);
	uint8_t* buffer = (uint8_t*)av_malloc(nbytes * sizeof(uint8_t));

	// assign appropriate parts of buffer to image planes in frameRGB
	// Note that frameRGB is an AVFrame, but AVFrame is a superset of AVPicture
	av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);

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
			int ret = avcodec_send_packet(codecContext, &packet);
			if (ret < 0) {
				fprintfAVErrorString(ret, "Error while sending a packet to the decoder");
				break;
			}

			static int count = 0;
			while (ret >= 0) {
				ret = avcodec_receive_frame(codecContext, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					fprintfAVErrorString(ret, "Error while receiving a frame from the decoder");
					break;
				}

				if (ret >= 0) {
					// convert the image from its native format to RGB
					sws_scale(swsContext,
							  frame->data, frame->linesize,
							  0, codecContext->height,
							  frameRGB->data, frameRGB->linesize);
					if (++count <= 5) {
						saveFrame(frameRGB, codecContext->width, codecContext->height, count);
					} else {
						break;
					}
				}
			}

			if (count >= 5) {
				break;
			}
		}

		av_packet_unref(&packet);
	}

	av_free(buffer);
	av_frame_free(&frameRGB);
	av_frame_free(&frame);
	avcodec_free_context(&codecContext);
	avformat_close_input(&fmtContext);
}
