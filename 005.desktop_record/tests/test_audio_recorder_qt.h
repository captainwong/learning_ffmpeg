#include "../audio_recorder.h"
#include <stdio.h>

static void test_audio_recorder_qt()
{
	auto outfile = "test_audio_recorder_qt.pcm";
	FILE* f = fopen(outfile, "wb");
	if (!f) {
		fprintf(stderr, "open '%s' failed\n", outfile);
		return;
	}

	int pcms = 1000;

	auto rec = audio_recorder::getInstance(audio_recorder::recorder_type::rec_qt);
	if (!rec->start()) {
		fprintf(stderr, "audio_recorder start failed\n");
		fclose(f);
		return;
	}

	for (int i = 0; i < pcms; i++) {
		auto pcm = rec->getPCM(true);
		fwrite(pcm.data, 1, pcm.len, f);
		printf(".");
	}

	fclose(f);
}
