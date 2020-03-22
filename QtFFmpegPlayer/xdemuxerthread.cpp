#include "xdemuxerthread.h"
#include "xdemuxer.h"
#include "xaudiodecodethread.h"
#include "xvideodecoderthread.h"
#include "xdecoder.h"


xdemuxerthread::xdemuxerthread()
	: QThread()
{
}

xdemuxerthread::~xdemuxerthread()
{
	isExit_ = true;
	wait();
}

bool xdemuxerthread::open(const char* url, IVideoCall* call)
{
	if (!url || url[0] == '\0') { return false; }
	std::lock_guard<std::mutex> lg(mutex_);
	if (!demuxer_) { demuxer_ = new xdemuxer(); }
	if (!vdt_) { vdt_ = new xvideodecoderthread(); }
	if (!adt_) { adt_ = new xaudiodecoderthread(); }

	bool ret = demuxer_->open(url);
	if (!ret) { return false; }
	totalMs_ = demuxer_->totalMs();
	if (!vdt_->open(demuxer_->getVideoParam(), call, demuxer_->width(), demuxer_->height())) {
		ret = false;
	}
	if (!adt_->open(demuxer_->getAudioParam(), demuxer_->sampleRate(), demuxer_->channels())) {
		ret = false;
	}

	return ret;
}

void xdemuxerthread::start()
{
	std::lock_guard<std::mutex> lg(mutex_);
	if (!demuxer_) { demuxer_ = new xdemuxer(); }
	if (!vdt_) { vdt_ = new xvideodecoderthread(); }
	if (!adt_) { adt_ = new xaudiodecoderthread(); }
	QThread::start();
	if (vdt_) { vdt_->start(); }
	if (adt_) { adt_->start(); }
}

void xdemuxerthread::clear()
{
	std::lock_guard<std::mutex> lg(mutex_);
	if (demuxer_) { demuxer_->clear(); }
	if (vdt_) { vdt_->clear(); }
	if (adt_) { adt_->clear(); }
}

void xdemuxerthread::close()
{
	isExit_ = true;
	wait();
	std::lock_guard<std::mutex> lg(mutex_);
	if (vdt_) { vdt_->close(); delete vdt_; }
	if (adt_) { adt_->close(); delete adt_; }
	adt_ = nullptr; vdt_ = nullptr;
}

void xdemuxerthread::seek(double pos)
{
	clear();
	bool isPaused = isPaused_;
	pause(true);

	{
		std::lock_guard<std::mutex> lg(mutex_);
		if (!demuxer_) { return; }
		demuxer_->seek(pos);
		int64_t seekPos = demuxer_->totalMs() * pos;
		while (!isExit_) {
			AVPacket* pkt = demuxer_->readVideo();
			if (!pkt) { break; }
			if (vdt_->repaintPts(pkt, seekPos)) {
				pts_ = seekPos;
				break;
			}
		}
	}

	if (!isPaused) {
		pause(false);
	}
}

void xdemuxerthread::pause(bool isPause)
{
	isPaused_ = isPause;

	{
		std::lock_guard<std::mutex> lg(mutex_);
		//printf("xdemuxerthread::pause enter\n");
		if (adt_) { adt_->pause(isPause); }
		if (vdt_) { vdt_->pause(isPause); }
		//printf("xdemuxerthread::pause leave\n");
	}	
}

void xdemuxerthread::run()
{
	bool shouldSleep = false;
	while (!isExit_) {
		msleep(1);

		if (shouldSleep) {
			shouldSleep = false;
			msleep(5);
			continue;
		}

		if (isPaused_) {
			msleep(5);
			continue;
		}
		
		std::lock_guard<std::mutex> lg(mutex_);
		if (!demuxer_) {
			shouldSleep = true;
			continue;
		}

		if (vdt_ && adt_) {
			pts_ = adt_->pts();
			vdt_->setSyncPts(pts_);
		}

		AVPacket* pkt = demuxer_->read();
		if (!pkt) {
			shouldSleep = true;
			continue;
		}

		if (demuxer_->isAudio(pkt)) {
			if (adt_) {
				adt_->push(pkt);
			} else {
				xdecoder::freePacket(&pkt);
			}
		} else {
			if (vdt_) {
				vdt_->push(pkt);
			} else {
				xdecoder::freePacket(&pkt);
			}
		}
	}
}
