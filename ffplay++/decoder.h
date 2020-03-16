#pragma once

#include "ffplay++.h"
#include "packetq.h"


typedef struct Decoder {
    AVPacket pkt;
    PacketQueue* queue;
    AVCodecContext* avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    SDL_cond* empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    SDL_Thread* decoder_tid;


    void init(AVCodecContext* avctx, PacketQueue* queue, SDL_cond* empty_queue_cond);
    int decode_frame(AVFrame* frame, AVSubtitle* sub);
    void destroy(); 
    int start(int (*fn)(void*), const char* thread_name, void* arg);

} Decoder;
