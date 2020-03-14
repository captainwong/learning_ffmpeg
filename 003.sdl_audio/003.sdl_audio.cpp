#include "../ffmpeg.h"
#include "../sdl.h"
#include <assert.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

/**
 * ׼�� ffmpeg
 * @param url ��Ƶ·��
 */
void preparFFmpeg(const char* url);

/**
 * �ͷ��ڴ�
 */
void free();

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio   48000 * (32/8)

//һ֡PCM�����ݳ���
unsigned int audioLen = 0;
unsigned char* audioChunk = nullptr;
//��ǰ��ȡ��λ��
unsigned char* audioPos = nullptr;

/** ��SDL2���õĻص����� ����Ҫ��ȡ����ι��Ӳ�����ŵ�ʱ����� **/
void fill_audio(void* codecContext, Uint8* stream, int len) {
    //SDL2�б�������ʹ��SDL_memset()��stream�е���������Ϊ0
    SDL_memset(stream, 0, len);
    if (audioLen == 0)
        return;

    len = (len > audioLen ? audioLen : len);
    //�����ݺϲ��� stream ��
    SDL_MixAudio(stream, audioPos, len, SDL_MIX_MAXVOLUME);

    //һ֡�����ݿ���
    audioPos += len;
    audioLen -= len;
}



/** ########### SDL��ʼ�� ############## **/
/** �Լ���Ҫ���������Ƶ��ʽ **/
SDL_AudioSpec wantSpec;
/** �ز��������� **/
SwrContext* auConvertContext;

/** ########### FFmpeg ��� ############# **/
AVFormatContext* formatContext;
AVCodecContext* codecContext;
AVCodec* codec;
AVPacket* packet;
AVFrame* frame;
int audioIndex = -1;

uint64_t out_chn_layout = AV_CH_LAYOUT_STEREO;  //�����ͨ������ ˫����
enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; //�����������ʽ
int out_sample_rate = 44100;   //����Ĳ�����
int out_nb_samples = -1;        //�������Ƶ����
int out_channels = -1;        //�����ͨ����
int out_buffer_size = -1;   //���buff��С
unsigned char* outBuff = NULL;//�����Buffer����
uint64_t in_chn_layout = -1;  //�����ͨ������

/** �ⲿ���÷��� **/
void playAudio(const char* url) {
    /** ########### ��ʼ��FFmpeg ############# **/
    preparFFmpeg(url);
    /** ########## ��ȡʵ����Ƶ�Ĳ��� ##########**/
    //����ͨ���еĲ�����
    out_nb_samples = codecContext->frame_size;
    //�����������
    out_channels = av_get_channel_layout_nb_channels(out_chn_layout);
    //�����Ƶ�Ĳ���
    in_chn_layout = av_get_default_channel_layout(codecContext->channels);

    /** �����ز������ʵ�����ݴ�С,������ռ� **/
    //���������buffer�Ĵ�С
    out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
    //�������buffer�Ŀռ�
    outBuff = (unsigned char*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2); //˫����

    //��ʼ��SDL���Լ������õĲ���
    wantSpec.freq = out_sample_rate;
    wantSpec.format = AUDIO_S16SYS;
    wantSpec.channels = out_channels;
    wantSpec.silence = 0;
    wantSpec.samples = out_nb_samples;
    wantSpec.callback = fill_audio;
    wantSpec.userdata = codecContext;

    //����Ƶ֮��wantSpec��ֵ���ܻ��иĶ�������ʵ���豸�Ĳ���ֵ
    if (SDL_OpenAudio(&wantSpec, NULL) < 0) {
        cout << "[error] open audio error" << endl;
        return;
    }
    //��ʼ���ز�����
    auConvertContext = swr_alloc_set_opts(NULL,
                                          out_chn_layout,
                                          out_sample_fmt,
                                          out_sample_rate,
                                          in_chn_layout,
                                          codecContext->sample_fmt,
                                          codecContext->sample_rate,
                                          0,
                                          NULL);
    //��ʼ��SwResample��Context
    swr_init(auConvertContext);

    //��ʼ���� �����������Ӳ���ŻῪʼ����
    SDL_PauseAudio(0);

    //ѭ����ȡpacket���ҽ���
    int sendcode = 0;
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index != audioIndex)continue;
        //���ܽ�������Ƶ����
        while (avcodec_receive_frame(codecContext, frame) == 0) {
            swr_convert(auConvertContext, &outBuff, MAX_AUDIO_FRAME_SIZE, (const uint8_t**)frame->data,
                        frame->nb_samples);
            //���û�в�����͵ȴ�1ms
            while (audioLen > 0)
                SDL_Delay(1);
            //ͬ������
            audioChunk = (unsigned char*)outBuff;
            audioPos = audioChunk;
            audioLen = out_buffer_size;
        }
        //���ͽ���ǰ�İ�����
        sendcode = avcodec_send_packet(codecContext, packet);
        //���ݷ��͵ķ���ֵ�ж�״̬
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

/** ׼��FFmpeg **/
void preparFFmpeg(const char* url) {
    int retcode;
    //��ʼ��FormatContext
    formatContext = avformat_alloc_context();
    if (!formatContext) {
        cout << "[error] alloc format context error!" << endl;
        return;
    }

    //��������
    retcode = avformat_open_input(&formatContext, url, nullptr, nullptr);
    if (retcode != 0) {
        cout << "[error] open input error!" << endl;
        return;
    }

    //��ȡý���ļ���Ϣ
    retcode = avformat_find_stream_info(formatContext, NULL);
    if (retcode != 0) {
        cout << "[error] find stream error!" << endl;
        return;
    }

    //����codecContext
    codecContext = avcodec_alloc_context3(NULL);
    if (!codecContext) {
        cout << "[error] alloc codec context error!" << endl;
        return;
    }

    //Ѱ�ҵ���Ƶ�����±�
    audioIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    //����Ƶ���ĵı������Ϣ������codecContext��
    retcode = avcodec_parameters_to_context(codecContext, formatContext->streams[audioIndex]->codecpar);
    if (retcode != 0) {
        cout << "[error] parameters to context error!" << endl;
        return;
    }

    //���ҽ�����
    codec = avcodec_find_decoder(codecContext->codec_id);
    if (codec == nullptr) {
        cout << "[error] find decoder error!" << endl;
        return;
    }

    //�򿪽�����
    retcode = avcodec_open2(codecContext, codec, nullptr);
    if (retcode != 0) {
        cout << "[error] open decodec error!" << endl;
        return;
    }

    //��ʼ��һ��packet
    packet = av_packet_alloc();
    //��ʼ��һ��Frame
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