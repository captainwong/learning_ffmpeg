/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

 /**
  * @file
  * simple media player based on the FFmpeg libraries
  */

#include "cmdutils.h"
#include "ffplay++.h"
#include "packetq.h"
#include "videostate.h"
#include "../sdl.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

//#if HAVE_SYS_RESOURCE_H
//#include <sys/time.h>
//#include <sys/resource.h>
//#endif
#ifdef _WIN32
#include <windows.h>
#endif

#include <assert.h>


FFPlay ffplay{};




static int decode_interrupt_cb(void* ctx)
{
    VideoState* is = (VideoState*)ctx;
    return is->abort_request;
}

static int stream_has_enough_packets(AVStream* st, int stream_id, PacketQueue* queue) {
    return stream_id < 0 ||
        queue->abort_request ||
        (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
        queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

static int is_realtime(AVFormatContext* s)
{
    if (!strcmp(s->iformat->name, "rtp")
        || !strcmp(s->iformat->name, "rtsp")
        || !strcmp(s->iformat->name, "sdp")
        )
        return 1;

    if (s->pb && (!strncmp(s->url, "rtp:", 4)
                  || !strncmp(s->url, "udp:", 4)
                  )
        )
        return 1;
    return 0;
}

/* this thread gets the stream from the disk or the network */
static int read_thread(void* arg)
{
    VideoState* is = (VideoState*)arg;
    AVFormatContext* ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, * pkt = &pkt1;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    AVDictionaryEntry* t;
    SDL_mutex* wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int64_t pkt_ts;

    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
    is->last_subtitle_stream = is->subtitle_stream = -1;
    is->eof = 0;

    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = is;
    if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = 1;
    }
    err = avformat_open_input(&ic, is->filename, is->iformat, &format_opts);
    if (err < 0) {
        print_error(is->filename, err);
        ret = -1;
        goto fail;
    }
    if (scan_all_pmts_set)
        av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }
    is->ic = ic;

    if (ffplay.genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    av_format_inject_global_side_data(ic);

    if (ffplay.find_stream_info) {
        AVDictionary** opts = setup_find_stream_info_opts(ic, codec_opts);
        int orig_nb_streams = ic->nb_streams;

        err = avformat_find_stream_info(ic, opts);

        for (i = 0; i < orig_nb_streams; i++)
            av_dict_free(&opts[i]);
        av_freep(&opts);

        if (err < 0) {
            av_log(NULL, AV_LOG_WARNING,
                   "%s: could not find codec parameters\n", is->filename);
            ret = -1;
            goto fail;
        }
    }

    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

    if (ffplay.seek_by_bytes < 0)
        ffplay.seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", ic->iformat->name);

    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    if (!ffplay.window_title && (t = av_dict_get(ic->metadata, "title", NULL, 0)))
        ffplay.window_title = av_asprintf("%s - %s", t->value, ffplay.input_filename);

    /* if seeking requested, we execute it */
    if (ffplay.start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = ffplay.start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
                   is->filename, (double)timestamp / AV_TIME_BASE);
        }
    }

    is->realtime = is_realtime(ic);

    if (ffplay.show_status)
        av_dump_format(ic, 0, is->filename, 0);

    for (i = 0; i < ic->nb_streams; i++) {
        AVStream* st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
        if (type >= 0 && ffplay.wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, ffplay.wanted_stream_spec[type]) > 0)
                st_index[type] = i;
    }
    for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
        if (ffplay.wanted_stream_spec[i] && st_index[i] == -1) {
            av_log(NULL, AV_LOG_ERROR, "Stream specifier %s does not match any %s stream\n", ffplay.wanted_stream_spec[i], av_get_media_type_string((AVMediaType)i));
            st_index[i] = INT_MAX;
        }
    }

    if (!ffplay.video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
        av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                            st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (!ffplay.audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
        av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                            st_index[AVMEDIA_TYPE_AUDIO],
                            st_index[AVMEDIA_TYPE_VIDEO],
                            NULL, 0);
    if (!ffplay.video_disable && !ffplay.subtitle_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] =
        av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                            st_index[AVMEDIA_TYPE_SUBTITLE],
                            (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                             st_index[AVMEDIA_TYPE_AUDIO] :
                             st_index[AVMEDIA_TYPE_VIDEO]),
                            NULL, 0);

    is->show_mode = ffplay.show_mode;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream* st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters* codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
        if (codecpar->width)
            set_default_window_size(codecpar->width, codecpar->height, sar);
    }

    /* open the streams */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        is->stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = is->stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (is->show_mode == SHOW_MODE_NONE)
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        is->stream_component_open(st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
               is->filename);
        ret = -1;
        goto fail;
    }

    if (ffplay.infinite_buffer < 0 && is->realtime)
        ffplay.infinite_buffer = 1;

    for (;;) {
        if (is->abort_request)
            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused &&
            (!strcmp(ic->iformat->name, "rtsp") ||
            (ic->pb && !strncmp(ffplay.input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            continue;
        }
#endif
        if (is->seek_req) {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
            int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;
            // FIXME the +-2 is due to rounding being not done in the correct direction in generation
            //      of the seek_pos/seek_rel variables

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "%s: error while seeking\n", is->ic->url);
            } else {
                if (is->audio_stream >= 0) {
                    is->audioq.flush();
                    is->audioq.put(&ffplay.flush_pkt);
                }
                if (is->subtitle_stream >= 0) {
                    is->subtitleq.flush();
                    is->subtitleq.put(&ffplay.flush_pkt);
                }
                if (is->video_stream >= 0) {
                    is->videoq.flush();
                    is->videoq.put(&ffplay.flush_pkt);
                }
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                    is->extclk.set_clock(NAN, 0);
                } else {
                    is->extclk.set_clock(seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            is->seek_req = 0;
            is->queue_attachments_req = 1;
            is->eof = 0;
            if (is->paused)
                is->step_to_next_frame();
        }
        if (is->queue_attachments_req) {
            if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                AVPacket copy = { 0 };
                if ((ret = av_packet_ref(&copy, &is->video_st->attached_pic)) < 0)
                    goto fail;
                is->videoq.put(&copy);
                is->videoq.put_nullpacket(is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (ffplay.infinite_buffer < 1 &&
            (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
             || (stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq) &&
                 stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq) &&
                 stream_has_enough_packets(is->subtitle_st, is->subtitle_stream, &is->subtitleq)))) {
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if (!is->paused &&
            (!is->audio_st || (is->auddec.finished == is->audioq.serial && is->sampq.nb_remaining() == 0)) &&
            (!is->video_st || (is->viddec.finished == is->videoq.serial && is->pictq.nb_remaining() == 0))) {
            if (ffplay.loop != 1 && (!ffplay.loop || --ffplay.loop)) {
                is->stream_seek(ffplay.start_time != AV_NOPTS_VALUE ? ffplay.start_time : 0, 0, 0);
            } else if (ffplay.autoexit) {
                ret = AVERROR_EOF;
                goto fail;
            }
        }
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                if (is->video_stream >= 0)
                    is->videoq.put_nullpacket(is->video_stream);
                if (is->audio_stream >= 0)
                    is->audioq.put_nullpacket(is->audio_stream);
                if (is->subtitle_stream >= 0)
                    is->subtitleq.put_nullpacket(is->subtitle_stream);
                is->eof = 1;
            }
            if (ic->pb && ic->pb->error)
                break;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        } else {
            is->eof = 0;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = ffplay.duration == AV_NOPTS_VALUE ||
            (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
            av_q2d(ic->streams[pkt->stream_index]->time_base) -
            (double)(ffplay.start_time != AV_NOPTS_VALUE ? ffplay.start_time : 0) / 1000000
            <= ((double)ffplay.duration / 1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            is->audioq.put(pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                   && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            is->videoq.put(pkt);
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            is->subtitleq.put(pkt);
        } else {
            av_packet_unref(pkt);
        }
    }

    ret = 0;
fail:
    if (ic && !is->ic)
        avformat_close_input(&ic);

    if (ret != 0) {
        SDL_Event event;

        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return 0;
}

static VideoState* stream_open(const char* filename, AVInputFormat* iformat)
{
    VideoState* is;

    is = (VideoState*)av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    is->filename = av_strdup(filename);
    if (!is->filename)
        goto fail;
    is->iformat = iformat;
    is->ytop = 0;
    is->xleft = 0;

    /* start video display */
    if (is->pictq.init(&is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
        goto fail;
    if (is->subpq.init(&is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
        goto fail;
    if (is->sampq.init(&is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;

    if (is->videoq.init() < 0 ||
        is->audioq.init() < 0 ||
        is->subtitleq.init() < 0)
        goto fail;

    if (!(is->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        goto fail;
    }

    is->vidclk.init_clock(&is->videoq.serial);
    is->audclk.init_clock(&is->audioq.serial);
    is->extclk.init_clock(&is->extclk.serial);
    is->audio_clock_serial = -1;
    if (ffplay.startup_volume < 0)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", ffplay.startup_volume);
    if (ffplay.startup_volume > 100)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", ffplay.startup_volume);
    ffplay.startup_volume = av_clip(ffplay.startup_volume, 0, 100);
    ffplay.startup_volume = av_clip(SDL_MIX_MAXVOLUME * ffplay.startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
    is->audio_volume = ffplay.startup_volume;
    is->muted = 0;
    is->av_sync_type = ffplay.av_sync_type;
    is->read_tid = SDL_CreateThread(read_thread, "read_thread", is);
    if (!is->read_tid) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
fail:
        is->stream_close();
        av_free(is);
        return NULL;
    }
    return is;
}


#if CONFIG_AVFILTER
static int opt_add_vfilter(void* optctx, const char* opt, const char* arg)
{
    //GROW_ARRAY(vfilters_list, nb_vfilters);
    ffplay.vfilters_list = (const char**)grow_array(ffplay.vfilters_list, sizeof(*ffplay.vfilters_list), &ffplay.nb_vfilters, ffplay.nb_vfilters + 1);
    ffplay.vfilters_list[ffplay.nb_vfilters - 1] = arg;
    return 0;
}
#endif

static int opt_frame_size(void* optctx, const char* opt, const char* arg)
{
    av_log(NULL, AV_LOG_WARNING, "Option -s is deprecated, use -video_size.\n");
    return opt_default(NULL, "video_size", arg);
}

static int opt_width(void* optctx, const char* opt, const char* arg)
{
    ffplay.screen_width = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

static int opt_height(void* optctx, const char* opt, const char* arg)
{
    ffplay.screen_height = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

static int opt_format(void* optctx, const char* opt, const char* arg)
{
    ffplay.file_iformat = av_find_input_format(arg);
    if (!ffplay.file_iformat) {
        av_log(NULL, AV_LOG_FATAL, "Unknown input format: %s\n", arg);
        return AVERROR(EINVAL);
    }
    return 0;
}

static int opt_frame_pix_fmt(void* optctx, const char* opt, const char* arg)
{
    av_log(NULL, AV_LOG_WARNING, "Option -pix_fmt is deprecated, use -pixel_format.\n");
    return opt_default(NULL, "pixel_format", arg);
}

static int opt_sync(void* optctx, const char* opt, const char* arg)
{
    if (!strcmp(arg, "audio"))
        ffplay.av_sync_type = AV_SYNC_AUDIO_MASTER;
    else if (!strcmp(arg, "video"))
        ffplay.av_sync_type = AV_SYNC_VIDEO_MASTER;
    else if (!strcmp(arg, "ext"))
        ffplay.av_sync_type = AV_SYNC_EXTERNAL_CLOCK;
    else {
        av_log(NULL, AV_LOG_ERROR, "Unknown value for %s: %s\n", opt, arg);
        exit(1);
    }
    return 0;
}

static int opt_seek(void* optctx, const char* opt, const char* arg)
{
    ffplay.start_time = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_duration(void* optctx, const char* opt, const char* arg)
{
    ffplay.duration = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_show_mode(void* optctx, const char* opt, const char* arg)
{
    ffplay.show_mode = !strcmp(arg, "video") ? SHOW_MODE_VIDEO :
        !strcmp(arg, "waves") ? SHOW_MODE_WAVES :
        !strcmp(arg, "rdft") ? SHOW_MODE_RDFT :
        (ShowMode)(int)parse_number_or_die(opt, arg, OPT_INT, 0, SHOW_MODE_NB - 1);
    return 0;
}

static void opt_input_file(void* optctx, const char* filename)
{
    if (ffplay.input_filename) {
        av_log(NULL, AV_LOG_FATAL,
               "Argument '%s' provided as input filename, but '%s' was already specified.\n",
               filename, ffplay.input_filename);
        exit(1);
    }
    if (!strcmp(filename, "-"))
        filename = "pipe:";
    ffplay.input_filename = filename;
}

static int opt_codec(void* optctx, const char* opt, const char* arg)
{
    const char* spec = strchr(opt, ':');
    if (!spec) {
        av_log(NULL, AV_LOG_ERROR,
               "No media specifier was specified in '%s' in option '%s'\n",
               arg, opt);
        return AVERROR(EINVAL);
    }
    spec++;
    switch (spec[0]) {
    case 'a':    ffplay.audio_codec_name = arg; break;
    case 's': ffplay.subtitle_codec_name = arg; break;
    case 'v':    ffplay.video_codec_name = arg; break;
    default:
        av_log(NULL, AV_LOG_ERROR,
               "Invalid media specifier '%s' in option '%s'\n", spec, opt);
        return AVERROR(EINVAL);
    }
    return 0;
}

static int dummy;

static const OptionDef options[] = {
    CMDUTILS_COMMON_OPTIONS
    { "x", HAS_ARG, opt_width, "force displayed width", "width" },
    { "y", HAS_ARG, opt_height, "force displayed height", "height" },
    { "s", HAS_ARG | OPT_VIDEO, opt_frame_size, "set frame size (WxH or abbreviation)", "size" },
    { "fs", OPT_BOOL, { &ffplay.is_full_screen }, "force full screen" },
    { "an", OPT_BOOL, { &ffplay.audio_disable }, "disable audio" },
    { "vn", OPT_BOOL, { &ffplay.video_disable }, "disable video" },
    { "sn", OPT_BOOL, { &ffplay.subtitle_disable }, "disable subtitling" },
    { "ast", OPT_STRING | HAS_ARG | OPT_EXPERT, { &ffplay.wanted_stream_spec[AVMEDIA_TYPE_AUDIO] }, "select desired audio stream", "stream_specifier" },
    { "vst", OPT_STRING | HAS_ARG | OPT_EXPERT, { &ffplay.wanted_stream_spec[AVMEDIA_TYPE_VIDEO] }, "select desired video stream", "stream_specifier" },
    { "sst", OPT_STRING | HAS_ARG | OPT_EXPERT, { &ffplay.wanted_stream_spec[AVMEDIA_TYPE_SUBTITLE] }, "select desired subtitle stream", "stream_specifier" },
    { "ss", HAS_ARG, opt_seek, "seek to a given position in seconds", "pos" },
    { "t", HAS_ARG, opt_duration, "play  \"duration\" seconds of audio/video", "duration" },
    { "bytes", OPT_INT | HAS_ARG, { &ffplay.seek_by_bytes }, "seek by bytes 0=off 1=on -1=auto", "val" },
    { "seek_interval", OPT_FLOAT | HAS_ARG, { &ffplay.seek_interval }, "set seek interval for left/right keys, in seconds", "seconds" },
    { "nodisp", OPT_BOOL, { &ffplay.display_disable }, "disable graphical display" },
    { "noborder", OPT_BOOL, { &ffplay.borderless }, "borderless window" },
    { "alwaysontop", OPT_BOOL, { &ffplay.alwaysontop }, "window always on top" },
    { "volume", OPT_INT | HAS_ARG, { &ffplay.startup_volume}, "set startup volume 0=min 100=max", "volume" },
    { "f", HAS_ARG, opt_format, "force format", "fmt" },
    { "pix_fmt", HAS_ARG | OPT_EXPERT | OPT_VIDEO, opt_frame_pix_fmt, "set pixel format", "format" },
    { "stats", OPT_BOOL | OPT_EXPERT, { &ffplay.show_status }, "show status", "" },
    { "fast", OPT_BOOL | OPT_EXPERT, { &ffplay.fast }, "non spec compliant optimizations", "" },
    { "genpts", OPT_BOOL | OPT_EXPERT, { &ffplay.genpts }, "generate pts", "" },
    { "drp", OPT_INT | HAS_ARG | OPT_EXPERT, { &ffplay.decoder_reorder_pts }, "let decoder reorder pts 0=off 1=on -1=auto", ""},
    { "lowres", OPT_INT | HAS_ARG | OPT_EXPERT, { &ffplay.lowres }, "", "" },
    { "sync", HAS_ARG | OPT_EXPERT, opt_sync, "set audio-video sync. type (type=audio/video/ext)", "type" },
    { "autoexit", OPT_BOOL | OPT_EXPERT, { &ffplay.autoexit }, "exit at the end", "" },
    { "exitonkeydown", OPT_BOOL | OPT_EXPERT, { &ffplay.exit_on_keydown }, "exit on key down", "" },
    { "exitonmousedown", OPT_BOOL | OPT_EXPERT, { &ffplay.exit_on_mousedown }, "exit on mouse down", "" },
    { "loop", OPT_INT | HAS_ARG | OPT_EXPERT, { &ffplay.loop }, "set number of times the playback shall be looped", "loop count" },
    { "framedrop", OPT_BOOL | OPT_EXPERT, { &ffplay.framedrop }, "drop frames when cpu is too slow", "" },
    { "infbuf", OPT_BOOL | OPT_EXPERT, { &ffplay.infinite_buffer }, "don't limit the input buffer size (useful with realtime streams)", "" },
    { "window_title", OPT_STRING | HAS_ARG, { &ffplay.window_title }, "set window title", "window title" },
    { "left", OPT_INT | HAS_ARG | OPT_EXPERT, { &ffplay.screen_left }, "set the x position for the left of the window", "x pos" },
    { "top", OPT_INT | HAS_ARG | OPT_EXPERT, { &ffplay.screen_top }, "set the y position for the top of the window", "y pos" },
#if CONFIG_AVFILTER
    { "vf", OPT_EXPERT | HAS_ARG, opt_add_vfilter, "set video filters", "filter_graph" },
    { "af", OPT_STRING | HAS_ARG, { &ffplay.afilters }, "set audio filters", "filter_graph" },
#endif
    { "rdftspeed", OPT_INT | HAS_ARG | OPT_AUDIO | OPT_EXPERT, { &ffplay.rdftspeed }, "rdft speed", "msecs" },
    { "showmode", HAS_ARG, opt_show_mode, "select show mode (0 = video, 1 = waves, 2 = RDFT)", "mode" },
    { "default", HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT, opt_default, "generic catch all option", "" },
    { "i", OPT_BOOL, { &dummy}, "read specified file", "input_file"},
    { "codec", HAS_ARG, opt_codec, "force decoder", "decoder_name" },
    { "acodec", HAS_ARG | OPT_STRING | OPT_EXPERT, {    &ffplay.audio_codec_name }, "force audio decoder",    "decoder_name" },
    { "scodec", HAS_ARG | OPT_STRING | OPT_EXPERT, { &ffplay.subtitle_codec_name }, "force subtitle decoder", "decoder_name" },
    { "vcodec", HAS_ARG | OPT_STRING | OPT_EXPERT, {    &ffplay.video_codec_name }, "force video decoder",    "decoder_name" },
    { "autorotate", OPT_BOOL, { &ffplay.autorotate }, "automatically rotate video", "" },
    { "find_stream_info", OPT_BOOL | OPT_INPUT | OPT_EXPERT, { &ffplay.find_stream_info },
        "read and decode the streams to fill missing information with heuristics" },
    { "filter_threads", HAS_ARG | OPT_INT | OPT_EXPERT, { &ffplay.filter_nbthreads }, "number of filter threads per graph" },
    { NULL, },
};

static void show_usage(void)
{
    av_log(NULL, AV_LOG_INFO, "Simple media player\n");
    av_log(NULL, AV_LOG_INFO, "usage: %s [options] input_file\n", program_name);
    av_log(NULL, AV_LOG_INFO, "\n");
}

void show_help_default(const char* opt, const char* arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:", 0, OPT_EXPERT, 0);
    show_help_options(options, "Advanced options:", OPT_EXPERT, 0, 0);
    printf("\n");
    show_help_children(avcodec_get_class(), AV_OPT_FLAG_DECODING_PARAM);
    show_help_children(avformat_get_class(), AV_OPT_FLAG_DECODING_PARAM);
#if !CONFIG_AVFILTER
    show_help_children(sws_get_class(), AV_OPT_FLAG_ENCODING_PARAM);
#else
    show_help_children(avfilter_get_class(), AV_OPT_FLAG_FILTERING_PARAM);
#endif
    printf("\nWhile playing:\n"
           "q, ESC              quit\n"
           "f                   toggle full screen\n"
           "p, SPC              pause\n"
           "m                   toggle mute\n"
           "9, 0                decrease and increase volume respectively\n"
           "/, *                decrease and increase volume respectively\n"
           "a                   cycle audio channel in the current program\n"
           "v                   cycle video channel\n"
           "t                   cycle subtitle channel in the current program\n"
           "c                   cycle program\n"
           "w                   cycle video filters or show modes\n"
           "s                   activate frame-step mode\n"
           "left/right          seek backward/forward 10 seconds or to custom interval if -seek_interval is set\n"
           "down/up             seek backward/forward 1 minute\n"
           "page down/page up   seek backward/forward 10 minutes\n"
           "right mouse click   seek to percentage in file corresponding to fraction of width\n"
           "left double-click   toggle full screen\n"
    );
}


static void sigterm_handler(int sig)
{
    exit(123);
}

/* Called from the main */
int main(int argc, char** argv)
{
    int flags;
    VideoState* is;

    init_dynload();

    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);

    /* register all codecs, demux and protocols */
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
    avformat_network_init();

    init_opts();

    signal(SIGINT, sigterm_handler); /* Interrupt (ANSI).    */
    signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */

    show_banner(argc, argv, options);

    parse_options(NULL, argc, argv, options, opt_input_file);

    if (!ffplay.input_filename) {
        show_usage();
        av_log(NULL, AV_LOG_FATAL, "An input file must be specified\n");
        av_log(NULL, AV_LOG_FATAL,
               "Use -h to get full help or, even better, run 'man %s'\n", program_name);
        exit(1);
    }

    if (ffplay.display_disable) {
        ffplay.video_disable = 1;
    }
    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (ffplay.audio_disable)
        flags &= ~SDL_INIT_AUDIO;
    else {
        /* Try to work around an occasional ALSA buffer underflow issue when the
         * period size is NPOT due to ALSA resampling by forcing the buffer size. */
        if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
            SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE", "1", 1);
    }
    if (ffplay.display_disable)
        flags &= ~SDL_INIT_VIDEO;
    if (SDL_Init(flags)) {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
        av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        exit(1);
    }

    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    av_init_packet(&ffplay.flush_pkt);
    ffplay.flush_pkt.data = (uint8_t*)&ffplay.flush_pkt;

    if (!ffplay.display_disable) {
        int flags = SDL_WINDOW_HIDDEN;
        if (ffplay.alwaysontop)
#if SDL_VERSION_ATLEAST(2,0,5)
            flags |= SDL_WINDOW_ALWAYS_ON_TOP;
#else
            av_log(NULL, AV_LOG_WARNING, "Your SDL version doesn't support SDL_WINDOW_ALWAYS_ON_TOP. Feature will be inactive.\n");
#endif
        if (ffplay.borderless)
            flags |= SDL_WINDOW_BORDERLESS;
        else
            flags |= SDL_WINDOW_RESIZABLE;
        ffplay.window = SDL_CreateWindow(program_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ffplay.default_width, ffplay.default_height, flags);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        if (ffplay.window) {
            ffplay.renderer = SDL_CreateRenderer(ffplay.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (!ffplay.renderer) {
                av_log(NULL, AV_LOG_WARNING, "Failed to initialize a hardware accelerated renderer: %s\n", SDL_GetError());
                ffplay.renderer = SDL_CreateRenderer(ffplay.window, -1, 0);
            }
            if (ffplay.renderer) {
                if (!SDL_GetRendererInfo(ffplay.renderer, &ffplay.renderer_info))
                    av_log(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.\n", ffplay.renderer_info.name);
            }
        }
        if (!ffplay.window || !ffplay.renderer || !ffplay.renderer_info.num_texture_formats) {
            av_log(NULL, AV_LOG_FATAL, "Failed to create window or renderer: %s", SDL_GetError());
            VideoState::do_exit(NULL);
        }
    }

    is = stream_open(ffplay.input_filename, ffplay.file_iformat);
    if (!is) {
        av_log(NULL, AV_LOG_FATAL, "Failed to initialize VideoState!\n");
        VideoState::do_exit(NULL);
    }

    is->event_loop();

    /* never returns */

    return 0;
}
