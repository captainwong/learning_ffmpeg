#include "xaudioplayer.h"
#include <QAudioFormat>
#include <QAudioOutput>
#include <mutex>

namespace jlib {
namespace qt {
namespace xplayer {

class qaudioplayer : public xaudioplayer
{
public:
	qaudioplayer() : xaudioplayer() {}
	virtual ~qaudioplayer() {}

	//! ����Ƶ����
	virtual bool open(int sampleRate = 44100, int channels = 2) override {
		close();

		QAudioFormat fmt;
		fmt.setSampleRate(sampleRate);
		fmt.setSampleSize(sampleSize_);
		fmt.setChannelCount(channels);
		fmt.setCodec("audio/pcm");
		fmt.setByteOrder(QAudioFormat::LittleEndian);
		fmt.setSampleType(QAudioFormat::UnSignedInt);

		std::lock_guard<std::mutex> lg(mutex);
		sampleRate_ = sampleRate;
		channels_ = channels;
		output = new QAudioOutput(fmt);
		io = output->start();
		return !!io;
	}

	virtual void close() override {
		std::lock_guard<std::mutex> lg(mutex);
		if (io) { io->close(); io = nullptr; }
		if (output) {
			output->stop();
			delete output;
			output = nullptr;
		}
	}

	virtual void clear() override {
		std::lock_guard<std::mutex> lg(mutex);
		if (io) { io->reset(); }
	}

	//! ��ȡ�������л�û�в��ŵ�����ʱ�������룩
	virtual int64_t getNotPlayedMs() override {
		std::lock_guard<std::mutex> lg(mutex);
		if (!output) { return 0; }
		// ��δ���ŵ��ֽ���
		double size = output->bufferSize() - output->bytesFree();
		// һ����Ƶ�ֽ���
		double oneSecSize = sampleRate_ * (sampleSize_ / 8) * channels_;
		if (oneSecSize <= 0) { return 0; }
		else { return size / oneSecSize * 1000; }
	}

	//! ��ȡ���û�������С
	virtual int getFreeSize() override {
		std::lock_guard<std::mutex> lg(mutex);
		if (!output) { return 0; }
		return output->bytesFree();
	}

	//! д����Ƶ���ݣ�����getFreeSize���ش�С��С��sizeʱ����
	virtual bool write(const char* data, int size) override {
		if (!data || size <= 0) { return false; }
		std::lock_guard<std::mutex> lg(mutex);
		if (!output || !io) { return false; }
		int writen = io->write(data, size);
		return writen == size;
	}

	virtual void pause(bool isPause) override {
		std::lock_guard<std::mutex> lg(mutex);
		//printf("qaudioplayer::pause enter\n");
		if (!output) { return; }
		isPause ? output->suspend() : output->resume();
		//printf("qaudioplayer::pause leave\n");
	}

private:
	QAudioOutput* output{ nullptr };
	QIODevice* io{ nullptr };
	std::mutex mutex{};
};


xaudioplayer* xaudioplayer::instance()
{
	static qaudioplayer player;
	return &player;
}



}
}
}

