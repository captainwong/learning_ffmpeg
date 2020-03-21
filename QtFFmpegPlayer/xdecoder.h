#pragma once

#include <mutex>
#include <stdint.h>

struct AVCodecContext;
struct AVCodecParameters;
struct AVFrame;
struct AVPacket;

/**
* ���ʽ
*/
class xdecoder
{
public:
	xdecoder();
	virtual ~xdecoder();

	static void freePacket(AVPacket** pkt);
	static void freeFrame(AVFrame** frame);

	/**
	* @brief �򿪽������������ͷ�param
	*/
	virtual bool open(AVCodecParameters* param);

	/**
	* @brief ���͵������̣߳������ͷ�pkt�ռ䣨����Ͷ����ڲ�data��
	*/
	virtual bool send(AVPacket* pkt);

	/**
	* @brief ��ȡ��������
	* @return AVFrame* �ɵ����߸����ͷ�
	* @note һ��send������Ҫ���recv��Ҫsend(nullptr)���ٴ�recv����Ի�ȡ�껺������
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

