#pragma once

#include "xdecoderthread.h"
#include <stdint.h>

struct AVCodecParameters;
class xaudioplayer;
class xresampler;

class xaudiodecoderthread : public xdecoderthread
{
public:
	xaudiodecoderthread();
	virtual ~xaudiodecoderthread();

	bool open(AVCodecParameters* param, int sampleRate, int channels);
	int64_t pts() const { return pts_; }
	
	virtual void close() override;
	virtual void clear() override;
	virtual void pause(bool isPause) override;

	virtual void run() override;

protected:
	std::mutex amutex_{};
	xaudioplayer* player_{ nullptr };
	xresampler* resampler_{ nullptr };
	//! 当前音频播放的PTS
	int64_t pts_ = 0;
};
