#include "screen_recorder.h"
#include "screen_recorder_directx.h"

screen_recorder* screen_recorder::getInstance(recorder_type type)
{
	switch (type) {
	case screen_recorder::recorder_type::directX:
	{
		static screen_recorder_directx rec;
		return &rec;
	}
	case screen_recorder::recorder_type::qt:
		break;
	default:
		break;
	}
	return nullptr;
}

bool screen_recorder::start(int outFPS, int outWidth, int outHeight)
{
	outFPS_ = outFPS > 0 ? outFPS : 10;
	outWidth_ = outWidth;
	outHeight_ = outHeight;
	return true;
}

void screen_recorder::stop()
{
	std::lock_guard<std::mutex> lg(mutex_);
	running_ = false;
	if (thread_.joinable()) {
		thread_.join();
	}
	for (auto p : bgras_) {
		delete[] p.data;
	}
	bgras_.clear();
}

screen_recorder::bgra screen_recorder::getBGRA(bool block)
{
	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(0));
		std::lock_guard<std::mutex> lg(mutex_);
		if (!bgras_.empty()) {
			auto p = bgras_.front();
			bgras_.pop_front();
			return p;
		} else if (block) {
			continue;
		} else {
			break;
		}
	}
	return bgra();
}
