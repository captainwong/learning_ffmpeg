#include <stdio.h>
#include <thread>
#include <string>
#include <jlib/win32/UnicodeTool.h>
#include <stdint.h>

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

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")

enum class FileType {
	pcm,
	wav,
	aac,
};

struct WavRiff {
	char	 chunkID[4];
	uint32_t chunkSize;
	char	 format[4];
};

struct WavFmt {
	char	 subchunk1ID[4];
	uint32_t subchunk1Size;
	uint16_t audioFormat;
	uint16_t numChannels;
	uint32_t sampleRate;
	uint32_t byteRate;
	uint16_t blockAlign;
	uint16_t bitsPerSample;
};

struct WavData {
	char	 subchunk2ID[4];
	uint32_t subchunk2Size;
	// data
};

FileType fileType = FileType::pcm;

WavRiff wavRiff = { 0 };
WavFmt	wavFmt =  { 0 };
WavData	wavData = { 0 };

const char* exe = nullptr;


void usage()
{
	printf("Usage:%s audio_device output.ext\n"
		   "\taudio_device The dshow audio device name like \"Microphone (3- USB2.0 MIC)\", you can run `ffmpeg -f dshow --list-devices true -i \"dummy\"` to find out\n"
		   "\toutput.ext   The recorded audio file extention, available exts are:\n"
		   "\t\tpcm		raw data, no compress\n"
		   "\t\twav		use wav format, no compress\n", exe);
}

FILE* open_file_or_die(const char* output_file)
{
	if (strlen(output_file) < 4) {
		usage();
		exit(0);
	}

	FILE* fp = nullptr;
	std::string ext = output_file;
	ext = ext.substr(ext.size() - 3, 3);
	if (ext == "pcm") {
		fileType = FileType::pcm;
		fp = fopen(output_file, "wb");
	} else if (ext == "wav") {
		fileType = FileType::wav;
		fp = fopen(output_file, "wb");
	} else if (ext == "aac") {
		fileType = FileType::aac;
	} else {
		fp = nullptr;
	}

	if (!fp) {
		int ret = errno;
		fprintf(stderr, "Failed to open output file [%d]:%s\n", ret, strerror(ret));
		exit(ret);
	}

	switch (fileType) {
	case FileType::pcm:
		break;
	case FileType::wav:
	{
		memcpy(wavRiff.chunkID, "RIFF", 4);
		memcpy(wavRiff.format, "WAVE", 4);
		// will write later
		fseek(fp, sizeof(WavRiff), SEEK_SET);

		memcpy(wavFmt.subchunk1ID, "fmt ", 4);
		wavFmt.subchunk1Size = 16;
		wavFmt.audioFormat = 1; // 1 for pcm
		wavFmt.numChannels = 2;
		wavFmt.sampleRate = 44100;
		wavFmt.bitsPerSample = 16;
		wavFmt.byteRate = wavFmt.sampleRate * wavFmt.numChannels * wavFmt.bitsPerSample / 8;
		wavFmt.blockAlign = wavFmt.numChannels * wavFmt.bitsPerSample / 8;
		fwrite(&wavFmt, 1, sizeof(WavFmt), fp);

		memcpy(wavData.subchunk2ID, "data", 4);
		// will write later
		fseek(fp, sizeof(WavData), SEEK_CUR);

		break;
	}
	case FileType::aac:
		break;
	default:
		break;
	}

	return fp;
}

void write_data(FILE* fp, const uint8_t* data, int len)
{
	switch (fileType) {
	case FileType::pcm:
		fwrite(data, 1, len, fp);
		fflush(fp);
		break;
	case FileType::wav:
		wavData.subchunk2Size += len;
		fwrite(data, 1, len, fp);
		fflush(fp);
		break;
	case FileType::aac:
		break;
	default:
		break;
	}
}

void close_file(FILE** fp)
{
	switch (fileType) {
	case FileType::pcm:
		break;
	case FileType::wav:
	{
		wavRiff.chunkSize = sizeof(WavRiff) + sizeof(WavFmt) + sizeof(WavData) + wavData.subchunk2Size;
		rewind(*fp);
		fwrite(&wavRiff, 1, sizeof(WavRiff), *fp);
		fseek(*fp, sizeof(WavFmt), SEEK_CUR);
		fwrite(&wavData, 1, sizeof(WavData), *fp);
		break;
	}
	case FileType::aac:
		break;
	default:
		break;
	}

	fclose(*fp);
	*fp = nullptr;
}


int main(int argc, char** argv)
{
	exe = argv[0];

#ifdef _DEBUG
	const char* audio_device = "麦克风 (Realtek High Definition Audio)";
	//const char* output_file = "output.pcm";
	const char* output_file = "output.wav";
#else
	if (argc < 3) {
		usage(argv[0]); return 0;
	}
	const char* audio_device = argv[1];
	const char* output_file = argv[2];
#endif
	std::string audio_device_name = "audio=";
	audio_device_name += jlib::win32::mbcs_to_utf8(audio_device);

	avdevice_register_all();

	AVInputFormat* ifmt = av_find_input_format("dshow");
	AVFormatContext* ic = NULL;
	int ret = avformat_open_input(&ic, audio_device_name.data(), ifmt, NULL);
	if (ret < 0) {
		char msg[1024];
		av_strerror(ret, msg, sizeof(msg));
		fprintf(stderr, "Failed to open audio device [%s]:%s\n", audio_device_name.data(), msg);
		return ret;
	}

	FILE* fp = open_file_or_die(output_file);
	printf("Press Q to stop record\n");
	bool running = true;
	std::thread t([&running]() {
		while (running) {
			int c = getchar();
			if (c == 'Q' || c == 'q') {
				running = false;
				break;
			} else {
				printf("Press Q to stop record\n");
			}
		}
	});

	AVPacket pkt;
	while ((ret = av_read_frame(ic, &pkt)) == 0 && running) {
		
		write_data(fp, pkt.data, pkt.size);
		printf(".");
		av_packet_unref(&pkt);
	}

	running = false;
	t.join();
	close_file(&fp);
	avformat_close_input(&ic);
	return ret;
}
