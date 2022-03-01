#pragma once

#include <stdint.h>

namespace jlib {
namespace qt {
namespace xplayer {

class xaudioplayer
{
public:
	static xaudioplayer* instance();

	//! ����Ƶ����
	virtual bool open(int sampleRate = 44100, int channels = 2) = 0;
	virtual void close() = 0;
	virtual void clear() = 0;
	//! ��ȡ�������л�û�в��ŵ�����ʱ�������룩
	virtual int64_t getNotPlayedMs() = 0;
	//! ��ȡ���û�������С
	virtual int getFreeSize() = 0;
	//! д����Ƶ���ݣ�����getFreeSize���ش�С��С��sizeʱ����
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
