#pragma once

#include "config.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/eval.h>
#include <libavutil/fifo.h>
#include <libavutil/timestamp.h>
}

#ifdef _WIN32
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avutil.lib")
#endif



#include <string>
#include <vector>

inline std::string cppav_strerror(int errnum) {
	char msg[64] = { 0 };
	av_strerror(errnum, msg, 64);
	return msg;
}
#define cppav_err2str(errnum) cppav_strerror(errnum).data()

inline std::string cppav_ts2str_helper(int64_t t) {
	char msg[32] = { 0 };
	av_ts_make_string(msg, t);
	return std::string(msg);
}
#define cppav_ts2str(t) cppav_ts2str_helper(t).data()

inline std::string cpp_av_ts_make_time_string(int64_t ts, AVRational* tb) {
	char msg[32] = { 0 };
	av_ts_make_time_string(msg, ts, tb);
	return std::string(msg);
}
#define cppav_ts2timestr(ts, tb) cpp_av_ts_make_time_string(ts, tb).data()




#define VSYNC_AUTO       -1
#define VSYNC_PASSTHROUGH 0
#define VSYNC_CFR         1
#define VSYNC_VFR         2
#define VSYNC_VSCFR       0xfe
#define VSYNC_DROP        0xff

#define MAX_STREAMS 1024    /* arbitrary sanity check value */




enum HWAccelID {
    HWACCEL_NONE = 0,
    HWACCEL_AUTO,
    HWACCEL_GENERIC,
    HWACCEL_VIDEOTOOLBOX,
    HWACCEL_QSV,
    HWACCEL_CUVID,
};

struct HWAccel {
    const char* name;
    int (*init)(AVCodecContext* s);
    enum HWAccelID id;
    enum AVPixelFormat pix_fmt;
};

struct HWDevice {
    const char* name;
    enum AVHWDeviceType type;
    AVBufferRef* device_ref;
};


enum forced_keyframes_const {
    FKF_N,
    FKF_N_FORCED,
    FKF_PREV_FORCED_N,
    FKF_PREV_FORCED_T,
    FKF_T,
    FKF_NB
};

#define ABORT_ON_FLAG_EMPTY_OUTPUT (1 <<  0)


typedef enum {
    ENCODER_FINISHED = 1,
    MUXER_FINISHED = 2,
} OSTFinished;

struct InputFilter;
struct InputStream {
    int file_index;
    AVStream* st;
    int discard;             /* true if stream data should be discarded */
    int user_set_discard;
    int decoding_needed;     /* non zero if the packets must be decoded in 'raw_fifo', see DECODING_FOR_* */
#define DECODING_FOR_OST    1
#define DECODING_FOR_FILTER 2

    AVCodecContext* dec_ctx;
    AVCodec* dec;
    AVFrame* decoded_frame;
    AVFrame* filter_frame; /* a ref of decoded_frame, to be sent to filters */

    int64_t       start;     /* time when read started */
    /* predicted dts of the next packet read for this stream or (when there are
     * several frames in a packet) of the next frame in current packet (in AV_TIME_BASE units) */
    int64_t       next_dts;
    int64_t       dts;       ///< dts of the last packet read for this stream (in AV_TIME_BASE units)

    int64_t       next_pts;  ///< synthetic pts for the next decode frame (in AV_TIME_BASE units)
    int64_t       pts;       ///< current pts of the decoded frame  (in AV_TIME_BASE units)
    int           wrap_correction_done;

    int64_t filter_in_rescale_delta_last;

    int64_t min_pts; /* pts with the smallest value in a current stream */
    int64_t max_pts; /* pts with the higher value in a current stream */

    // when forcing constant input framerate through -r,
    // this contains the pts that will be given to the next decoded frame
    int64_t cfr_next_pts;

    int64_t nb_samples; /* number of samples in the last decoded audio frame before looping */

    double ts_scale;
    int saw_first_ts;
    AVDictionary* decoder_opts;
    AVRational framerate;               /* framerate forced with -r */
    int top_field_first;
    int guess_layout_max;

    int autorotate;

    int fix_sub_duration;
    struct { /* previous decoded subtitle and related variables */
        int got_output;
        int ret;
        AVSubtitle subtitle;
    } prev_sub;

    struct sub2video {
        int64_t last_pts;
        int64_t end_pts;
        AVFifoBuffer* sub_queue;    ///< queue of AVSubtitle* before filter init
        AVFrame* frame;
        int w, h;
    } sub2video;

    int dr1;

    /* decoded data from this stream goes into all those filters
     * currently video and audio only */
    InputFilter** filters;
    int        nb_filters;

    int reinit_filters;

    /* hwaccel options */
    HWAccelID hwaccel_id;
    AVHWDeviceType hwaccel_device_type;
    char* hwaccel_device;
    AVPixelFormat hwaccel_output_format;

    /* hwaccel context */
    void* hwaccel_ctx;
    void (*hwaccel_uninit)(AVCodecContext* s);
    int  (*hwaccel_get_buffer)(AVCodecContext* s, AVFrame* frame, int flags);
    int  (*hwaccel_retrieve_data)(AVCodecContext* s, AVFrame* frame);
    AVPixelFormat hwaccel_pix_fmt;
    AVPixelFormat hwaccel_retrieved_pix_fmt;
    AVBufferRef* hw_frames_ctx;

    /* stats */
    // combined size of all the packets read
    uint64_t data_size;
    /* number of packets successfully read for this stream */
    uint64_t nb_packets;
    // number of frames/samples retrieved from the decoder
    uint64_t frames_decoded;
    uint64_t samples_decoded;

    int64_t* dts_buffer;
    int nb_dts_buffer;

    int got_output;
}; // InputStream

struct InputFile {
    AVFormatContext* ctx;
    int eof_reached;      /* true if eof reached */
    int eagain;           /* true if last read attempt returned EAGAIN */
    int ist_index;        /* index of first stream in input_streams */
    int loop;             /* set number of times input stream should be looped */
    int64_t duration;     /* actual duration of the longest stream in a file
                             at the moment when looping happens */
    AVRational time_base; /* time base of the duration */
    int64_t input_ts_offset;

    int64_t ts_offset;
    int64_t last_ts;
    int64_t start_time;   /* user-specified start time in AV_TIME_BASE or AV_NOPTS_VALUE */
    int seek_timestamp;
    int64_t recording_time;
    int nb_streams;       /* number of stream that ffmpeg is aware of; may be different
                             from ctx.nb_streams if new streams appear during av_read_frame() */
    int nb_streams_warn;  /* number of streams that the user was warned of */
    int rate_emu;
    int accurate_seek;

#if HAVE_THREADS
    AVThreadMessageQueue* in_thread_queue;
    pthread_t thread;           /* thread reading from this file */
    int non_blocking;           /* reading packets from the thread should not block */
    int joined;                 /* the thread has been joined */
    int thread_queue_size;      /* maximum number of queued packets */
#endif
}; // InputFile

struct OutputFilter;
struct OutputStream {
    int file_index;          /* file index */
    int index;               /* stream index in the output file */
    int source_index;        /* InputStream index */
    AVStream* st;            /* stream in the output file */
    int encoding_needed;     /* true if encoding needed for this stream */
    int frame_number;
    /* input pts and corresponding output pts
       for A/V sync */
    struct InputStream* sync_ist; /* input stream to sync against */
    int64_t sync_opts;       /* output frame counter, could be changed to some true timestamp */ // FIXME look at frame_number
    /* pts of the first frame encoded for this stream, used for limiting
     * recording time */
    int64_t first_pts;
    /* dts of the last packet sent to the muxer */
    int64_t last_mux_dts;
    // the timebase of the packets sent to the muxer
    AVRational mux_timebase;
    AVRational enc_timebase;

    int nb_bitstream_filters;
    AVBSFContext** bsf_ctx;

    AVCodecContext* enc_ctx;
    AVCodecParameters* ref_par; /* associated input codec parameters with encoders options applied */
    AVCodec* enc;
    int64_t max_frames;
    AVFrame* filtered_frame;
    AVFrame* last_frame;
    int last_dropped;
    int last_nb0_frames[3];

    void* hwaccel_ctx;

    /* video only */
    AVRational frame_rate;
    int is_cfr;
    int force_fps;
    int top_field_first;
    int rotate_overridden;
    double rotate_override_value;

    AVRational frame_aspect_ratio;

    /* forced key frames */
    int64_t forced_kf_ref_pts;
    int64_t* forced_kf_pts;
    int forced_kf_count;
    int forced_kf_index;
    char* forced_keyframes;
    AVExpr* forced_keyframes_pexpr;
    double forced_keyframes_expr_const_values[FKF_NB];

    /* audio only */
    int* audio_channels_map;             /* list of the channels id to pick from the source stream */
    int audio_channels_mapped;           /* number of channels in audio_channels_map */

    char* logfile_prefix;
    FILE* logfile;

    OutputFilter* filter;
    char* avfilter;
    char* filters;         ///< filtergraph associated to the -filter option
    char* filters_script;  ///< filtergraph script associated to the -filter_script option

    AVDictionary* encoder_opts;
    AVDictionary* sws_dict;
    AVDictionary* swr_opts;
    AVDictionary* resample_opts;
    char* apad;
    OSTFinished finished;        /* no more packets should be written for this stream */
    int unavailable;                     /* true if the steram is unavailable (possibly temporarily) */
    int stream_copy;

    // init_output_stream() has been called for this stream
    // The encoder and the bitstream filters have been initialized and the stream
    // parameters are set in the AVStream.
    int initialized;

    int inputs_done;

    const char* attachment_filename;
    int copy_initial_nonkeyframes;
    int copy_prior_start;
    char* disposition;

    int keep_pix_fmt;

    /* stats */
    // combined size of all the packets written
    uint64_t data_size;
    // number of packets send to the muxer
    uint64_t packets_written;
    // number of frames/samples sent to the encoder
    uint64_t frames_encoded;
    uint64_t samples_encoded;

    /* packet quality factor */
    int quality;

    int max_muxing_queue_size;

    /* the packets are buffered here until the muxer is ready to be initialized */
    AVFifoBuffer* muxing_queue;

    /* packet picture type */
    int pict_type;

    /* frame encode sum of squared error values */
    int64_t error[4];
}; // OutputStream

struct OutputFile {
    AVFormatContext* ctx;
    AVDictionary* opts;
    int ost_index;       /* index of the first stream in output_streams */
    int64_t recording_time;  ///< desired length of the resulting file in microseconds == AV_TIME_BASE units
    int64_t start_time;      ///< start time in microseconds == AV_TIME_BASE units
    uint64_t limit_filesize; /* filesize limit expressed in bytes */

    int shortest;

    int header_written;
};

struct FilterGraph;
struct InputFilter {
    AVFilterContext* filter = nullptr;
    InputStream* ist = nullptr;
    FilterGraph* graph = nullptr;
    uint8_t* name = name;
    enum AVMediaType type = AVMEDIA_TYPE_UNKNOWN;   // AVMEDIA_TYPE_SUBTITLE for sub2video

    AVFifoBuffer* frame_queue = nullptr;

    // parameters configured for this input
    int format = 0;

    int width = 0, height = 0;
    AVRational sample_aspect_ratio = AVRational{ 0, 1 };

    int sample_rate = 0;
    int channels = 0;
    uint64_t channel_layout = 0;

    AVBufferRef* hw_frames_ctx = nullptr;

    int eof = 0;
};

struct OutputFilter {
    AVFilterContext* filter = nullptr;
    OutputStream* ost = nullptr;
    FilterGraph* graph = nullptr;
    uint8_t* name = nullptr;

    /* temporary storage until stream maps are processed */
    AVFilterInOut* out_tmp = nullptr;
    AVMediaType type = AVMEDIA_TYPE_UNKNOWN;

    /* desired output stream properties */
    int width = 0, height = 0;
    AVRational frame_rate = AVRational{ 0, 1 };
    int format = 0;
    int sample_rate = 0;
    uint64_t channel_layout = 0;

    // those are only set if no format is specified and the encoder gives us multiple options
    int* formats = nullptr;
    uint64_t* channel_layouts = nullptr;
    int* sample_rates = nullptr;
};

struct FilterGraph {
	int            index = 0;
	const char* graph_desc = nullptr;

	AVFilterGraph* graph = nullptr;
	int reconfiguration;

	InputFilter** inputs = nullptr;
	int          nb_inputs = 0;
    //std::vector<InputFilter*> inputFilters = {};
	OutputFilter** outputs = nullptr;
	int         nb_outputs = 0;
    //std::vector<OutputFilter*> outputFilters = {};
};

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
};


struct FFmpegOption {
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





void term_init(void);
void term_exit(void);








