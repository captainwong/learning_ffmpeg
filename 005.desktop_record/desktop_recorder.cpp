#include "desktop_recorder.h"
#include "encoder.h"
#include "audio_recorder.h"
#include "screen_recorder.h"


bool desktop_recorder::start(const char* fileName, int fps, int outWidth, int outHeight)
{
	stop();

	do {
		std::lock_guard<std::mutex> lg(mutex_);

		auto srec = screen_recorder::getInstance();
		if (!srec->start(fps)) {
			break;
		}

		if (!audio_recorder::getInstance()->start()) {
			break;
		}

		auto enc = encoder::getInstance();
		if (!enc->open(fileName)) {
			break;
		}
		if (!enc->addAudioStream()) {
			break;
		}
		if (!enc->addVideoStream(srec->getWidth(), srec->getHeight(), encoder::VPixFmt::AV_PIX_FMT_BGRA, outWidth, outHeight, fps)) {
			break;
		}
		if (!enc->writeHeader()) {
			break;
		}

		running_ = true;
		thread_ = std::thread(&desktop_recorder::run, this);
		return true;

	} while (0);

	stop();
	return false;
}

void desktop_recorder::stop()
{
	running_ = false;

	{
		std::lock_guard<std::mutex> lg(mutex_);
		if (thread_.joinable()) {
			thread_.join();
		}
	}

	encoder::getInstance()->writeTrailer();
	encoder::getInstance()->close();

	audio_recorder::getInstance()->stop();
	screen_recorder::getInstance()->stop();
}

void desktop_recorder::run()
{
	auto arec = audio_recorder::getInstance();
	auto vrec = screen_recorder::getInstance();
	auto enc = encoder::getInstance();

	while (running_) {
		std::lock_guard<std::mutex> lg(mutex_);
		auto bgra = vrec->getBGRA();
		if (bgra.data) {
			auto pkt = enc->encodeVideo(bgra.data);
			delete[] bgra.data;
			if (pkt && enc->writeFrame(pkt)) {
				printf("-");
			} else {
				fprintf(stderr, "*");
			}
		}
		auto pcm = arec->getPCM();
		if (pcm.data) {
			auto pkt = enc->encodeAudio(pcm.data);
			delete[] pcm.data;
			if (pkt && enc->writeFrame(pkt)) {
				printf(".");
			} else {
				fprintf(stderr, "*");
			}
		}
	}
}
