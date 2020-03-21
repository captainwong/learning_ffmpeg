#include "xresampler.h"

#pragma warning(disable:4819)

extern "C" {
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
}

#pragma comment(lib, "swresample.lib")

xresampler::xresampler()
{
}

xresampler::~xresampler()
{
}

bool xresampler::open(AVCodecParameters* param, bool freeParam)
{
	if (!param) { return false; }
	std::lock_guard<std::mutex> lg(mutex_);
	context_ = swr_alloc_set_opts(context_,
								  av_get_default_channel_layout(2),					// �����ʽ
								  (AVSampleFormat)outFormat_,						// ���������ʽ
								  param->sample_rate,								// ���������
								  av_get_default_channel_layout(param->channels),	// �����ʽ
								  (AVSampleFormat)param->format,					// ����������ʽ
								  param->sample_rate,								// ���������
								  0, 0);

	if (freeParam) {
		avcodec_parameters_free(&param);
	}

	int ret = swr_init(context_);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "swr_init failed: %s\n", buf);
		return false;
	}

	return true;
}

int xresampler::resample(AVFrame* frame, unsigned char* data)
{
	if (!frame) { return 0; }
	if (!data) {
		av_frame_free(&frame);
		return 0;
	}

	uint8_t* outData[2] = { data, nullptr };
	int ret = swr_convert(context_,
						  outData, frame->nb_samples,
						  (const uint8_t**)frame->data, frame->nb_samples);
	int outSize = ret * frame->channels * av_get_bytes_per_sample((AVSampleFormat)outFormat_);
	av_frame_free(&frame);
	return ret <= 0 ? ret : outSize;
}

void xresampler::close()
{
	std::lock_guard<std::mutex> lg(mutex_);
	if (context_) {
		swr_free(&context_);
	}
}
