#pragma once

#include <string>

struct AVPacket;

class encoder
{
public:
	~encoder();

	enum class VPixFmt {
		AV_PIX_FMT_BGRA = 28,
	};

	enum class ASmpFmt {
		AV_SAMPLE_FMT_S16 = 1,
		AV_SAMPLE_FMT_FLTP = 8,
	};

	static encoder* getInstance();

	bool open(const char* file);
	void close();
	
	/**
	* @brief 添加一个视频流
	* @param inWidth 输入视频的宽度
	* @param inHeight 输入视频的高度
	* @param inFmt 输入视频的格式
	* @param outWidth 输出视频的宽度
	* @param outHeight 输出视频的宽度
	* @param outFPS 输出视频的帧率
	* @param outBitRate 输出视频的码率
	*/
	bool addVideoStream(int inWidth = 800, int inHeight  = 600, VPixFmt inPixFmt = VPixFmt::AV_PIX_FMT_BGRA,
						int outWidth = 800, int outHeight = 600, int outFPS = 25, int outBitRate = 5000000);

	/**
	* @brief 添加一个音频流
	* @param inSampleRate 
	* @param inChannels 
	* @param inSmpFmt 
	* @param inOutNbSamples 输入输出都要用到，每帧数据每通道样本数量
	* @param outSampleRate 
	* @param outChannels 
	* @param outSmpFmt 
	* @param outBitRate 
	*/
	bool addAudioStream(int inSampleRate = 44100, int inChannels = 2, ASmpFmt inSmpFmt = ASmpFmt::AV_SAMPLE_FMT_S16, int inOutNbSamples = 1024,
						int outSampleRate = 44100, int outChannels = 2, ASmpFmt outSmpFmt = ASmpFmt::AV_SAMPLE_FMT_FLTP, int outBitRate = 64000);
	
	AVPacket* encodeVideo(char* bgra);
	AVPacket* encodeAudio(char* pcm);
	
	bool writeHeader();
	bool writeFrame(AVPacket* pkt);
	bool writeTrailer();
	bool isVideoBeforeAudio();


protected:
	encoder() {}

	std::string fileName_ = {};
	int inWidth_ = 800;
	int inHeight_ = 600;
};
