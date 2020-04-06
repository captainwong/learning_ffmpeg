#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <list>

class audio_recorder
{
public:

	struct pcm {
		char* data = nullptr;
		size_t len = 0;
	};

	enum class recorder_type {
		rec_qt,
	};

	static audio_recorder* getInstance(recorder_type type = recorder_type::rec_qt);

	virtual ~audio_recorder() { stop(); }

	/**
	* @brief ��ʼ¼��
	* @param sample_rate ��Ƶ������
	* @param channels ������
	* @param max_cached_pcms ��󻺴�PCM����
	*/
	virtual bool start(int sample_rate = 44100, int channels = 2, int max_cached_pcms = 10);

	virtual void stop();

	virtual pcm getPCM(bool block = false);

protected:
	virtual void run() = 0;


protected:
	int sample_rate_ = 44100;
	int channels_ = 2;
	int max_cached_pcms_ = 10;

	std::atomic_bool running_ = false;
	std::list<pcm> pcms_ = {};
	std::mutex mutex_ = {};
	std::thread thread_ = {};


	audio_recorder() {}
};

