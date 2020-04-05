#include "audio_recorder_qt.h"
#include <QAudioInput>
#include <chrono>

static QAudioInput* gInput = nullptr;
static QIODevice* gIO = nullptr;

bool audio_recorder_qt::start(int sample_rate, int channels, int max_cached_pcms)
{
	__super::start(sample_rate, channels, max_cached_pcms);

	std::lock_guard<std::mutex> lg(mutex_);

	QAudioFormat fmt;
	fmt.setSampleRate(sample_rate);
	fmt.setChannelCount(channels);
	fmt.setSampleSize(16);
	fmt.setSampleType(QAudioFormat::UnSignedInt);
	fmt.setByteOrder(QAudioFormat::LittleEndian);
	fmt.setCodec("audio/pcm");
	
	gInput = new QAudioInput(fmt);
	gIO = gInput->start();

	running_ = true;
	thread_ = std::thread(&audio_recorder_qt::run, this);

	return true;
}

void audio_recorder_qt::stop()
{
	__super::stop();
	std::lock_guard<std::mutex> lg(mutex_);
	
	if (gIO) {
		gIO->close();
	}
	if (gInput) {
		gInput->stop();
		delete gInput;
	}
	gInput = nullptr;
	gIO = nullptr;
}

void audio_recorder_qt::run()
{
	const int size = 1024 * channels_ * 2;

	while (running_) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		std::lock_guard<std::mutex> lg(mutex_);
		if (pcms_.size() > max_cached_pcms_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		pcm p;
		p.data = new char[size];
		p.len = 0;

		while (p.len < size && running_) {
			int bytes = 1024;
			if (gInput->bytesReady() < bytes) {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}

			bytes = std::min(bytes, size - (int)p.len);
			p.len += gIO->read(p.data + p.len, bytes);
		}

		pcms_.push_back(p);
	}
}
