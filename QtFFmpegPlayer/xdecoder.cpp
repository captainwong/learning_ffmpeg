#include "xdecoder.h"
#include <stdio.h>
#include <thread>

#pragma warning(disable:4819)

extern "C" {
#include <libavcodec/avcodec.h>
}


namespace jlib {
namespace qt {
namespace xplayer {

xdecoder::xdecoder()
{
}

xdecoder::~xdecoder()
{
}

void xdecoder::freePacket(AVPacket** pkt)
{
	av_packet_free(pkt);
}

void xdecoder::freeFrame(AVFrame** frame)
{
	av_frame_free(frame);
}

bool xdecoder::open(AVCodecParameters* param)
{
	if (!param) { return false; }
	close();

	do {
		AVCodec* codec = avcodec_find_decoder(param->codec_id);
		if (!codec) {
			fprintf(stderr, "Find decoder failed\n");
			break;
		}

		std::lock_guard<std::mutex> lg(mutex_);
		context_ = avcodec_alloc_context3(codec);
		if (!context_) {
			fprintf(stderr, "Allocate codec context failed\n");
			break;
		}

		int ret = avcodec_parameters_to_context(context_, param);
		avcodec_parameters_free(&param);
		if (ret < 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "Config codec context failed: %s\n", buf);
			break;
		}

		context_->thread_count = (int)std::thread::hardware_concurrency();

		ret = avcodec_open2(context_, nullptr, nullptr);
		if (ret != 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "Open codec failed: %s\n", buf);
			break;
		}

		return true;

	} while (0);

	avcodec_parameters_free(&param);

	std::lock_guard<std::mutex> lg(mutex_);
	if (context_) {
		avcodec_free_context(&context_);
	}

	return false;
}

bool xdecoder::send(AVPacket* pkt)
{
	if (!pkt || !pkt->data || pkt->size <= 0) { return false; }
	std::lock_guard<std::mutex> lg(mutex_);
	int ret = 0;
	if (context_) {
		ret = avcodec_send_packet(context_, pkt);
		if (ret < 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "Send packet failed: %s\n", buf);
		}
	}
	av_packet_free(&pkt);
	return ret == 0;
}

AVFrame* xdecoder::recv()
{
	std::lock_guard<std::mutex> lg(mutex_);
	AVFrame* frame = av_frame_alloc();
	int ret = avcodec_receive_frame(context_, frame);
	if (ret != 0) {
		if (ret != AVERROR(EAGAIN)) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "Receive frame failed: %s\n", buf);
		}		
		av_frame_free(&frame);
	} else {
		pts_ = frame->pts;
		printf("pts=%lld\n", pts_);
	}
	return frame;
}

void xdecoder::clear()
{
	std::lock_guard<std::mutex> lg(mutex_);
	if (context_) {
		avcodec_flush_buffers(context_);
	}
}

void xdecoder::close()
{
	std::lock_guard<std::mutex> lg(mutex_);
	if (context_) {
		avcodec_close(context_);
		avcodec_free_context(&context_);
	}
	pts_ = 0;
}

}
}
}
