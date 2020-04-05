#pragma once

#include "screen_recorder.h"

class screen_recorder_directx : public screen_recorder
{
public:
	virtual ~screen_recorder_directx() {}

	virtual bool start(int outFPS = 10, int maxCachedBgra = 10) override;
	virtual void stop() override;

protected:
	virtual void run() override;

protected:
	friend class screen_recorder;
	screen_recorder_directx() : screen_recorder() {}
};
