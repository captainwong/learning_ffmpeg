#include "xvideodecoderthread.h"
#include "xdecoder.h"

namespace jlib {
namespace qt {
namespace xplayer {

xvideodecoderthread::xvideodecoderthread()
	: xdecoderthread()
{
}

xvideodecoderthread::~xvideodecoderthread()
{
}

bool xvideodecoderthread::open(AVCodecParameters* param, IVideoCall* call, int width, int height)
{
	if (!param) { return false; }
	clear();
	std::lock_guard<std::mutex> lg(vmutex_);
	syncPts_ = 0;
	call_ = call;
	if (call) {
		call->doInit(width, height);
	}
	if (!decoder_->open(param)) {
		fprintf(stderr, "Open video decoder failed\n");
		return false;
	}

	return true;
}

bool xvideodecoderthread::repaintPts(AVPacket* pkt, int64_t seekPts)
{
	std::lock_guard<std::mutex> lg(vmutex_);
	if (!decoder_->send(pkt)) {
		return true;
	}
	AVFrame* frame = decoder_->recv();
	if (!frame) { return false; }
	if (decoder_->pts() >= seekPts) {
		if (call_) {
			call_->doRepaint(frame);
		} else {
			decoder_->freeFrame(&frame);
		}
		return true;
	}
	decoder_->freeFrame(&frame);
	return false;
}

void xvideodecoderthread::run()
{
	bool shouldSleep = false;

	while (!isExit_) {
		if (shouldSleep) {
			shouldSleep = false;
			msleep(5);
			continue;
		}

		std::lock_guard<std::mutex> lg(vmutex_);
		if (isPaused_) {
			shouldSleep = true;
			continue;
		}

		if (0 < syncPts_ && syncPts_ < decoder_->pts()) {
			shouldSleep = true;
			continue;
		}

		if (!decoder_->send(pop())) {
			shouldSleep = true;
			continue;
		}

		while (!isExit_) {
			AVFrame* frame = decoder_->recv();
			if (!frame) { break; }

			if (call_) {
				call_->doRepaint(frame);
			} else {
				decoder_->freeFrame(&frame);
			}
		}
	}
}

}
}
}
