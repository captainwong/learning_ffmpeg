#pragma once

struct AVCodecParameters;
struct AVFrame;
struct SwrContext;

#include <mutex>

class xresampler
{
public:
	xresampler();
	virtual ~xresampler();

	/**
	* @brief ׼���ز���
	* @param param �������
	* @param freeParam �Ƿ��ͷ�param
	* @note ���˲�����ʽת��ΪS16����������������������һ��
	*/
	virtual bool open(AVCodecParameters* param, bool freeParam = false);

	/**
	* @brief �ز���
	* @param[in] frame ��Ƶ֡
	* @param[in|out] data �ز��������ݴ�ŵ�ַ ���û��豣֤data�㹻��һ����10M��
	* @return �ز��������ݴ�С������ʾ������
	* @note �����ͷ�frame�����ڲ��ռ�
	*/
	virtual int resample(AVFrame* frame, unsigned char* data);

	virtual void close();

protected:
	std::mutex mutex_{};
	SwrContext* context_{ nullptr };
	int outFormat_ = 1; // AV_SAMPLE_FMT_S16
};
