#include "../ffmpeg.h"
#include "../sdl.h"
#include <assert.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

/**
 * 准备 ffmpeg
 * @param url 视频路径
 */
void preparFFmpeg(const char* url);

/**
 * 释放内存
 */
void free();

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio   48000 * (32/8)

//一帧PCM的数据长度
unsigned int audioLen = 0;
unsigned char* audioChunk = nullptr;
//当前读取的位置
unsigned char* audioPos = nullptr;

/** 被SDL2调用的回调函数 当需要获取数据喂入硬件播放的时候调用 **/
void fill_audio(void* codecContext, Uint8* stream, int len) {
    //SDL2中必须首先使用SDL_memset()将stream中的数据设置为0
    SDL_memset(stream, 0, len);
    if (audioLen == 0)
        return;

    len = (len > audioLen ? audioLen : len);
    //将数据合并到 stream 里
    SDL_MixAudio(stream, audioPos, len, SDL_MIX_MAXVOLUME);

    //一帧的数据控制
    audioPos += len;
    audioLen -= len;
}



/** ########### SDL初始化 ############## **/
/** 自己想要的输出的音频格式 **/
SDL_AudioSpec wantSpec;
/** 重采样上下文 **/
SwrContext* auConvertContext;

/** ########### FFmpeg 相关 ############# **/
AVFormatContext* formatContext;
AVCodecContext* codecContext;
AVCodec* codec;
AVPacket* packet;
AVFrame* frame;
int audioIndex = -1;

uint64_t out_chn_layout = AV_CH_LAYOUT_STEREO;  //输出的通道布局 双声道
enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; //输出的声音格式
int out_sample_rate = 44100;   //输出的采样率
int out_nb_samples = -1;        //输出的音频采样
int out_channels = -1;        //输出的通道数
int out_buffer_size = -1;   //输出buff大小
unsigned char* outBuff = NULL;//输出的Buffer数据
uint64_t in_chn_layout = -1;  //输入的通道布局

/** 外部调用方法 **/
void playAudio(const char* url) {
    /** ########### 初始化FFmpeg ############# **/
    preparFFmpeg(url);
    /** ########## 获取实际音频的参数 ##########**/
    //单个通道中的采样数
    out_nb_samples = codecContext->frame_size;
    //输出的声道数
    out_channels = av_get_channel_layout_nb_channels(out_chn_layout);
    //输出音频的布局
    in_chn_layout = av_get_default_channel_layout(codecContext->channels);

    /** 计算重采样后的实际数据大小,并分配空间 **/
    //计算输出的buffer的大小
    out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
    //分配输出buffer的空间
    outBuff = (unsigned char*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2); //双声道

    //初始化SDL中自己想设置的参数
    wantSpec.freq = out_sample_rate;
    wantSpec.format = AUDIO_S16SYS;
    wantSpec.channels = out_channels;
    wantSpec.silence = 0;
    wantSpec.samples = out_nb_samples;
    wantSpec.callback = fill_audio;
    wantSpec.userdata = codecContext;

    //打开音频之后wantSpec的值可能会有改动，返回实际设备的参数值
    if (SDL_OpenAudio(&wantSpec, NULL) < 0) {
        cout << "[error] open audio error" << endl;
        return;
    }
    //初始化重采样器
    auConvertContext = swr_alloc_set_opts(NULL,
                                          out_chn_layout,
                                          out_sample_fmt,
                                          out_sample_rate,
                                          in_chn_layout,
                                          codecContext->sample_fmt,
                                          codecContext->sample_rate,
                                          0,
                                          NULL);
    //初始化SwResample的Context
    swr_init(auConvertContext);

    //开始播放 调用这个方法硬件才会开始播放
    SDL_PauseAudio(0);

    //循环读取packet并且解码
    int sendcode = 0;
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index != audioIndex)continue;
        //接受解码后的音频数据
        while (avcodec_receive_frame(codecContext, frame) == 0) {
            swr_convert(auConvertContext, &outBuff, MAX_AUDIO_FRAME_SIZE, (const uint8_t**)frame->data,
                        frame->nb_samples);
            //如果没有播放完就等待1ms
            while (audioLen > 0)
                SDL_Delay(1);
            //同步数据
            audioChunk = (unsigned char*)outBuff;
            audioPos = audioChunk;
            audioLen = out_buffer_size;
        }
        //发送解码前的包数据
        sendcode = avcodec_send_packet(codecContext, packet);
        //根据发送的返回值判断状态
        if (sendcode == 0) {
            cout << "[debug] " << "SUCCESS" << endl;
        } else if (sendcode == AVERROR_EOF) {
            cout << "[debug] " << "EOF" << endl;
        } else if (sendcode == AVERROR(EAGAIN)) {
            cout << "[debug] " << "EAGAIN" << endl;
        } else {
            //cout << "[debug] " << av_err2str(AVERROR(sendcode)) << endl;
        }

        av_packet_unref(packet);
    }


}

/** 准备FFmpeg **/
void preparFFmpeg(const char* url) {
    int retcode;
    //初始化FormatContext
    formatContext = avformat_alloc_context();
    if (!formatContext) {
        cout << "[error] alloc format context error!" << endl;
        return;
    }

    //打开输入流
    retcode = avformat_open_input(&formatContext, url, nullptr, nullptr);
    if (retcode != 0) {
        cout << "[error] open input error!" << endl;
        return;
    }

    //读取媒体文件信息
    retcode = avformat_find_stream_info(formatContext, NULL);
    if (retcode != 0) {
        cout << "[error] find stream error!" << endl;
        return;
    }

    //分配codecContext
    codecContext = avcodec_alloc_context3(NULL);
    if (!codecContext) {
        cout << "[error] alloc codec context error!" << endl;
        return;
    }

    //寻找到音频流的下标
    audioIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    //将视频流的的编解码信息拷贝到codecContext中
    retcode = avcodec_parameters_to_context(codecContext, formatContext->streams[audioIndex]->codecpar);
    if (retcode != 0) {
        cout << "[error] parameters to context error!" << endl;
        return;
    }

    //查找解码器
    codec = avcodec_find_decoder(codecContext->codec_id);
    if (codec == nullptr) {
        cout << "[error] find decoder error!" << endl;
        return;
    }

    //打开解码器
    retcode = avcodec_open2(codecContext, codec, nullptr);
    if (retcode != 0) {
        cout << "[error] open decodec error!" << endl;
        return;
    }

    //初始化一个packet
    packet = av_packet_alloc();
    //初始化一个Frame
    frame = av_frame_alloc();

}

void free() {
    if (formatContext != nullptr) avformat_close_input(&formatContext);
    if (codecContext != nullptr) avcodec_free_context(&codecContext);
    if (packet != nullptr) av_packet_free(&packet);
    if (frame != nullptr) av_frame_free(&frame);
    if (auConvertContext != nullptr) swr_free(&auConvertContext);
    SDL_CloseAudio();
    SDL_Quit();

}


int main()
{
    playAudio(R"(F:\CloudMusic\TsunaAwakes.mp3)");
}