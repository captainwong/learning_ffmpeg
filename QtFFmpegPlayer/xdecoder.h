#pragma once

#include <mutex>
#include <stdint.h>

struct AVCodecContext;
struct AVCodecParameters;
struct AVFrame;
struct AVPacket;

/**
* 解格式
*/
class xdecoder
{
public:
	xdecoder();
	virtual ~xdecoder();

	static void freePacket(AVPacket** pkt);
	static void freeFrame(AVFrame** frame);

	/**
	* @brief 打开解码器，总是释放param
	*/
	virtual bool open(AVCodecParameters* param);

	/**
	* @brief 发送到解码线程，总是释放pkt空间（对象和对象内部data）
	*/
	virtual bool send(AVPacket* pkt);

	/**
	* @brief 获取解码数据
	* @return AVFrame* 由调用者负责释放
	* @note 一次send可能需要多次recv；要send(nullptr)并再次recv多次以获取完缓存数据
	*/
	virtual AVFrame* recv();

	virtual void clear();
	virtual void close();

	int64_t pts() const { return pts_; }

protected:
	std::mutex mutex_{};
	AVCodecContext* context_ = nullptr;
	int64_t pts_ = 0;
};

