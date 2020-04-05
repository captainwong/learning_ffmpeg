#include "desktop_record.h"
#include <QtWidgets/QApplication>
#include "tests/test_audio_recorder_qt.h"
#include "tests/test_record_audio.h"
#include "tests/test_screen_recorder_directx.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	//test_audio_recorder_qt();
	//test_record_audio();
	test_screen_recorder_directx();

	desktop_record w;
	w.show();
	return a.exec();
}
