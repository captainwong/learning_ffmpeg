#pragma once

#include <mutex>

struct AVFormatContext;
struct AVCodecContext;
struct AVPacket;

class XDemux
{
public:
	XDemux();
	virtual ~XDemux();

	//! 

protected:
	std::mutex mutex = {};
	AVFormatContext* ic_ = nullptr;
	int videoStream_ = -1;
	int audioStream_ = -1;
	//! 媒体总时长
	int totalMs_ = 0;
	//! 视频宽度
	int width_ = 0;
	//! 视频高度
	int height_ = 0;
	//! 音频采样率
	int sampleRate_ = 0;
	//! 音频通道数
	int channels_ = 0;
};