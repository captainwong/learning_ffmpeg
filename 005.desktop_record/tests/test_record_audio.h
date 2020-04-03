#pragma once

#include "../audio_recorder.h"
#include "../encoder.h"
#include <QDebug>

void test_record_audio()
{
	auto outfile = "test_record_audio.mp4";
	
	auto enc = encoder::getInstance();
	auto ret = enc->open(outfile);
	qDebug() << "open" << ret;
	ret = enc->addAudioStream();
	qDebug() << "addAudioStream" << ret;
	ret = enc->writeHeader();
	qDebug() << "writeHeader" << ret;



	int pcms = 1000;

	auto rec = audio_recorder::getInstance(audio_recorder::recorder_type::rec_qt);
	if (!rec->start()) {
		fprintf(stderr, "audio_recorder start failed\n");
		return;
	}

	for (int i = 0; i < pcms; i++) {
		auto pcm = rec->getPCM(true);
		if (pcm.data) {
			auto pkt = enc->encodeAudio(pcm.data);
			delete[] pcm.data;
			ret = enc->writeFrame(pkt);
			if (ret) {
				printf(".");
			} else {
				printf("*");
			}
		}
	}

	ret = enc->writeTrailer();
	qDebug() << "\nwriteTrailer" << ret;
	enc->close();
}
