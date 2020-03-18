#include "frameq.h"

int FrameQueue::init(PacketQueue* pktq, int max_size, int keep_last)
{
    memset(this, 0, sizeof(FrameQueue));
    if (!(this->mutex = SDL_CreateMutex())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    if (!(this->cond = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    this->pktq = pktq;
    this->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    this->keep_last = !!keep_last;
    for (int i = 0; i < this->max_size; i++)
        if (!(this->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    return 0;
}

void FrameQueue::destory()
{
    for (int i = 0; i < this->max_size; i++) {
        Frame* vp = &this->queue[i];
        frame_queue_unref_item(vp);
        av_frame_free(&vp->frame);
    }
    SDL_DestroyMutex(this->mutex);
    SDL_DestroyCond(this->cond);
}

Frame* FrameQueue::peek_writable()
{
    /* wait until we have space to put a new frame */
    SDL_LockMutex(mutex);
    while (size >= max_size &&
           !pktq->abort_request) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    if (pktq->abort_request)
        return NULL;

    return &queue[windex];
}

Frame* FrameQueue::peek_readable()
{
    /* wait until we have a readable a new frame */
    SDL_LockMutex(mutex);
    while (size - rindex_shown <= 0 &&
           !pktq->abort_request) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    if (pktq->abort_request)
        return NULL;

    return &queue[(rindex + rindex_shown) % max_size];
}

void FrameQueue::push()
{
    if (++windex == max_size)
        windex = 0;
    SDL_LockMutex(mutex);
    size++;
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
}

void FrameQueue::next()
{
    if (keep_last && !rindex_shown) {
        rindex_shown = 1;
        return;
    }
    frame_queue_unref_item(&queue[rindex]);
    if (++rindex == max_size)
        rindex = 0;
    SDL_LockMutex(mutex);
    size--;
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
}

/* return last shown position */
int64_t FrameQueue::last_pos()
{
    Frame* fp = &queue[rindex];
    if (rindex_shown && fp->serial == pktq->serial)
        return fp->pos;
    else
        return -1;
}

void FrameQueue::decoder_abort(Decoder* d)
{
    d->queue->abort();
    frame_queue_signal();
    SDL_WaitThread(d->decoder_tid, NULL);
    d->decoder_tid = NULL;
    d->queue->flush();
}