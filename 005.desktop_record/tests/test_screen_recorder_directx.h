#pragma once

#include "../screen_recorder.h"

static void test_screen_recorder_directx()
{
	auto outfile = "test_screen_recorder_directx.bgra";
	FILE* f = fopen(outfile, "wb");
	if (!f) {
		fprintf(stderr, "open '%s' failed\n", outfile);
		return;
	}

	int pcms = 100;

	auto rec = screen_recorder::getInstance(screen_recorder::recorder_type::directX);
	if (!rec->start(25)) {
		fprintf(stderr, "audio_recorder start failed\n");
		fclose(f);
		return;
	}

	for (int i = 0; i < pcms; i++) {
		auto p = rec->getBGRA(true);
		fwrite(p.data, 1, p.len, f);
		printf(".");
	}

	fclose(f);
}
