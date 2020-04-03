#pragma once

#include "audio_recorder.h"

class audio_recorder_qt : public audio_recorder
{
public:
	virtual bool start(int sample_rate = 44100, int channels = 2, int max_cached_pcms = 10) override;
	virtual void stop() override;

protected:
	virtual void run() override;
};