#pragma once

#include <thread>
#include <mutex>
#include <atomic>

class desktop_recorder
{
public:
	~desktop_recorder() { stop(); }

	static desktop_recorder* getInstance() {
		static desktop_recorder rec;
		return &rec;
	}

	bool start(const char* fileName, int fps = 10, int outWidth = 800, int outHeight = 600);
	void stop();

protected:
	void run();

protected:
	std::thread thread_ = {};
	std::mutex mutex_ = {};
	std::atomic_bool running_ = false;

	desktop_recorder() {}
};
