#pragma once

#include <QThread>
#include <mutex>
#include <queue>
#include <atomic>

struct AVPacket;
class xdecoder;

class xdecoderthread : public QThread
{
public:
	xdecoderthread();
	virtual ~xdecoderthread();

	virtual void push(AVPacket* pkt);
	virtual AVPacket* pop();
	virtual void clear();
	virtual void close();

protected:
	xdecoder* decoder_{ nullptr };
	std::queue<AVPacket*> pkts_;
	std::mutex mutex_{};
	int maxQ_ = 100;
	std::atomic_bool isExit_{ false };
};
