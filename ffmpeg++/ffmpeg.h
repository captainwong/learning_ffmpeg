#pragma once

#include "structs.h"


static constexpr const char* program_name = "ffmpeg";
static constexpr int program_birth_year = 2000;


struct FFmpegResource {

    InputStream** input_streams = nullptr;
    int        nb_input_streams = 0;
    InputFile** input_files = NULL;
    int        nb_input_files = 0;

    OutputStream** output_streams = nullptr;
    int         nb_output_streams = 0;
    OutputFile** output_files = nullptr;
    int         nb_output_files = 0;

	FilterGraph** filtergraphs = nullptr;
	int        nb_filtergraphs = 0;
    //std::vector<FilterGraph*> filterGraphs = {};

    AVIOContext* progress_avio = nullptr;

};

struct FFmpegConstResource {
    const AVIOInterruptCB int_cb;

    FFmpegConstResource();
};


struct FFmpegOption {
    AVBufferRef* hw_device_ctx = nullptr;
    HWDevice* filter_hw_device = nullptr;

    char* vstats_filename = nullptr;
    char* sdp_filename = nullptr;

    float audio_drift_threshold = 0.1f;
    float dts_delta_threshold = 10;
    float dts_error_threshold = 3600 * 30;

    int audio_volume = 256;
    int audio_sync_method = 0;
    int video_sync_method = VSYNC_AUTO;
    float frame_drop_threshold = 0;
    int do_deinterlace = 0;
    int do_benchmark = 0;
    int do_benchmark_all = 0;
    int do_hex_dump = 0;
    int do_pkt_dump = 0;
    int copy_ts = 0;
    int start_at_zero = 0;
    int copy_tb = -1;
    int debug_ts = 0;
    int exit_on_error = 0;
    int abort_on_flags = 0;
    int print_stats = -1;
    int qp_hist = 0;
    int stdin_interaction = 1;
    int frame_bits_per_raw_sample = 0;
    float max_error_rate = 2.0f / 3;
    int filter_nbthreads = 0;
    int filter_complex_nbthreads = 0;
    int vstats_version = 2;

};

struct CmdutilResource {

    AVDictionary* sws_dict = nullptr;
    AVDictionary* swr_opts = nullptr;
    AVDictionary* format_opts = nullptr, * codec_opts = nullptr, * resample_opts = nullptr;

};


extern FFmpegResource g_ffmpeg_resource;
extern FFmpegOption g_ffmpeg_option;
extern FFmpegConstResource g_ffmpeg_const_resource;
extern CmdutilResource g_cmdutil_resource;

void term_init(void);
void term_exit(void);



int guess_input_channel_layout(InputStream* ist);

void remove_avoptions(AVDictionary** a, AVDictionary* b);
void assert_avoptions(AVDictionary* m);

void sub2video_update(InputStream* ist, AVSubtitle* sub);

int configure_filtergraph(FilterGraph* fg);
int configure_output_filter(FilterGraph* fg, OutputFilter* ofilter, AVFilterInOut* out);
void check_filter_outputs(void);
int ist_in_filtergraph(FilterGraph* fg, InputStream* ist);
int filtergraph_is_simple(FilterGraph* fg);
int init_simple_filtergraph(InputStream* ist, OutputStream* ost);
int init_complex_filtergraph(FilterGraph* fg);

