#pragma once

#include "ffplay++.h"
#include "packetq.h"
#include "decoder.h"

#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame* frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
} Frame;

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex* mutex;
    SDL_cond* cond;
    PacketQueue* pktq;


    static void frame_queue_unref_item(Frame* vp){
        av_frame_unref(vp->frame);
        avsubtitle_free(&vp->sub);
    }

    int init(PacketQueue* pktq, int max_size, int keep_last);
    void destory();

    void frame_queue_signal() {
        SDL_LockMutex(mutex);
        SDL_CondSignal(cond);
        SDL_UnlockMutex(mutex);
    }

    Frame* peek() { return &queue[(rindex + rindex_shown) % max_size]; }
    Frame* peek_next() { return &queue[(rindex + rindex_shown + 1) % max_size]; }
    Frame* peek_last() { return &queue[rindex]; }
    Frame* peek_writable();
    Frame* peek_readable();
    void push();
    void next();
    /* return the number of undisplayed frames in the queue */
    int nb_remaining() { return size - rindex_shown; }
    /* return last shown position */
    int64_t last_pos();
    void decoder_abort(Decoder* d);
} FrameQueue;
