#pragma once

#include <mutex>

struct AVCodecParameters;
struct AVFormatContext;
struct AVPacket;

/**
* 解封装
*/
class xdemuxer
{
public:
	xdemuxer();
	virtual ~xdemuxer();

	//! 打开媒体文件（mp4, avi, rmvb...）、流媒体（rtsp, rtmp, http...)
	virtual bool open(const char* url);

	//! 关闭
	virtual void close();

	//! 清空读取缓存
	virtual void clear();

	/**
	* @brief 读取一帧（视频或音频）
	* @return AVPacket*
	* @note 由调用者负责释放AVPacket*及其数据 av_packet_free
	*/
	virtual AVPacket* read();

	/**
	* @brief 读取一帧视频
	* @return AVPacket*
	* @note 由调用者负责释放AVPacket*及其数据 av_packet_free
	*/
	virtual AVPacket* readVideo();

	virtual bool isAudio(AVPacket* pkt);

	/**
	* @brief 获取视频参数
	* @return AVCodecParameters*
	* @note 由调用者负责释放AVCodecParameters*， avcodec_parameters_free
	*/
	virtual AVCodecParameters* getVideoParam();

	/**
	* @brief 获取音频参数
	* @return AVCodecParameters*
	* @note 由调用者负责释放AVCodecParameters*， avcodec_parameters_free
	*/
	virtual AVCodecParameters* getAudioParam();

	/**
	* @brief 跳转播放
	* @param pos [0.0, 1.0)
	*/
	virtual bool seek(double pos);

	//! 媒体总时长
	int totalMs() const { return totalMs_; }
	//! 视频宽度
	int width() const { return width_; }
	//! 视频高度
	int height() const { return height_; }
	//! 音频采样率
	int sampleRate() const { return sampleRate_; }
	//! 音频通道数
	int channels() const { return channels_; }

protected:
	std::mutex mutex_ = {};
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
