#include "decoder.h"


void Decoder::init(AVCodecContext* avctx, PacketQueue* queue, SDL_cond* empty_queue_cond)
{
    memset(this, 0, sizeof(Decoder));
    this->avctx = avctx;
    this->queue = queue;
    this->empty_queue_cond = empty_queue_cond;
    start_pts = AV_NOPTS_VALUE;
    pkt_serial = -1;
}

int  Decoder::decode_frame(AVFrame* frame, AVSubtitle* sub)
{
    int ret = AVERROR(EAGAIN);

    for (;;) {
        AVPacket pkt;

        if (queue->serial == pkt_serial) {
            do {
                if (queue->abort_request)
                    return -1;

                switch (avctx->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    ret = avcodec_receive_frame(avctx, frame);
                    if (ret >= 0) {
                        if (ffplay.decoder_reorder_pts == -1) {
                            frame->pts = frame->best_effort_timestamp;
                        } else if (!ffplay.decoder_reorder_pts) {
                            frame->pts = frame->pkt_dts;
                        }
                    }
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    ret = avcodec_receive_frame(avctx, frame);
                    if (ret >= 0) {
                        AVRational tb{ 1, frame->sample_rate };
                        if (frame->pts != AV_NOPTS_VALUE)
                            frame->pts = av_rescale_q(frame->pts, avctx->pkt_timebase, tb);
                        else if (next_pts != AV_NOPTS_VALUE)
                            frame->pts = av_rescale_q(next_pts, next_pts_tb, tb);
                        if (frame->pts != AV_NOPTS_VALUE) {
                            next_pts = frame->pts + frame->nb_samples;
                            next_pts_tb = tb;
                        }
                    }
                    break;
                }
                if (ret == AVERROR_EOF) {
                    finished = pkt_serial;
                    avcodec_flush_buffers(avctx);
                    return 0;
                }
                if (ret >= 0)
                    return 1;
            } while (ret != AVERROR(EAGAIN));
        }

        do {
            if (queue->nb_packets == 0)
                SDL_CondSignal(empty_queue_cond);
            if (packet_pending) {
                av_packet_move_ref(&pkt, &pkt);
                packet_pending = 0;
            } else {
                if (queue->get(&pkt, 1, &pkt_serial) < 0)
                    return -1;
            }
            if (queue->serial == pkt_serial)
                break;
            av_packet_unref(&pkt);
        } while (1);

        if (pkt.data == ffplay.flush_pkt.data) {
            avcodec_flush_buffers(avctx);
            finished = 0;
            next_pts = start_pts;
            next_pts_tb = start_pts_tb;
        } else {
            if (avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
                int got_frame = 0;
                ret = avcodec_decode_subtitle2(avctx, sub, &got_frame, &pkt);
                if (ret < 0) {
                    ret = AVERROR(EAGAIN);
                } else {
                    if (got_frame && !pkt.data) {
                        packet_pending = 1;
                        av_packet_move_ref(&pkt, &pkt);
                    }
                    ret = got_frame ? 0 : (pkt.data ? AVERROR(EAGAIN) : AVERROR_EOF);
                }
            } else {
                if (avcodec_send_packet(avctx, &pkt) == AVERROR(EAGAIN)) {
                    av_log(avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    packet_pending = 1;
                    av_packet_move_ref(&pkt, &pkt);
                }
            }
            av_packet_unref(&pkt);
        }
    }
}

void  Decoder::destroy()
{
    av_packet_unref(&pkt);
    avcodec_free_context(&avctx);
}

int Decoder::start(int (*fn)(void*), const char* thread_name, void* arg)
{
    queue->start();
    decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!decoder_tid) {
        av_log(NULL, AV_LOG_ERROR, "SDL_CreateThread(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}
