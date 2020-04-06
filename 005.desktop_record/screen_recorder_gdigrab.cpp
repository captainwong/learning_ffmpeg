#include "screen_recorder_gdigrab.h"
#include <assert.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <QElapsedTimer>

#pragma warning(disable:4819)

extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

#pragma comment(lib, "avdevice.lib")

static AVFormatContext* ic = nullptr;


bool screen_recorder_gdigrab::start(int outFPS, int maxCachedBgra)
{
	stop();
	__super::start(outFPS, maxCachedBgra);

	do {
		std::lock_guard<std::mutex> lg(mutex_);

		outWidth_ = GetSystemMetrics(SM_CXSCREEN);
		outHeight_ = GetSystemMetrics(SM_CYSCREEN);		

		ic = avformat_alloc_context();
		if (!ic) {
			fprintf(stderr, "avformat_alloc_context failed\n");
			break;
		}

		avdevice_register_all();
		AVInputFormat* ifmt = av_find_input_format("gdigrab"); 
		if (!ifmt) {
			fprintf(stderr, "av_find_input_format for 'gdigrab' failed\n");
			break;
		}

		AVDictionary* opts = nullptr;
		//av_dict_set(&opts, "framerate", std::to_string(outFPS_).data(), 0);
		std::string video_size = std::to_string(outWidth_) + "x" + std::to_string(outHeight_);
		av_dict_set(&opts, "video_size", video_size.data(), 0);
		int ret = avformat_open_input(&ic, "desktop", ifmt, &opts);
		if (ret != 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "avformat_open_input 'desktop' failed: %s\n", buf);
			break;
		}

		ret = avformat_find_stream_info(ic, nullptr);
		if (ret < 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "avformat_find_stream_info failed: %s\n", buf);
			break;
		}

		av_dump_format(ic, 0, "gdigrab", 0);

		ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		if (ret < 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "av_find_best_stream AVMEDIA_TYPE_VIDEO failed: %s\n", buf);
			break;
		}

		running_ = true;
		thread_ = std::thread(&screen_recorder_gdigrab::run, this);
		return true;
	} while (0);

	stop();
	return false;
}

void screen_recorder_gdigrab::stop()
{
	__super::stop();

	avformat_close_input(&ic);
}

void screen_recorder_gdigrab::run()
{
	QElapsedTimer t;

	while (running_) {
		t.restart();
		int ms = 1000;

		do {
			std::lock_guard<std::mutex> lg(mutex_);
			ms /= outFPS_;
			if (!ic) { break; }
			if (bgras_.size() >= maxCachedBgra_) {
				break;
			}

			AVPacket* pkt = av_packet_alloc();
			int ret = av_read_frame(ic, pkt);
			if (ret != 0) {
				char buf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, buf, sizeof(buf) - 1);
				fprintf(stderr, "Read frame failed: %s\n", buf);
				av_packet_free(&pkt);
				break;
			}

			assert(pkt->size > outWidth_* outHeight_ * 4);
			bgra p;
			p.len = outWidth_ * outHeight_ * 4;
			p.data = new char[p.len];
			memcpy(p.data, pkt->data + (pkt->size - p.len), p.len);
			bgras_.push_back(p);
			av_packet_free(&pkt);
		} while (0);

		ms -= t.restart();
		if (ms <= 0 || ms > 1000) {
			ms = 10;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));		
	}
}
