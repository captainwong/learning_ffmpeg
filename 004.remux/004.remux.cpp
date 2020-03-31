#pragma warning(disable:4819)
#pragma warning(disable:4996)

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
}

#include <vector>

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")


static void logPacket(const AVFormatContext* c, const AVPacket* pkt, const char* tag)
{
	AVRational* time_base = &c->streams[pkt->stream_index]->time_base;
	char pts[AV_TS_MAX_STRING_SIZE]; char pts_time[AV_TS_MAX_STRING_SIZE];
	char dts[AV_TS_MAX_STRING_SIZE]; char dts_time[AV_TS_MAX_STRING_SIZE];
	char duration[AV_TS_MAX_STRING_SIZE]; char duration_time[AV_TS_MAX_STRING_SIZE];
	printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		   tag,
		   av_ts_make_string(pts, pkt->pts), av_ts_make_time_string(pts_time, pkt->pts, time_base),
		   av_ts_make_string(dts, pkt->dts), av_ts_make_time_string(dts_time, pkt->dts, time_base),
		   av_ts_make_string(duration, pkt->duration), av_ts_make_time_string(duration_time, pkt->duration, time_base),
		   pkt->stream_index);
}


int main(int argc, char** argv)
{
	if (argc < 3) {
		printf("usage: %s input output\n"
			   "API example program to remux a media file with libavformat and libavcodec.\n"
			   "The output format is guessed according to the file extension.\n"
			   "\n", argv[0]);
		return 1;
	}

	const auto infile = argv[1];
	const auto outfile = argv[2];

	// 输入上下文
	AVFormatContext* ic = nullptr;
	// 输出上下文
	AVFormatContext* oc = nullptr;

	int ret = 0;

	do {
		// 1 打开输入上下文
		ret = avformat_open_input(&ic, infile, nullptr, nullptr);
		if (!ic) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "open input file '%s' failed: %s\n", infile, buf);
			break;
		}

		// 2 查找输入的流信息
		ret = avformat_find_stream_info(ic, nullptr);
		if (ret < 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "retrieve input stream information failed: %s\n", buf);
			break;
		}

		av_dump_format(ic, 0, infile, 0);

		// 3 建立输出上下文
		avformat_alloc_output_context2(&oc, nullptr, nullptr, outfile);
		if (!oc) {
			fprintf(stderr, "create output context failed: %s\n", outfile); 
			ret = AVERROR_UNKNOWN;
			break;
		}

		// 4 遍历输入流，建立输出流、设置输出流编码参数，并建立输入流与输出流的对应关系

		// 下标为输入stream index，内容为输出stream index
		std::vector<int> streamMap(ic->nb_streams);
		for (unsigned int i = 0, ostreamIndex = 0; i < ic->nb_streams; i++) {
			AVCodecParameters* inCodecpar = ic->streams[i]->codecpar;
			if (inCodecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
				inCodecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
				inCodecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
				streamMap[i] = -1;
				continue;
			}

			streamMap[i] = ostreamIndex++; 

			// 申请输出流
			AVStream* ostream = avformat_new_stream(oc, nullptr);
			if (!ostream) {
				fprintf(stderr, "Failed allocating output stream\n");
				ret = AVERROR_UNKNOWN;
				break;
			}

			// 拷贝编解码参数
			ret = avcodec_parameters_copy(ostream->codecpar, inCodecpar);
			if (ret < 0) {
				char buf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, buf, sizeof(buf) - 1);
				fprintf(stderr, "avcodec_parameters_copy failed: %s\n", buf);
				break;
			}

			ostream->codecpar->codec_tag = 0;
		}

		if (ret != 0) { break; }

		printf("\n=================================\n");
		av_dump_format(oc, 0, outfile, 1);

		// 5 打开输出文件
		if (!(oc->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&oc->pb, outfile, AVIO_FLAG_WRITE);
			if (ret < 0) {
				char buf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, buf, sizeof(buf) - 1);
				fprintf(stderr, "open output file '%s' failed: %s\n", outfile, buf);
				break;
			}
		}

		// 6 写入文件头
		ret = avformat_write_header(oc, nullptr);
		if (ret < 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, buf, sizeof(buf) - 1);
			fprintf(stderr, "write file header failed: %s\n", buf);
			break;
		}

		// 7 读写流
		AVPacket pkt;
		while (1) {
			ret = av_read_frame(ic, &pkt);
			if (ret < 0) { break; }

			AVStream* istream = ic->streams[pkt.stream_index];
			if (pkt.stream_index >= static_cast<int>(streamMap.size()) || 
				streamMap[pkt.stream_index] < 0) {
				av_packet_unref(&pkt);
				continue;
			}

			logPacket(ic, &pkt, "in ");

			pkt.stream_index = streamMap[pkt.stream_index];
			AVStream* ostream = oc->streams[pkt.stream_index];

			// copy packet
			pkt.pts = av_rescale_q_rnd(pkt.pts, istream->time_base, ostream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.dts = av_rescale_q_rnd(pkt.dts, istream->time_base, ostream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.duration = av_rescale_q_rnd(pkt.duration, istream->time_base, ostream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.pos = -1;
			logPacket(oc, &pkt, "out");

			ret = av_interleaved_write_frame(oc, &pkt);
			if (ret < 0) {
				char buf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, buf, sizeof(buf) - 1);
				fprintf(stderr, "muxing packet failed: %s\n", buf);
				break;
			}
			av_packet_unref(&pkt);
		}

		// 8 写入文件尾部信息
		av_write_trailer(oc);
	} while (0);

	// 9 清理
	avformat_close_input(&ic);
	if (oc && oc->oformat && !(oc->oformat->flags & AVFMT_NOFILE)) {
		avio_closep(&oc->pb);
	}
	avformat_free_context(oc);

	if (ret < 0 && ret != AVERROR_EOF) {
		return ret;
	}

	return 0;
}

