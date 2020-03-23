#pragma once

#include <QThread>
#include <mutex>
#include <atomic>
#include <stdint.h>
#include "IVideoCall.h"

class xdemuxer;
class xaudiodecoderthread;
class xvideodecoderthread;

class xdemuxerthread : QThread
{
public:
	xdemuxerthread();
	virtual ~xdemuxerthread();

	virtual bool open(const char* url, IVideoCall* call);
	virtual void start();
	virtual void clear();
	virtual void close();
	virtual void seek(double pos);
	virtual void pause(bool isPause);
	virtual bool isPaused() const { return isPaused_; }
	virtual void run() override;

	int64_t totalMs() const { return totalMs_; }
	int64_t pts() const { return pts_; }

protected:
	std::mutex mutex_{};
	std::atomic_bool isExit_{ false };
	std::atomic_bool isPaused_{ false };
	std::atomic<int64_t> pts_{ 0 };
	std::atomic<int64_t> totalMs_{ 0 };
	xdemuxer* demuxer_ = nullptr;
	xvideodecoderthread* vdt_ = nullptr;
	xaudiodecoderthread* adt_ = nullptr;
};
