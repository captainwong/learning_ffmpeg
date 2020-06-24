#include "ffmpeg.h"
#include "cmdutils.h"

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <atomic>
#include <signal.h>


extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/bprint.h>
}

#if HAVE_IO_H
#include <io.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_RESOURCE_H
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#elif HAVE_GETPROCESSTIMES
#include <windows.h>
#endif
#if HAVE_GETPROCESSMEMORYINFO
#include <windows.h>
#include <psapi.h>
#endif
#if HAVE_SETCONSOLECTRLHANDLER
#include <windows.h>
#endif


// 仅在本文件内使用的资源
struct FFmpegLocalResource {
    FILE* vstats_file = nullptr;
    uint8_t* subtitle_out = nullptr;
    volatile int received_sigterm = 0;
    volatile int received_nb_signals = 0;
    std::atomic_int transcode_init_done = 0;
    volatile int ffmpeg_exited = 0;
    int main_return_code = 0;
};

FFmpegResource g_ffmpeg_resource = {};
FFmpegLocalResource g_ffmpeg_local_resource = {};
FFmpegOption g_ffmpeg_option = {};

static int decode_interrupt_cb(void* ctx)
{
    return g_ffmpeg_local_resource.received_nb_signals > g_ffmpeg_local_resource.transcode_init_done.load();
}

FFmpegConstResource::FFmpegConstResource()
    : int_cb(AVIOInterruptCB{ decode_interrupt_cb, nullptr })
{

}

FFmpegConstResource g_ffmpeg_const_resource = {};


/* sub2video hack:
   Convert subtitles to video with alpha to insert them in filter graphs.
   This is a temporary solution until libavfilter gets real subtitles support.
 */

static int sub2video_get_blank_frame(InputStream* ist)
{
    int ret;
    AVFrame* frame = ist->sub2video.frame;

    av_frame_unref(frame);
    ist->sub2video.frame->width = ist->dec_ctx->width ? ist->dec_ctx->width : ist->sub2video.w;
    ist->sub2video.frame->height = ist->dec_ctx->height ? ist->dec_ctx->height : ist->sub2video.h;
    ist->sub2video.frame->format = AV_PIX_FMT_RGB32;
    if ((ret = av_frame_get_buffer(frame, 32)) < 0)
        return ret;
    memset(frame->data[0], 0, frame->height * frame->linesize[0]);
    return 0;
}

static void sub2video_copy_rect(uint8_t* dst, int dst_linesize, int w, int h,
                                AVSubtitleRect* r)
{
    uint32_t* pal, * dst2;
    uint8_t* src, * src2;
    int x, y;

    if (r->type != SUBTITLE_BITMAP) {
        av_log(NULL, AV_LOG_WARNING, "sub2video: non-bitmap subtitle\n");
        return;
    }
    if (r->x < 0 || r->x + r->w > w || r->y < 0 || r->y + r->h > h) {
        av_log(NULL, AV_LOG_WARNING, "sub2video: rectangle (%d %d %d %d) overflowing %d %d\n",
               r->x, r->y, r->w, r->h, w, h
        );
        return;
    }

    dst += r->y * dst_linesize + r->x * 4;
    src = r->data[0];
    pal = (uint32_t*)r->data[1];
    for (y = 0; y < r->h; y++) {
        dst2 = (uint32_t*)dst;
        src2 = src;
        for (x = 0; x < r->w; x++)
            *(dst2++) = pal[*(src2++)];
        dst += dst_linesize;
        src += r->linesize[0];
    }
}

static void sub2video_push_ref(InputStream* ist, int64_t pts)
{
    AVFrame* frame = ist->sub2video.frame;
    int i;
    int ret;

    av_assert1(frame->data[0]);
    ist->sub2video.last_pts = frame->pts = pts;
    for (i = 0; i < ist->nb_filters; i++) {
        ret = av_buffersrc_add_frame_flags(ist->filters[i]->filter, frame,
                                           AV_BUFFERSRC_FLAG_KEEP_REF |
                                           AV_BUFFERSRC_FLAG_PUSH);
        if (ret != AVERROR_EOF && ret < 0)
            av_log(NULL, AV_LOG_WARNING, "Error while add the frame to buffer source(%s).\n",
                   cppav_err2str(ret));
    }
}

void sub2video_update(InputStream* ist, AVSubtitle* sub)
{
    AVFrame* frame = ist->sub2video.frame;
    int8_t* dst;
    int     dst_linesize;
    int num_rects, i;
    int64_t pts, end_pts;

    if (!frame)
        return;
    if (sub) {
        pts = av_rescale_q(sub->pts + sub->start_display_time * 1000LL,
                           CPPAV_TIME_BASE_Q, ist->st->time_base);
        end_pts = av_rescale_q(sub->pts + sub->end_display_time * 1000LL,
                               CPPAV_TIME_BASE_Q, ist->st->time_base);
        num_rects = sub->num_rects;
    } else {
        pts = ist->sub2video.end_pts;
        end_pts = INT64_MAX;
        num_rects = 0;
    }
    if (sub2video_get_blank_frame(ist) < 0) {
        av_log(ist->dec_ctx, AV_LOG_ERROR,
               "Impossible to get a blank canvas.\n");
        return;
    }
    dst = (int8_t*) frame->data[0];
    dst_linesize = frame->linesize[0];
    for (i = 0; i < num_rects; i++)
        sub2video_copy_rect((uint8_t*)dst, dst_linesize, frame->width, frame->height, sub->rects[i]);
    sub2video_push_ref(ist, pts);
    ist->sub2video.end_pts = end_pts;
}

static void sub2video_heartbeat(InputStream* ist, int64_t pts)
{
    InputFile* infile = g_ffmpeg_resource. input_files[ist->file_index];
    int i, j, nb_reqs;
    int64_t pts2;

    /* When a frame is read from a file, examine all sub2video streams in
       the same file and send the sub2video frame again. Otherwise, decoded
       video frames could be accumulating in the filter graph while a filter
       (possibly overlay) is desperately waiting for a subtitle frame. */
    for (i = 0; i < infile->nb_streams; i++) {
        InputStream* ist2 = g_ffmpeg_resource.input_streams[infile->ist_index + i];
        if (!ist2->sub2video.frame)
            continue;
        /* subtitles seem to be usually muxed ahead of other streams;
           if not, subtracting a larger time here is necessary */
        pts2 = av_rescale_q(pts, ist->st->time_base, ist2->st->time_base) - 1;
        /* do not send the heartbeat frame if the subtitle is already ahead */
        if (pts2 <= ist2->sub2video.last_pts)
            continue;
        if (pts2 >= ist2->sub2video.end_pts ||
            (!ist2->sub2video.frame->data[0] && ist2->sub2video.end_pts < INT64_MAX))
            sub2video_update(ist2, NULL);
        for (j = 0, nb_reqs = 0; j < ist2->nb_filters; j++)
            nb_reqs += av_buffersrc_get_nb_failed_requests(ist2->filters[j]->filter);
        if (nb_reqs)
            sub2video_push_ref(ist2, pts2);
    }
}

static void sub2video_flush(InputStream* ist)
{
    int i;
    int ret;

    if (ist->sub2video.end_pts < INT64_MAX)
        sub2video_update(ist, NULL);
    for (i = 0; i < ist->nb_filters; i++) {
        ret = av_buffersrc_add_frame(ist->filters[i]->filter, NULL);
        if (ret != AVERROR_EOF && ret < 0)
            av_log(NULL, AV_LOG_WARNING, "Flush the frame error.\n");
    }
}

/* end of sub2video hack */


static void term_exit_sigsafe(void)
{
#if HAVE_TERMIOS_H
    if (restore_tty)
        tcsetattr(0, TCSANOW, &oldtty);
#endif
}

void term_exit(void)
{
    av_log(NULL, AV_LOG_QUIET, "%s", "");
    term_exit_sigsafe();
}

static void sigterm_handler(int sig)
{
    int ret;
    g_ffmpeg_local_resource.received_sigterm = sig;
    g_ffmpeg_local_resource.received_nb_signals++;
    term_exit_sigsafe();
    if (g_ffmpeg_local_resource.received_nb_signals > 3) {
        ret = write(2/*STDERR_FILENO*/, "Received > 3 system signals, hard exiting\n",
                    strlen("Received > 3 system signals, hard exiting\n"));
        if (ret < 0) { /* Do nothing */ };
        exit(123);
    }
}

#if HAVE_SETCONSOLECTRLHANDLER
static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    av_log(NULL, AV_LOG_DEBUG, "\nReceived windows signal %ld\n", fdwCtrlType);

    switch (fdwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        sigterm_handler(SIGINT);
        return TRUE;

    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        sigterm_handler(SIGTERM);
        /* Basically, with these 3 events, when we return from this method the
           process is hard terminated, so stall as long as we need to
           to try and let the main thread(s) clean up and gracefully terminate
           (we have at most 5 seconds, but should be done far before that). */
        while (!g_ffmpeg_local_resource.ffmpeg_exited) {
            Sleep(0);
        }
        return TRUE;

    default:
        av_log(NULL, AV_LOG_ERROR, "Received unknown windows signal %ld\n", fdwCtrlType);
        return FALSE;
    }
}
#endif


void term_init(void)
{
#if HAVE_TERMIOS_H
    if (!run_as_daemon && stdin_interaction) {
        struct termios tty;
        if (tcgetattr(0, &tty) == 0) {
            oldtty = tty;
            restore_tty = 1;

            tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                             | INLCR | IGNCR | ICRNL | IXON);
            tty.c_oflag |= OPOST;
            tty.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
            tty.c_cflag &= ~(CSIZE | PARENB);
            tty.c_cflag |= CS8;
            tty.c_cc[VMIN] = 1;
            tty.c_cc[VTIME] = 0;

            tcsetattr(0, TCSANOW, &tty);
        }
        signal(SIGQUIT, sigterm_handler); /* Quit (POSIX).  */
    }
#endif

    signal(SIGINT, sigterm_handler); /* Interrupt (ANSI).    */
    signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */
#ifdef SIGXCPU
    signal(SIGXCPU, sigterm_handler);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN); /* Broken pipe (POSIX). */
#endif
#if HAVE_SETCONSOLECTRLHANDLER
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif
}


typedef struct BenchmarkTimeStamps {
    int64_t real_usec;
    int64_t user_usec;
    int64_t sys_usec;
} BenchmarkTimeStamps;


static int64_t getmaxrss(void)
{
#if HAVE_GETRUSAGE && HAVE_STRUCT_RUSAGE_RU_MAXRSS
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
    return (int64_t)rusage.ru_maxrss * 1024;
#elif HAVE_GETPROCESSMEMORYINFO
    HANDLE proc;
    PROCESS_MEMORY_COUNTERS memcounters;
    proc = GetCurrentProcess();
    memcounters.cb = sizeof(memcounters);
    GetProcessMemoryInfo(proc, &memcounters, sizeof(memcounters));
    return memcounters.PeakPagefileUsage;
#else
    return 0;
#endif
}

static void ffmpeg_cleanup(int ret)
{
    if (g_ffmpeg_option.do_benchmark) {
        int maxrss = (int)(getmaxrss() / 1024);
        av_log(NULL, AV_LOG_INFO, "bench: maxrss=%iKB\n", maxrss);
    }

    for (int i = 0; i < g_ffmpeg_resource.nb_filtergraphs; i++) {
        FilterGraph* fg = g_ffmpeg_resource.filtergraphs[i];
        avfilter_graph_free(&fg->graph);
        for (int j = 0; j < fg->nb_inputs; j++) {
            while (av_fifo_size(fg->inputs[j]->frame_queue)) {
                AVFrame* frame = nullptr;
                av_fifo_generic_read(fg->inputs[j]->frame_queue, &frame, sizeof(frame), nullptr);
                av_frame_free(&frame);
            }
            av_fifo_freep(&fg->inputs[j]->frame_queue);
            if (fg->inputs[j]->ist->sub2video.sub_queue) {
                while (av_fifo_size(fg->inputs[j]->ist->sub2video.sub_queue)) {
                    AVSubtitle sub;
                    av_fifo_generic_read(fg->inputs[j]->ist->sub2video.sub_queue, &sub, sizeof(sub), nullptr);
                    avsubtitle_free(&sub);
                }
                av_fifo_freep(&fg->inputs[j]->ist->sub2video.sub_queue);
            }
            av_buffer_unref(&fg->inputs[j]->hw_frames_ctx);
            av_freep(&fg->inputs[j]->name);
            av_freep(&fg->inputs[j]);
        }
        av_freep(&fg->inputs);

        for (int j = 0; j < fg->nb_outputs; j++) {
            av_freep(&fg->outputs[j]->name);
            av_freep(&fg->outputs[j]->formats);
            av_freep(&fg->outputs[j]->channel_layouts);
            av_freep(&fg->outputs[j]->sample_rates);
            av_freep(&fg->outputs[j]);
        }

        av_freep(&fg->outputs);
        av_freep(&fg->graph_desc);

        av_freep(&g_ffmpeg_resource.filtergraphs[i]);
    }

    av_freep(&g_ffmpeg_resource.filtergraphs);

    av_freep(&g_ffmpeg_local_resource.subtitle_out);

    /* close files */
    for (int i = 0; i < g_ffmpeg_resource.nb_output_files; i++) {
        OutputFile* of = g_ffmpeg_resource.output_files[i];
        AVFormatContext* s;
        if (!of)
            continue;
        s = of->ctx;
        if (s && s->oformat && !(s->oformat->flags & AVFMT_NOFILE))
            avio_closep(&s->pb);
        avformat_free_context(s);
        av_dict_free(&of->opts);

        av_freep(&g_ffmpeg_resource.output_files[i]);
    }
    for (int i = 0; i < g_ffmpeg_resource.nb_output_streams; i++) {
        OutputStream* ost = g_ffmpeg_resource.output_streams[i];

        if (!ost)
            continue;

        for (int j = 0; j < ost->nb_bitstream_filters; j++)
            av_bsf_free(&ost->bsf_ctx[j]);
        av_freep(&ost->bsf_ctx);

        av_frame_free(&ost->filtered_frame);
        av_frame_free(&ost->last_frame);
        av_dict_free(&ost->encoder_opts);

        av_freep(&ost->forced_keyframes);
        av_expr_free(ost->forced_keyframes_pexpr);
        av_freep(&ost->avfilter);
        av_freep(&ost->logfile_prefix);

        av_freep(&ost->audio_channels_map);
        ost->audio_channels_mapped = 0;

        av_dict_free(&ost->sws_dict);

        avcodec_free_context(&ost->enc_ctx);
        avcodec_parameters_free(&ost->ref_par);

        if (ost->muxing_queue) {
            while (av_fifo_size(ost->muxing_queue)) {
                AVPacket pkt;
                av_fifo_generic_read(ost->muxing_queue, &pkt, sizeof(pkt), NULL);
                av_packet_unref(&pkt);
            }
            av_fifo_freep(&ost->muxing_queue);
        }

        av_freep(&g_ffmpeg_resource.output_streams[i]);
    }
#if HAVE_THREADS
    free_input_threads();
#endif
    for (int i = 0; i < g_ffmpeg_resource.nb_input_files; i++) {
        avformat_close_input(&g_ffmpeg_resource.input_files[i]->ctx);
        av_freep(&g_ffmpeg_resource.input_files[i]);
    }
    for (int i = 0; i < g_ffmpeg_resource.nb_input_streams; i++) {
        InputStream* ist = g_ffmpeg_resource.input_streams[i];

        av_frame_free(&ist->decoded_frame);
        av_frame_free(&ist->filter_frame);
        av_dict_free(&ist->decoder_opts);
        avsubtitle_free(&ist->prev_sub.subtitle);
        av_frame_free(&ist->sub2video.frame);
        av_freep(&ist->filters);
        av_freep(&ist->hwaccel_device);
        av_freep(&ist->dts_buffer);

        avcodec_free_context(&ist->dec_ctx);

        av_freep(&g_ffmpeg_resource.input_streams[i]);
    }

    if (g_ffmpeg_local_resource.vstats_file) {
        if (fclose(g_ffmpeg_local_resource.vstats_file))
            av_log(NULL, AV_LOG_ERROR,
                   "Error closing vstats file, loss of information possible: %s\n",
                   cppav_err2str(AVERROR(errno)));
    }
    av_freep(&g_ffmpeg_option.vstats_filename);

    av_freep(&g_ffmpeg_resource.input_streams);
    av_freep(&g_ffmpeg_resource.input_files);
    av_freep(&g_ffmpeg_resource.output_streams);
    av_freep(&g_ffmpeg_resource.output_files);

    uninit_opts();

    avformat_network_deinit();

    if (g_ffmpeg_local_resource.received_sigterm) {
        av_log(NULL, AV_LOG_INFO, "Exiting normally, received signal %d.\n",
               (int)g_ffmpeg_local_resource.received_sigterm);
    } else if (ret && g_ffmpeg_local_resource.transcode_init_done.load()) {
        av_log(NULL, AV_LOG_INFO, "Conversion failed!\n");
    }
    term_exit();
    g_ffmpeg_local_resource.ffmpeg_exited = 1;
}


int guess_input_channel_layout(InputStream* ist)
{
    AVCodecContext* dec = ist->dec_ctx;

    if (!dec->channel_layout) {
        char layout_name[256];

        if (dec->channels > ist->guess_layout_max)
            return 0;
        dec->channel_layout = av_get_default_channel_layout(dec->channels);
        if (!dec->channel_layout)
            return 0;
        av_get_channel_layout_string(layout_name, sizeof(layout_name),
                                     dec->channels, dec->channel_layout);
        av_log(NULL, AV_LOG_WARNING, "Guessed Channel Layout for Input Stream "
               "#%d.%d : %s\n", ist->file_index, ist->st->index, layout_name);
    }
    return 1;
}

void remove_avoptions(AVDictionary** a, AVDictionary* b)
{
    AVDictionaryEntry* t = NULL;

    while ((t = av_dict_get(b, "", t, AV_DICT_IGNORE_SUFFIX))) {
        av_dict_set(a, t->key, NULL, AV_DICT_MATCH_CASE);
    }
}

void assert_avoptions(AVDictionary* m)
{
    AVDictionaryEntry* t;
    if ((t = av_dict_get(m, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_FATAL, "Option %s not found.\n", t->key);
        exit_program(1);
    }
}


int main(int argc, char** argv)
{
    init_dynload();
    register_exit(ffmpeg_cleanup);
    setvbuf(stderr, NULL, _IONBF, 0); /* win32 runtime needs this */
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);
    
    exit_program(g_ffmpeg_local_resource.main_return_code);
    return g_ffmpeg_local_resource.main_return_code;
}
