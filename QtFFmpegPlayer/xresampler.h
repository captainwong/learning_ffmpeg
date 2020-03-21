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
	* @brief 准备重采样
	* @param param 输入参数
	* @param freeParam 是否释放param
	* @note 除了采样格式转换为S16，其他输出参数与输入参数一致
	*/
	virtual bool open(AVCodecParameters* param, bool freeParam = false);

	/**
	* @brief 重采样
	* @param[in] frame 音频帧
	* @param[in|out] data 重采样后数据存放地址 ，用户需保证data足够大，一般用10M？
	* @return 重采样后数据大小或负数表示错误码
	* @note 总是释放frame及其内部空间
	*/
	virtual int resample(AVFrame* frame, unsigned char* data);

	virtual void close();

protected:
	std::mutex mutex_{};
	SwrContext* context_{ nullptr };
	int outFormat_ = 1; // AV_SAMPLE_FMT_S16
};
