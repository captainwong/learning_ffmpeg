#include "xdecoderthread.h"
#include "xdecoder.h"

xdecoderthread::xdecoderthread()
	: QThread()
	, decoder_(new xdecoder())
{
	
}

xdecoderthread::~xdecoderthread()
{
	isExit_ = true;
	wait();
}

void xdecoderthread::push(AVPacket* pkt)
{
	if (!pkt) { return; }
	while (!isExit_) {
		msleep(1);
		std::lock_guard<std::mutex> lg(mutex_);
		if (pkts_.size() < maxQ_) {
			pkts_.push(pkt);
			return;
		}
	}
}

AVPacket* xdecoderthread::pop()
{
	std::lock_guard<std::mutex> lg(mutex_);
	if (pkts_.empty()) { return nullptr; }
	AVPacket* pkt = pkts_.front(); pkts_.pop();
	return pkt;
}

void xdecoderthread::clear()
{
	std::lock_guard<std::mutex> lg(mutex_);
	decoder_->clear();
	while (!pkts_.empty()) {
		AVPacket* pkt = pkts_.front(); pkts_.pop();
		xdecoder::freePacket(&pkt);
	}
}

void xdecoderthread::close()
{
	clear();
	isExit_ = true;
	wait();

	std::lock_guard<std::mutex> lg(mutex_);
	decoder_->close();
	delete decoder_;
	decoder_ = nullptr;
}

void xdecoderthread::pause(bool isPause)
{
	//printf("xdecoderthread::pause enter\n");
	isPaused_ = isPause;
	//printf("xdecoderthread::pause leave\n");
}
