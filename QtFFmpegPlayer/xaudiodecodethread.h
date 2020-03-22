#pragma once

#include "xdecoderthread.h"
#include <stdint.h>

struct AVCodecParameters;
class xaudioplayer;
class xresampler;

class xaudiodecoderthread : xdecoderthread
{
public:
	xaudiodecoderthread();
	virtual ~xaudiodecoderthread();

	bool open(AVCodecParameters* param, int sampleRate, int channels);
	void close();
	void clear();
	void pause(bool isPause);

	virtual void run() override;

protected:
	std::mutex amutex_{};
	xaudioplayer* player_{ nullptr };
	xresampler* resampler_{ nullptr };
	std::atomic_bool isPaused_{ false };
	//! 当前音频播放的PTS
	int64_t pts_ = 0;
};
