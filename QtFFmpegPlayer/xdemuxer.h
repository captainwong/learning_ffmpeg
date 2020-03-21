#pragma once

#include <mutex>

struct AVCodecParameters;
struct AVFormatContext;
struct AVPacket;

/**
* ���װ
*/
class xdemuxer
{
public:
	xdemuxer();
	virtual ~xdemuxer();

	//! ��ý���ļ���mp4, avi, rmvb...������ý�壨rtsp, rtmp, http...)
	virtual bool open(const char* url);

	//! �ر�
	virtual void close();

	//! ��ն�ȡ����
	virtual void clear();

	/**
	* @brief ��ȡһ֡����Ƶ����Ƶ��
	* @return AVPacket*
	* @note �ɵ����߸����ͷ�AVPacket*�������� av_packet_free
	*/
	virtual AVPacket* read();

	/**
	* @brief ��ȡһ֡��Ƶ
	* @return AVPacket*
	* @note �ɵ����߸����ͷ�AVPacket*�������� av_packet_free
	*/
	virtual AVPacket* readVideo();

	virtual bool isAudio(AVPacket* pkt);

	/**
	* @brief ��ȡ��Ƶ����
	* @return AVCodecParameters*
	* @note �ɵ����߸����ͷ�AVCodecParameters*�� avcodec_parameters_free
	*/
	virtual AVCodecParameters* getVideoParam();

	/**
	* @brief ��ȡ��Ƶ����
	* @return AVCodecParameters*
	* @note �ɵ����߸����ͷ�AVCodecParameters*�� avcodec_parameters_free
	*/
	virtual AVCodecParameters* getAudioParam();

	/**
	* @brief ��ת����
	* @param pos [0.0, 1.0)
	*/
	virtual bool seek(double pos);

	//! ý����ʱ��
	int totalMs() const { return totalMs_; }
	//! ��Ƶ���
	int width() const { return width_; }
	//! ��Ƶ�߶�
	int height() const { return height_; }
	//! ��Ƶ������
	int sampleRate() const { return sampleRate_; }
	//! ��Ƶͨ����
	int channels() const { return channels_; }

protected:
	std::mutex mutex_ = {};
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
