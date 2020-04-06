#include "screen_recorder.h"
#include "screen_recorder_directx.h"
#include "screen_recorder_gdigrab.h"

screen_recorder* screen_recorder::getInstance(recorder_type type)
{
	switch (type) {
	case screen_recorder::recorder_type::directx:
	{
		static screen_recorder_directx rec;
		return &rec;
	}
	case screen_recorder::recorder_type::gdigrab:
	{
		static screen_recorder_gdigrab rec;
		return &rec;
	}
	default:
		return nullptr;
		break;
	}
}

bool screen_recorder::start(int outFPS, int maxCachedBgra)
{
	outFPS_ = outFPS > 0 ? outFPS : 10;
	maxCachedBgra_ = maxCachedBgra;
	return true;
}

void screen_recorder::stop()
{
	if (!running_) { return; }
	running_ = false;

	{
		std::lock_guard<std::mutex> lg(mutex_);
		if (thread_.joinable()) {
			thread_.join();
		}
		for (auto p : bgras_) {
			delete[] p.data;
		}
		bgras_.clear();
	}
}

screen_recorder::bgra screen_recorder::getBGRA(bool block)
{
	bool shouldSleep = false;
	while (running_) {
		if (shouldSleep) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		std::lock_guard<std::mutex> lg(mutex_);
		if (!bgras_.empty()) {
			auto p = bgras_.front();
			bgras_.pop_front();
			return p;
		} else if (block) {
			shouldSleep = true;
			continue;
		} else {
			break;
		}
	}
	return bgra();
}
