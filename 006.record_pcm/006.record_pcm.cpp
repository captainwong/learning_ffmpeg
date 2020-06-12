/**
2020年6月12日
Jack
通过各系统的音频输入设备采集音频并写入pcm文件

*/


#include <stdio.h>
#include <thread>
#include <string>
#ifdef _WIN32
#include <jlib/win32/UnicodeTool.h>
#endif
#include <stdint.h>
#include <assert.h>

#pragma warning(disable:4819)
#pragma warning(disable:4996)

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#ifdef _WIN32
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#endif

const char* exe = "";


void usage()
{
	printf("Usage:%s indevice audio_device output.pcm\n"
		   "indevice     The input device, for Windows it's usually dshow or vfwcap, for Mac its avfoundation, Linux is alsa\n"
		   "audio_device The dshow audio device name like \"Microphone (3- USB2.0 MIC)\", i.e.\n"
		   "               on Windows, you can run `ffmpeg -f dshow --list-devices true -i \"dummy\"` to find out which device to use.\n"
		   "               on MacOS, its `ffmpeg -f avfoundation --list-devices true -i \"dummy\"`\n"
		   "               on Linux, its `cat /proc/asound/cards` or `cat /proc/asound/devices`\n"
		   "output.pcm   The recorded audio file\n"
		   "Example:\n"
		   "Windows:     %s dshow \"audio=麦克风(Realtek High Definition Audio)\" output.pcm\n"
		   "MacOS:       %s avfoundation :0 output.pcm\n"
		   "Linux:       %s alsa hw:0 output.pcm\n"
		   , exe, exe, exe, exe);
}


int get_format_from_input_fmt(const char** fmt,
							  AVFormatContext* ic)
{
	AVCodecContext* avctx = avcodec_alloc_context3(NULL);
	if (!avctx)
		return -1;
	int ret = avcodec_parameters_to_context(avctx, ic->streams[0]->codecpar);
	if (ret < 0) {
		avcodec_free_context(&avctx);
		return ret;
	}

	*fmt = av_get_sample_fmt_name(avctx->sample_fmt);
	avcodec_free_context(&avctx);
	return 0;
}


void myavcodec_string(char* buf, int buf_size, AVCodecContext* enc, int encode)
{
	const char* codec_type;
	const char* codec_name;
	const char* profile = NULL;
	int64_t bitrate;
	int new_line = 0;
	AVRational display_aspect_ratio;
	const char* separator = enc->dump_separator ? (const char*)enc->dump_separator : ", ";

	if (!buf || buf_size <= 0)
		return;
	codec_type = av_get_media_type_string(enc->codec_type);
	codec_name = avcodec_get_name(enc->codec_id);
	profile = avcodec_profile_name(enc->codec_id, enc->profile);

	snprintf(buf, buf_size, "%s: %s", codec_type ? codec_type : "unknown",
			 codec_name);
	buf[0] ^= 'a' ^ 'A'; /* first letter in uppercase */

	if (enc->codec && strcmp(enc->codec->name, codec_name))
		snprintf(buf + strlen(buf), buf_size - strlen(buf), " (%s)", enc->codec->name);

	if (profile)
		snprintf(buf + strlen(buf), buf_size - strlen(buf), " (%s)", profile);
	if (enc->codec_type == AVMEDIA_TYPE_VIDEO
		&& av_log_get_level() >= AV_LOG_VERBOSE
		&& enc->refs)
		snprintf(buf + strlen(buf), buf_size - strlen(buf),
				 ", %d reference frame%s",
				 enc->refs, enc->refs > 1 ? "s" : "");

	//if (enc->codec_tag)
		//snprintf(buf + strlen(buf), buf_size - strlen(buf), " (%s / 0x%04X)",
		//		 av_fourcc2str(enc->codec_tag), enc->codec_tag);

	switch (enc->codec_type) {
	/*case AVMEDIA_TYPE_VIDEO:
	{
		char detail[256] = "(";

		av_strlcat(buf, separator, buf_size);

		snprintf(buf + strlen(buf), buf_size - strlen(buf),
				 "%s", enc->pix_fmt == AV_PIX_FMT_NONE ? "none" :
				 av_get_pix_fmt_name(enc->pix_fmt));
		if (enc->bits_per_raw_sample && enc->pix_fmt != AV_PIX_FMT_NONE &&
			enc->bits_per_raw_sample < av_pix_fmt_desc_get(enc->pix_fmt)->comp[0].depth)
			av_strlcatf(detail, sizeof(detail), "%d bpc, ", enc->bits_per_raw_sample);
		if (enc->color_range != AVCOL_RANGE_UNSPECIFIED)
			av_strlcatf(detail, sizeof(detail), "%s, ",
						av_color_range_name(enc->color_range));

		if (enc->colorspace != AVCOL_SPC_UNSPECIFIED ||
			enc->color_primaries != AVCOL_PRI_UNSPECIFIED ||
			enc->color_trc != AVCOL_TRC_UNSPECIFIED) {
			if (enc->colorspace != (int)enc->color_primaries ||
				enc->colorspace != (int)enc->color_trc) {
				new_line = 1;
				av_strlcatf(detail, sizeof(detail), "%s/%s/%s, ",
							av_color_space_name(enc->colorspace),
							av_color_primaries_name(enc->color_primaries),
							av_color_transfer_name(enc->color_trc));
			} else
				av_strlcatf(detail, sizeof(detail), "%s, ",
							av_get_colorspace_name(enc->colorspace));
		}

		if (enc->field_order != AV_FIELD_UNKNOWN) {
			const char* field_order = "progressive";
			if (enc->field_order == AV_FIELD_TT)
				field_order = "top first";
			else if (enc->field_order == AV_FIELD_BB)
				field_order = "bottom first";
			else if (enc->field_order == AV_FIELD_TB)
				field_order = "top coded first (swapped)";
			else if (enc->field_order == AV_FIELD_BT)
				field_order = "bottom coded first (swapped)";

			av_strlcatf(detail, sizeof(detail), "%s, ", field_order);
		}

		if (av_log_get_level() >= AV_LOG_VERBOSE &&
			enc->chroma_sample_location != AVCHROMA_LOC_UNSPECIFIED)
			av_strlcatf(detail, sizeof(detail), "%s, ",
						av_chroma_location_name(enc->chroma_sample_location));

		if (strlen(detail) > 1) {
			detail[strlen(detail) - 2] = 0;
			av_strlcatf(buf, buf_size, "%s)", detail);
		}
	}

	if (enc->width) {
		av_strlcat(buf, new_line ? separator : ", ", buf_size);

		snprintf(buf + strlen(buf), buf_size - strlen(buf),
				 "%dx%d",
				 enc->width, enc->height);

		if (av_log_get_level() >= AV_LOG_VERBOSE &&
			(enc->width != enc->coded_width ||
			 enc->height != enc->coded_height))
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 " (%dx%d)", enc->coded_width, enc->coded_height);

		if (enc->sample_aspect_ratio.num) {
			av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
					  enc->width * (int64_t)enc->sample_aspect_ratio.num,
					  enc->height * (int64_t)enc->sample_aspect_ratio.den,
					  1024 * 1024);
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 " [SAR %d:%d DAR %d:%d]",
					 enc->sample_aspect_ratio.num, enc->sample_aspect_ratio.den,
					 display_aspect_ratio.num, display_aspect_ratio.den);
		}
		if (av_log_get_level() >= AV_LOG_DEBUG) {
			int g = av_gcd(enc->time_base.num, enc->time_base.den);
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 ", %d/%d",
					 enc->time_base.num / g, enc->time_base.den / g);
		}
	}
	if (encode) {
		snprintf(buf + strlen(buf), buf_size - strlen(buf),
				 ", q=%d-%d", enc->qmin, enc->qmax);
	} else {
		if (enc->properties & FF_CODEC_PROPERTY_CLOSED_CAPTIONS)
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 ", Closed Captions");
		if (enc->properties & FF_CODEC_PROPERTY_LOSSLESS)
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 ", lossless");
	}*/
	break;
	case AVMEDIA_TYPE_AUDIO:
		//av_strlcat(buf, separator, buf_size);

		if (enc->sample_rate) {
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 "%d Hz, ", enc->sample_rate);
		}
		av_get_channel_layout_string(buf + strlen(buf), buf_size - strlen(buf), enc->channels, enc->channel_layout);
		if (enc->sample_fmt != AV_SAMPLE_FMT_NONE) {
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 ", %s", av_get_sample_fmt_name(enc->sample_fmt));
		}
		if (enc->bits_per_raw_sample > 0
			&& enc->bits_per_raw_sample != av_get_bytes_per_sample(enc->sample_fmt) * 8)
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 " (%d bit)", enc->bits_per_raw_sample);
		if (av_log_get_level() >= AV_LOG_VERBOSE) {
			if (enc->initial_padding)
				snprintf(buf + strlen(buf), buf_size - strlen(buf),
						 ", delay %d", enc->initial_padding);
			if (enc->trailing_padding)
				snprintf(buf + strlen(buf), buf_size - strlen(buf),
						 ", padding %d", enc->trailing_padding);
		}
		break;
	case AVMEDIA_TYPE_DATA:
		if (av_log_get_level() >= AV_LOG_DEBUG) {
			int g = av_gcd(enc->time_base.num, enc->time_base.den);
			if (g)
				snprintf(buf + strlen(buf), buf_size - strlen(buf),
						 ", %d/%d",
						 enc->time_base.num / g, enc->time_base.den / g);
		}
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		if (enc->width)
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 ", %dx%d", enc->width, enc->height);
		break;
	default:
		return;
	}
	if (encode) {
		if (enc->flags & AV_CODEC_FLAG_PASS1)
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 ", pass 1");
		if (enc->flags & AV_CODEC_FLAG_PASS2)
			snprintf(buf + strlen(buf), buf_size - strlen(buf),
					 ", pass 2");
	}
	/*bitrate = get_bit_rate(enc);
	if (bitrate != 0) {
		snprintf(buf + strlen(buf), buf_size - strlen(buf),
				 ", %"PRId64" kb/s", bitrate / 1000);
	} else if (enc->rc_max_rate > 0) {
		snprintf(buf + strlen(buf), buf_size - strlen(buf),
				 ", max. %"PRId64" kb/s", enc->rc_max_rate / 1000);
	}*/
}

static void dump_stream_format(AVFormatContext* ic, int i,
							   int index, int is_output)
{
	char buf[256];
	int flags = (is_output ? ic->oformat->flags : ic->iformat->flags);
	AVStream* st = ic->streams[i];
	AVDictionaryEntry* lang = av_dict_get(st->metadata, "language", NULL, 0);
	char* separator = (char*)ic->dump_separator;
	AVCodecContext* avctx;
	int ret;

	avctx = avcodec_alloc_context3(NULL);
	if (!avctx)
		return;

	ret = avcodec_parameters_to_context(avctx, st->codecpar);
	if (ret < 0) {
		avcodec_free_context(&avctx);
		return;
	}

	// Fields which are missing from AVCodecParameters need to be taken from the AVCodecContext
	avctx->properties = st->codec->properties;
	avctx->codec = st->codec->codec;
	avctx->qmin = st->codec->qmin;
	avctx->qmax = st->codec->qmax;
	avctx->coded_width = st->codec->coded_width;
	avctx->coded_height = st->codec->coded_height;

	if (separator)
		av_opt_set(avctx, "dump_separator", separator, 0);
	myavcodec_string(buf, sizeof(buf), avctx, is_output);
	avcodec_free_context(&avctx);

	av_log(NULL, AV_LOG_INFO, "    Stream #%d:%d", index, i);

	/* the pid is an important information, so we display it */
	/* XXX: add a generic system */
	if (flags & AVFMT_SHOW_IDS)
		av_log(NULL, AV_LOG_INFO, "[0x%x]", st->id);
	if (lang)
		av_log(NULL, AV_LOG_INFO, "(%s)", lang->value);
	av_log(NULL, AV_LOG_DEBUG, ", %d, %d/%d", st->codec_info_nb_frames,
		   st->time_base.num, st->time_base.den);
	av_log(NULL, AV_LOG_INFO, ": %s", buf);

	if (st->sample_aspect_ratio.num &&
		av_cmp_q(st->sample_aspect_ratio, st->codecpar->sample_aspect_ratio)) {
		AVRational display_aspect_ratio;
		av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
				  st->codecpar->width * (int64_t)st->sample_aspect_ratio.num,
				  st->codecpar->height * (int64_t)st->sample_aspect_ratio.den,
				  1024 * 1024);
		av_log(NULL, AV_LOG_INFO, ", SAR %d:%d DAR %d:%d",
			   st->sample_aspect_ratio.num, st->sample_aspect_ratio.den,
			   display_aspect_ratio.num, display_aspect_ratio.den);
	}

	if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
		int fps = st->avg_frame_rate.den && st->avg_frame_rate.num;
		int tbr = st->r_frame_rate.den && st->r_frame_rate.num;
		int tbn = st->time_base.den && st->time_base.num;
		int tbc = st->codec->time_base.den && st->codec->time_base.num;

		if (fps || tbr || tbn || tbc)
			av_log(NULL, AV_LOG_INFO, "%s", separator);

		/*if (fps)
			print_fps(av_q2d(st->avg_frame_rate), tbr || tbn || tbc ? "fps, " : "fps");
		if (tbr)
			print_fps(av_q2d(st->r_frame_rate), tbn || tbc ? "tbr, " : "tbr");
		if (tbn)
			print_fps(1 / av_q2d(st->time_base), tbc ? "tbn, " : "tbn");
		if (tbc)
			print_fps(1 / av_q2d(st->codec->time_base), "tbc");*/
	}

	if (st->disposition & AV_DISPOSITION_DEFAULT)
		av_log(NULL, AV_LOG_INFO, " (default)");
	if (st->disposition & AV_DISPOSITION_DUB)
		av_log(NULL, AV_LOG_INFO, " (dub)");
	if (st->disposition & AV_DISPOSITION_ORIGINAL)
		av_log(NULL, AV_LOG_INFO, " (original)");
	if (st->disposition & AV_DISPOSITION_COMMENT)
		av_log(NULL, AV_LOG_INFO, " (comment)");
	if (st->disposition & AV_DISPOSITION_LYRICS)
		av_log(NULL, AV_LOG_INFO, " (lyrics)");
	if (st->disposition & AV_DISPOSITION_KARAOKE)
		av_log(NULL, AV_LOG_INFO, " (karaoke)");
	if (st->disposition & AV_DISPOSITION_FORCED)
		av_log(NULL, AV_LOG_INFO, " (forced)");
	if (st->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
		av_log(NULL, AV_LOG_INFO, " (hearing impaired)");
	if (st->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
		av_log(NULL, AV_LOG_INFO, " (visual impaired)");
	if (st->disposition & AV_DISPOSITION_CLEAN_EFFECTS)
		av_log(NULL, AV_LOG_INFO, " (clean effects)");
	if (st->disposition & AV_DISPOSITION_ATTACHED_PIC)
		av_log(NULL, AV_LOG_INFO, " (attached pic)");
	if (st->disposition & AV_DISPOSITION_TIMED_THUMBNAILS)
		av_log(NULL, AV_LOG_INFO, " (timed thumbnails)");
	if (st->disposition & AV_DISPOSITION_CAPTIONS)
		av_log(NULL, AV_LOG_INFO, " (captions)");
	if (st->disposition & AV_DISPOSITION_DESCRIPTIONS)
		av_log(NULL, AV_LOG_INFO, " (descriptions)");
	if (st->disposition & AV_DISPOSITION_METADATA)
		av_log(NULL, AV_LOG_INFO, " (metadata)");
	if (st->disposition & AV_DISPOSITION_DEPENDENT)
		av_log(NULL, AV_LOG_INFO, " (dependent)");
	if (st->disposition & AV_DISPOSITION_STILL_IMAGE)
		av_log(NULL, AV_LOG_INFO, " (still image)");
	av_log(NULL, AV_LOG_INFO, "\n");

	//dump_metadata(NULL, st->metadata, "    ");

	//dump_sidedata(NULL, st, "    ");
}


int record_pcm(const char* indevice, const char* device_name, const char* pcm_file)
{
	avdevice_register_all(); // 这一句必须有，否则下面返回的 ifmt 是 NULL
	AVInputFormat* ifmt = av_find_input_format(indevice);
	if (!ifmt) {
		fprintf(stderr, "Failed to find input format for input device '%s'\n", indevice);
		return -1;
	}
	AVFormatContext* ic = NULL;
	char msg[1024];
	int ret = avformat_open_input(&ic, device_name, ifmt, NULL);
	if (ret < 0) {
		av_strerror(ret, msg, sizeof(msg));
		fprintf(stderr, "Failed to open audio device [%s]:%s\n", device_name, msg);
		return ret;
	}

	//av_dump_format(ic, 0, device_name, 0);
	dump_stream_format(ic, 0, 0, 0);

	AVCodecContext* cctx = nullptr;
	FILE* fp = nullptr;
	AVPacket pkt;

	av_init_packet(&pkt);
	pkt.data = nullptr; pkt.size = 0;

	do {
		if (!(fp = fopen(pcm_file, "wb"))) {
			ret = errno;
			fprintf(stderr, "Failed to open file '%s':[%d]%s\n", pcm_file, ret, strerror(ret));
			break;
		};

		assert(ic->nb_streams == 1);
		AVSampleFormat sample_format = (AVSampleFormat)ic->streams[0]->codecpar->format;
		int channel_count = ic->streams[0]->codecpar->channels;
		int sample_rate = ic->streams[0]->codecpar->sample_rate;
		const char* fmt = nullptr;
		//get_format_from_input_fmt(&fmt, sample_format);		

		printf("Press Q to stop record\n");
		bool running = true;
		std::thread t([&ic, &pkt, &running, &fp]() {
			while ((av_read_frame(ic, &pkt) >= 0) && running) {
				printf(".");
				fwrite(pkt.data, 1, pkt.size, fp);
				av_packet_unref(&pkt);
			}
		});

		while (1) {
			int c = getc(stdin);
			if (c == 'Q' || c == 'q') {
				running = false;
				break;
			} else {
				printf("Press Q to stop record\n");
			}
		}

		running = false;
		t.join();

		if (fmt) {
			printf("Record stopped, play the output audio file with the command:\n"
				   "ffplay -f %s -ac %d -ar %d %s\n",
				   fmt, channel_count, sample_rate, pcm_file);
		}

	} while (0);

	if (cctx) {
		avcodec_free_context(&cctx);
	}

	if (fp) {
		fclose(fp);
		fp = nullptr;
	}

	if (ic) {
		avformat_close_input(&ic);
	}

	return ret;
}

int main(int argc, char** argv)
{
	exe = argv[0];

#if defined(_WIN32) && defined(_DEBUG)
	const char* indevice = "dshow";
	const char* audio_device = "audio=麦克风 (Realtek High Definition Audio)";
	const char* output_file = "output.pcm";
#else
	if (argc < 4) {
		usage(); return 0;
	}
	const char* indevice = argv[1];
	const char* audio_device = argv[2];
	const char* output_file = argv[3];
#endif

	std::string audio_device_name = audio_device;	
#ifdef _WIN32
	audio_device_name = jlib::win32::mbcs_to_utf8(audio_device);
#endif
	return record_pcm(indevice, audio_device_name.data(), output_file);
}