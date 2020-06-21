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
