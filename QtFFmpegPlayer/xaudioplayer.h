#pragma once

#include <stdint.h>

namespace jlib {
namespace qt {
namespace xplayer {

class xaudioplayer
{
public:
	static xaudioplayer* instance();

	//! 打开音频播放
	virtual bool open(int sampleRate = 44100, int channels = 2) = 0;
	virtual void close() = 0;
	virtual void clear() = 0;
	//! 获取缓冲区中还没有播放的数据时长（毫秒）
	virtual int64_t getNotPlayedMs() = 0;
	//! 获取可用缓冲区大小
	virtual int getFreeSize() = 0;
	//! 写入音频数据，需在getFreeSize返回大小不小于size时调用
	virtual bool write(const char* data, int size) = 0;
	virtual void pause(bool isPause) = 0;

protected:
	xaudioplayer() {}
	virtual ~xaudioplayer() {}

	int sampleRate_ = 44100;
	int sampleSize_ = 16;
	int channels_ = 2;
};

}
}
}
