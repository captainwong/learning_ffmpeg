#pragma once

#include "ffplay++.h"

typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList* next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList* first_pkt, * last_pkt;
    int nb_packets;
    int size;
    int64_t duration;
    int abort_request;
    int serial;
    SDL_mutex* mutex;
    SDL_cond* cond;

    /* packet queue handling */
    int init()
    {
        memset(this, 0, sizeof(PacketQueue));
        mutex = SDL_CreateMutex();
        if (!mutex) {
            av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
            return AVERROR(ENOMEM);
        }
        cond = SDL_CreateCond();
        if (!cond) {
            av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
            return AVERROR(ENOMEM);
        }
        abort_request = 1;
        return 0;
    }

    int put_private(AVPacket* pkt)
    {
        if (abort_request)
            return -1;

        MyAVPacketList* pkt1 = (MyAVPacketList*)av_malloc(sizeof(MyAVPacketList));
        if (!pkt1)
            return -1;

        pkt1->pkt = *pkt;
        pkt1->next = NULL;
        if (pkt == &ffplay.flush_pkt)
            serial++;
        pkt1->serial = serial;

        if (!last_pkt)
            first_pkt = pkt1;
        else
            last_pkt->next = pkt1;
        last_pkt = pkt1;
        nb_packets++;
        size += pkt1->pkt.size + sizeof(*pkt1);
        duration += pkt1->pkt.duration;
        /* XXX: should duplicate packet data in DV case */
        SDL_CondSignal(cond);
        return 0;
    }

    int put(AVPacket* pkt)
    {
        int ret;

        SDL_LockMutex(mutex);
        ret = put_private(pkt);
        SDL_UnlockMutex(mutex);

        if (pkt != &ffplay.flush_pkt && ret < 0)
            av_packet_unref(pkt);

        return ret;
    }

    int put_nullpacket(int stream_index)
    {
        AVPacket pkt1, * pkt = &pkt1;
        av_init_packet(pkt);
        pkt->data = NULL;
        pkt->size = 0;
        pkt->stream_index = stream_index;
        return put(pkt);
    }
    
    void flush()
    {
        MyAVPacketList* pkt, * pkt1;

        SDL_LockMutex(mutex);
        for (pkt = first_pkt; pkt; pkt = pkt1) {
            pkt1 = pkt->next;
            av_packet_unref(&pkt->pkt);
            av_freep(&pkt);
        }
        last_pkt = NULL;
        first_pkt = NULL;
        nb_packets = 0;
        size = 0;
        duration = 0;
        SDL_UnlockMutex(mutex);
    }

    void destroy()
    {
        flush();
        SDL_DestroyMutex(mutex);
        SDL_DestroyCond(cond);
    }

    void abort()
    {
        SDL_LockMutex(mutex);
        abort_request = 1;
        SDL_CondSignal(cond);
        SDL_UnlockMutex(mutex);
    }

    void start()
    {
        SDL_LockMutex(mutex);
        abort_request = 0;
        put_private(&ffplay.flush_pkt);
        SDL_UnlockMutex(mutex);
    }

    /* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
    int get(AVPacket* pkt, int block, int* serial)
    {
        MyAVPacketList* pkt1;
        int ret;

        SDL_LockMutex(mutex);

        for (;;) {
            if (abort_request) {
                ret = -1;
                break;
            }

            pkt1 = first_pkt;
            if (pkt1) {
                first_pkt = pkt1->next;
                if (!first_pkt)
                    last_pkt = NULL;
                nb_packets--;
                size -= pkt1->pkt.size + sizeof(*pkt1);
                duration -= pkt1->pkt.duration;
                *pkt = pkt1->pkt;
                if (serial)
                    *serial = pkt1->serial;
                av_free(pkt1);
                ret = 1;
                break;
            } else if (!block) {
                ret = 0;
                break;
            } else {
                SDL_CondWait(cond, mutex);
            }
        }
        SDL_UnlockMutex(mutex);
        return ret;
    }
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9

