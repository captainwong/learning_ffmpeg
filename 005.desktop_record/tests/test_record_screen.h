#pragma once

#include "../screen_recorder.h"
#include "../encoder.h"
#include <QDebug>

static void test_record_screen()
{
	auto outfile = "test_record_screen.mp4";

	auto enc = encoder::getInstance();
	auto ret = enc->open(outfile);
	qDebug() << "open" << ret;
	ret = enc->addVideoStream(1920, 1080, encoder::VPixFmt::AV_PIX_FMT_BGRA, 1920, 1080);
	qDebug() << "addVideoStream" << ret;
	ret = enc->writeHeader();
	qDebug() << "writeHeader" << ret;

	int pcms = 200;

	auto rec = screen_recorder::getInstance(screen_recorder::recorder_type::directX);
	if (!rec->start(25, 1920, 1080)) {
		fprintf(stderr, "screen_recorder start failed\n");
		return;
	}

	for (int i = 0; i < pcms; i++) {
		auto p = rec->getBGRA(true);
		if (p.data) {
			auto pkt = enc->encodeVideo(p.data);
			delete[] p.data;
			ret = enc->writeFrame(pkt);
			if (ret) {
				printf(".");
			} else {
				printf("*");
			}
		}
	}

	rec->stop();

	ret = enc->writeTrailer();
	qDebug() << "\nwriteTrailer" << ret;
	enc->close();
}
