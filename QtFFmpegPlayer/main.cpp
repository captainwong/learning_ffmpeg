#include "QtFFmpegPlayer.h"
#include <QtWidgets/QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	QtFFmpegPlayer w;
	//MainWindow w;
	w.show();
	return a.exec();
}
