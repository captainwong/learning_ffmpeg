#include "xdemuxer.h"
#include <stdio.h>

#pragma warning(disable:4819)

extern "C" {
#include <libavformat/avformat.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

static double r2d(AVRational r) {
	return r.den == 0.0 ? 0.0 : (double)r.num / (double)r.den;
}

struct InitHelper {
	InitHelper() {
		av_register_all();
		avformat_network_init();
	}
};

xdemuxer::xdemuxer()
{
	static InitHelper helper{};
}

xdemuxer::~xdemuxer()
{
}

bool xdemuxer::open(const char* url)
{
	close();
	AVDictionary* opts = nullptr;
	// 设置rtsp流以tcp协议打开
	av_dict_set(&opts, "rtsp_transport", "tcp", 0);
	// 网络延迟时间
	av_dict_set(&opts, "max_delay", "500", 0);

	std::lock_guard<std::mutex> lg(mutex);
	int ret = avformat_open_input(&ic_, url, 0, &opts);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "Open %s failed: %s\n", url, buf);
		return false;
	}

	ret = avformat_find_stream_info(ic_, nullptr);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "Find stream info failed: %s\n", buf);
		return false;
	}
	
	av_dump_format(ic_, 0, url, 0);

	totalMs_ = ic_->duration / (AV_TIME_BASE / 1000);

	videoStream_ = av_find_best_stream(ic_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (videoStream_ < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "Warning: find video stream failed: %s\n", buf);
	} else {
		width_ = ic_->streams[videoStream_]->codecpar->width;
		height_ = ic_->streams[videoStream_]->codecpar->height;
	}

	audioStream_ = av_find_best_stream(ic_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (videoStream_ < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "Warning: find audio stream failed: %s\n", buf);
	} else {
		sampleRate_ = ic_->streams[audioStream_]->codecpar->sample_rate;
		channels_ = ic_->streams[audioStream_]->codecpar->channels;
	}

	return videoStream_ >= 0 || audioStream_ >= 0;
}

void xdemuxer::close()
{
	std::lock_guard<std::mutex> lg(mutex);
	if (ic_) {
		avformat_close_input(&ic_);
	}
	totalMs_ = 0;
}

void xdemuxer::clear()
{
	std::lock_guard<std::mutex> lg(mutex);
	if (ic_) {
		avformat_flush(ic_);
	}
}

AVPacket* xdemuxer::read()
{
	std::lock_guard<std::mutex> lg(mutex);
	AVPacket* pkt = nullptr;
	do {
		if (!ic_) { break; }
		pkt = av_packet_alloc();
		int ret = av_read_frame(ic_, pkt);
		if (ret != 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "Read frame failed: %s\n", buf);
			break;
		}

		// pts转换为毫秒
		pkt->pts = pkt->pts * (1000 * r2d(ic_->streams[pkt->stream_index]->time_base));
		pkt->dts = pkt->dts * (1000 * r2d(ic_->streams[pkt->stream_index]->time_base));

		return pkt;
	} while (0);
	
	if (pkt) {
		av_packet_free(&pkt);
	}

	return nullptr;
}

AVPacket* xdemuxer::readVideo()
{
	for (int i = 0; i < 20; i++) {
		auto pkt = read();
		if (!pkt) { break; }
		if (pkt->stream_index == videoStream_) {
			return pkt;
		}
	}
	return nullptr;
}

bool xdemuxer::isAudio(AVPacket* pkt)
{
	std::lock_guard<std::mutex> lg(mutex);
	return pkt && pkt->stream_index == audioStream_;
}

AVCodecParameters* xdemuxer::getVideoParam()
{
	std::lock_guard<std::mutex> lg(mutex);
	if (ic_) {
		auto param = avcodec_parameters_alloc();
		avcodec_parameters_copy(param, ic_->streams[videoStream_]->codecpar);
		return param;
	}
	return nullptr;
}

AVCodecParameters* xdemuxer::getAudioParam()
{
	std::lock_guard<std::mutex> lg(mutex);
	if (ic_) {
		auto param = avcodec_parameters_alloc();
		avcodec_parameters_copy(param, ic_->streams[audioStream_]->codecpar);
		return param;
	}
	return nullptr;
}

bool xdemuxer::seek(double pos)
{
	std::lock_guard<std::mutex> lg(mutex);
	if (!ic_) { return false; }
	avformat_flush(ic_);
	long long seekPos = ic_->streams[videoStream_]->duration * pos;
	int ret = av_seek_frame(ic_, videoStream_, seekPos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "Seek failed: %s\n", buf);
		return false;
	}
	return true;
}
