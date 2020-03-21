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
	//! ý����ʱ��
	int totalMs_ = 0;
	//! ��Ƶ���
	int width_ = 0;
	//! ��Ƶ�߶�
	int height_ = 0;
	//! ��Ƶ������
	int sampleRate_ = 0;
	//! ��Ƶͨ����
	int channels_ = 0;
};