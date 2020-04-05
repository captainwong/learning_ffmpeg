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
	* @brief ���һ����Ƶ��
	* @param inWidth ������Ƶ�Ŀ��
	* @param inHeight ������Ƶ�ĸ߶�
	* @param inFmt ������Ƶ�ĸ�ʽ
	* @param outWidth �����Ƶ�Ŀ��
	* @param outHeight �����Ƶ�Ŀ��
	* @param outFPS �����Ƶ��֡��
	* @param outBitRate �����Ƶ������
	*/
	bool addVideoStream(int inWidth = 800, int inHeight  = 600, VPixFmt inPixFmt = VPixFmt::AV_PIX_FMT_BGRA,
						int outWidth = 800, int outHeight = 600, int outFPS = 25, int outBitRate = 5000000);

	/**
	* @brief ���һ����Ƶ��
	* @param inSampleRate 
	* @param inChannels 
	* @param inSmpFmt 
	* @param inOutNbSamples ���������Ҫ�õ���ÿ֡����ÿͨ����������
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
