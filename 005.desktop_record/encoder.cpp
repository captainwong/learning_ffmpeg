#include "encoder.h"

#pragma warning(disable:4819)

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "avutil.lib")


AVFormatContext* oc = nullptr;
AVCodecContext* ac = nullptr;
AVCodecContext* vc = nullptr;
SwsContext* sws = nullptr;
SwrContext* swr = nullptr;
AVStream* as = nullptr;
AVStream* vs = nullptr;
AVFrame* yuv = nullptr;
AVFrame* pcm = nullptr;
int vpts = 0;
int apts = 0;


encoder::~encoder()
{
}

encoder* encoder::getInstance()
{
	static encoder e;
	return &e;
}

bool encoder::open(const char* file)
{
	close();
	int ret = avformat_alloc_output_context2(&oc, nullptr, nullptr, file);
	if (ret < 0 || !oc) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "could not alloc output context for file '%s': %s\n", file, buf);
		return false;
	}
	fileName_ = file;
	return true;
}

void encoder::close()
{
	if (oc) { avformat_close_input(&oc); }
	if (ac) { avcodec_free_context(&ac); }
	if (vc) { avcodec_free_context(&vc); }
	if (sws) { sws_freeContext(sws); sws = nullptr; }
	if (swr) { swr_free(&swr); }
	if (yuv) { av_frame_free(&yuv); }
	if (pcm) { av_frame_free(&pcm); }
	apts = vpts = 0;
}

bool encoder::addVideoStream(int inWidth, int inHeight, VPixFmt inPixFmt, int outWidth, int outHeight, int outFPS, int outBitRate)
{
	if (!oc) { return false; }

	inWidth_ = inWidth;
	inHeight_ = inHeight;

	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec) {
		fprintf(stderr, "avcodec_find_encoder AV_CODEC_ID_H264 failed\n");
		return false;
	}

	vc = avcodec_alloc_context3(codec);
	if (!vc) {
		fprintf(stderr, "avcodec_alloc_context3 failed\n");
		return false;
	}

	vc->bit_rate = outBitRate;
	vc->width = outWidth;
	vc->height = outHeight;

	vc->time_base = { 1, outFPS };
	vc->framerate = { outFPS, 1 };

	vc->gop_size = 50;
	vc->max_b_frames = 0;

	vc->pix_fmt = AV_PIX_FMT_YUV420P;
	vc->codec_id = AV_CODEC_ID_H264;
	av_opt_set(vc->priv_data, "preset", "superfast", 0);
	vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	int ret = avcodec_open2(vc, codec, nullptr);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avcodec_open2 failed: %s\n", buf);
		return false;
	}

	vs = avformat_new_stream(oc, nullptr);
	if (!vs) {
		fprintf(stderr, "avformat_new_stream failed\n");
		return false;
	}
	vs->codecpar->codec_tag = 0;

	ret = avcodec_parameters_from_context(vs->codecpar, vc); 
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avcodec_parameters_from_context failed: %s\n", buf);
		return false;
	}

	av_dump_format(oc, 0, fileName_.data(), 1);

	auto fmt = (AVPixelFormat)inPixFmt;
	sws = sws_getCachedContext(sws,
							   inWidth, inHeight, fmt,
							   outWidth, outHeight, AV_PIX_FMT_YUV420P,
							   SWS_BICUBIC,
							   nullptr, nullptr, nullptr);
	if (!sws) {
		fprintf(stderr, "sws_getCachedContext failed\n");
		return false;
	}

	if (!yuv) {
		yuv = av_frame_alloc();
		yuv->format = AV_PIX_FMT_YUV420P;
		yuv->width = outWidth;
		yuv->height = outHeight;
		yuv->pts = 0;
		ret = av_frame_get_buffer(yuv, 32);
		if (ret != 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "av_frame_get_buffer failed: %s\n", buf);
			return false;
		}
	}

	return true;
}

bool encoder::addAudioStream(int inSampleRate, int inChannels, ASmpFmt inSmpFmt, int inOutNbSamples, 
							 int outSampleRate, int outChannels, ASmpFmt outSmpFmt, int outBitRate)
{
	if (!oc) { return false; }

	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec) {
		fprintf(stderr, "avcodec_find_encoder AV_CODEC_ID_AAC failed\n");
		return false;
	}

	ac = avcodec_alloc_context3(codec);
	if (!ac) {
		fprintf(stderr, "avcodec_alloc_context3 failed\n");
		return false;
	}

	ac->bit_rate = outBitRate;
	ac->sample_rate = outSampleRate;
	ac->sample_fmt = (AVSampleFormat)outSmpFmt;
	ac->channels = outChannels;
	ac->channel_layout = av_get_default_channel_layout(outChannels);
	ac->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	int ret = avcodec_open2(ac, codec, nullptr);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avcodec_open2 failed: %s\n", buf);
		return false;
	}

	as = avformat_new_stream(oc, nullptr);
	if (!as) {
		fprintf(stderr, "avformat_new_stream failed\n");
		return false;
	}
	as->codecpar->codec_tag = 0;

	ret = avcodec_parameters_from_context(as->codecpar, ac);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avcodec_parameters_from_context failed: %s\n", buf);
		return false;
	}

	av_dump_format(oc, 0, fileName_.data(), 1);

	swr = swr_alloc_set_opts(swr,
							 ac->channel_layout, ac->sample_fmt, ac->sample_rate,
							 av_get_default_channel_layout(inChannels), (AVSampleFormat)inSmpFmt, inSampleRate,
							 0, nullptr);
	ret = swr_init(swr);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "swr_init failed: %s\n", buf);
		return false;
	}

	if (!pcm) {
		pcm = av_frame_alloc();
		pcm->format = ac->sample_fmt;
		pcm->channels = ac->channels;
		pcm->channel_layout = ac->channel_layout;
		pcm->nb_samples = inOutNbSamples;

		ret = av_frame_get_buffer(pcm, 0);
		if (ret != 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "av_frame_get_buffer failed: %s\n", buf);
			return false;
		}
	}

	return true;
}

AVPacket* encoder::encodeVideo(char* bgra)
{
	if (!oc || !vs || !sws || !yuv) { return nullptr; }

	const uint8_t* data[AV_NUM_DATA_POINTERS] = { nullptr };
	data[0] = (const uint8_t*)bgra;

	int linesize[AV_NUM_DATA_POINTERS] = { 0 };
	linesize[0] = inWidth_ * 4;

	int ret = sws_scale(sws, 
						data, linesize, 0, inHeight_,
						yuv->data, yuv->linesize);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "sws_scale failed: %s\n", buf);
		return nullptr;
	}

	yuv->pts = vpts++;

	ret = avcodec_send_frame(vc, yuv);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avcodec_send_frame failed: %s\n", buf);
		return nullptr;
	}

	AVPacket* pkt = av_packet_alloc();
	ret = avcodec_receive_packet(vc, pkt);
	if (ret != 0 || pkt->size <= 0) {
		av_packet_free(&pkt);
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avcodec_receive_packet failed: %s\n", buf);
		return nullptr;
	}

	av_packet_rescale_ts(pkt, vc->time_base, vs->time_base);
	pkt->stream_index = vs->index;
	return pkt;
}

AVPacket* encoder::encodeAudio(char* d)
{
	if (!oc || !as || !swr || !pcm) { return nullptr; }

	const uint8_t* data[AV_NUM_DATA_POINTERS] = { nullptr };
	data[0] = (const uint8_t*)d;
	int ret = swr_convert(swr, 
						  pcm->data, pcm->nb_samples,
						  data, pcm->nb_samples);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "swr_convert failed: %s\n", buf);
		return nullptr;
	}

	ret = avcodec_send_frame(ac, pcm);
	if (ret != 0) { return nullptr; }

	AVPacket* pkt = av_packet_alloc();
	av_init_packet(pkt);
	ret = avcodec_receive_packet(ac, pkt);
	if (ret != 0) {
		av_packet_free(&pkt);
		return nullptr;
	}

	pkt->stream_index = as->index;
	pkt->pts = pkt->dts = apts;
	apts += av_rescale_q(pcm->nb_samples, { 1, ac->sample_rate }, ac->time_base);

	return pkt;
}

bool encoder::writeHeader()
{
	if (!oc) { return false; }
	int ret = avio_open(&oc->pb, fileName_.data(), AVIO_FLAG_WRITE);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avio_open '%s' failed: %s\n", fileName_.data(), buf);
		return false;
	}

	ret = avformat_write_header(oc, nullptr);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avformat_write_header failed: %s\n", buf);
		return false;
	}

	return true;
}

bool encoder::writeFrame(AVPacket* pkt)
{
	if (!oc || !pkt || pkt->size <= 0) { return false; }
	int ret = av_interleaved_write_frame(oc, pkt);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "av_interleaved_write_frame failed: %s\n", buf);
		return false;
	}
	return true;
}

bool encoder::writeTrailer()
{
	if (!oc || !oc->pb) { return false; }
	int ret = av_write_trailer(oc);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "av_write_trailer failed: %s\n", buf);
		return false;
	}

	ret = avio_closep(&oc->pb);
	if (ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, buf, sizeof(buf) - 1);
		fprintf(stderr, "avio_closep failed: %s\n", buf);
		return false;
	}

	return true;
}

bool encoder::isVideoBeforeAudio()
{
	if (!oc || !as || !vs) { return false; }
	int ret = av_compare_ts(vpts, vc->time_base, apts, ac->time_base);
	return ret <= 0;
}
