#pragma warning(disable:4819)
#pragma warning(disable:4996)

extern "C" {
#include <libavformat/avformat.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

int main(int argc, char** argv) 
{
	if (argc < 3) { return 0; }
	auto infile = argv[1];
	auto outfile = argv[2];

	av_register_all();

	AVFormatContext* ic = nullptr;
	avformat_open_input(&ic, infile, nullptr, nullptr);
	if (!ic) {
		fprintf(stderr, "open input failed: %s\n", infile);
		return -1;
	}
	av_dump_format(ic, 0, infile, 0);

	AVFormatContext* oc = nullptr;
	avformat_alloc_output_context2(&oc, nullptr, nullptr, outfile);
	if (!oc) {
		fprintf(stderr, "open output failed: %s\n", outfile);
		return -1;
	}

	int videoStreamIndex = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (videoStreamIndex < 0) {
		fprintf(stderr, "WARN: find video stream failed\n");
	}

	int audioStreamIndex = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (audioStreamIndex < 0) {
		fprintf(stderr, "WARN: find audio stream failed\n");
	}

	if (videoStreamIndex < 0 && audioStreamIndex < 0) {
		fprintf(stderr, "ERROR: could not find vidoe nor audio stream\n");
		return -1;
	}

	AVStream* videoStream = nullptr;
	AVStream* audioStream = nullptr;
	if (videoStreamIndex >= 0) {
		videoStream = avformat_new_stream(oc, nullptr);
		avcodec_parameters_copy(videoStream->codecpar, ic->streams[videoStreamIndex]->codecpar);
		videoStream->codecpar->codec_tag = 0;
	}

	if (audioStreamIndex >= 0) {
		audioStream = avformat_new_stream(oc, nullptr);
		avcodec_parameters_copy(audioStream->codecpar, ic->streams[audioStreamIndex]->codecpar);
		audioStream->codecpar->codec_tag = 0;
	}

	printf("=================================\n");
	av_dump_format(oc, 0, outfile, 1);

	int ret = avio_open(&oc->pb, outfile, AVIO_FLAG_WRITE);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avio_open failed: %s\n", buf);
		return -1;
	}

	ret = avformat_write_header(oc, nullptr);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avformat_write_header failed: %s\n", buf);
		return -1;
	}

	AVPacket pkt;
	while (1) {
		ret = av_read_frame(ic, &pkt);
		if (ret < 0) { break; }
		AVStream* stream = nullptr;
		if (videoStreamIndex >= 0 && pkt.stream_index == videoStreamIndex) {
			stream = videoStream;
			printf("-");
		} else if (audioStreamIndex >= 0 && pkt.stream_index == audioStreamIndex) {
			stream = audioStream;
			printf(".");
		}
		if (stream) {
			pkt.pts = av_rescale_q_rnd(pkt.pts,
									   ic->streams[pkt.stream_index]->time_base,
									   stream->time_base,
									   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.dts = av_rescale_q_rnd(pkt.dts,
									   ic->streams[pkt.stream_index]->time_base,
									   stream->time_base,
									   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.duration = av_rescale_q_rnd(pkt.duration,
									   ic->streams[pkt.stream_index]->time_base,
									   stream->time_base,
									   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.pos = -1;

			//av_write_frame(oc, &pkt);
			av_interleaved_write_frame(oc, &pkt);
		} else {
			av_packet_unref(&pkt);
		}
	}

	av_write_trailer(oc);
	avio_close(oc->pb);
	avformat_free_context(oc);
	avformat_free_context(ic);
	printf("\ndone\n");
}

