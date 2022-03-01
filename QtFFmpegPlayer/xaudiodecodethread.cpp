#include "xaudiodecodethread.h"
#include "xresampler.h"
#include "xaudioplayer.h"
#include "xdecoder.h"

namespace jlib {
namespace qt {
namespace xplayer {

xaudiodecoderthread::xaudiodecoderthread()
{
	resampler_ = new xresampler();
	player_ = xaudioplayer::instance();
}

xaudiodecoderthread::~xaudiodecoderthread()
{
	isExit_ = true;
	wait();
}

bool xaudiodecoderthread::open(AVCodecParameters* param, int sampleRate, int channels)
{
	if (!param) { return false; }
	clear();

	std::lock_guard<std::mutex> lg(amutex_);
	pts_ = 0;

	bool ret = true;

	if (!resampler_->open(param, false)) {
		ret = false;
		fprintf(stderr, "Open resampler failed\n");
	}

	if (!player_->open(sampleRate, channels)) {
		ret = false;
		fprintf(stderr, "Open audio player failed\n");
	}

	if (!decoder_->open(param)) {
		ret = false;
		fprintf(stderr, "Open audio decoder failed\n");
	}

	return ret;
}

void xaudiodecoderthread::close()
{
	xdecoderthread::close();
	std::lock_guard<std::mutex> lg(mutex_);
	if (resampler_) {
		resampler_->close();
		delete resampler_;
		resampler_ = nullptr;
	}
	if (player_) {
		player_->close();
		player_ = nullptr;
	}
}

void xaudiodecoderthread::clear()
{
	xdecoderthread::clear();
	std::lock_guard<std::mutex> lg(mutex_);
	if (player_) { player_->clear(); }
}

void xaudiodecoderthread::pause(bool isPause)
{
	//printf("xaudiodecoderthread::pause enter\n");
	xdecoderthread::pause(isPause);
	//std::lock_guard<std::mutex> lg(amutex_);
	if (player_) {
		player_->pause(isPause);
	}
	//printf("xaudiodecoderthread::pause leave\n");
}

void xaudiodecoderthread::run()
{
	uint8_t* pcm = new uint8_t[1024 * 1024 * 10];
	bool shouldSleep = false;

	while (!isExit_) {
		if (shouldSleep) {
			usleep(5);
			shouldSleep = false;
		}

		{
			std::lock_guard<std::mutex> lg(amutex_);
			if (isPaused_) {
				shouldSleep = true;
				continue;
			}

			if (!decoder_->send(pop())) {
				shouldSleep = true;
				continue;
			}
		}

		while (!isExit_) {			
			std::lock_guard<std::mutex> lg(amutex_);
			AVFrame* frame = decoder_->recv();
			if (!frame) { break; }

			pts_ = decoder_->pts() - player_->getNotPlayedMs();
			int size = resampler_->resample(frame, pcm);
			if (size <= 0) { break; }

			while (!isExit_) {
				if (isPaused_ || player_->getFreeSize() < size) {
					msleep(5);
					continue;
				}
				player_->write((const char*)pcm, size);
				break;
			}
		}
	}

	delete[] pcm;
}

}
}
}
