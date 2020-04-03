#include "desktop_record.h"
#include <QtWidgets/QApplication>
#include "tests/test_audio_recorder_qt.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	test_audio_recorder_qt();

	desktop_record w;
	w.show();
	return a.exec();
}
