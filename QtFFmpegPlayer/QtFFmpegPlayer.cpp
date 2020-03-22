#pragma execution_character_set("utf-8")
#include "QtFFmpegPlayer.h"
#include <QFileDialog>
#include <QMessageBox>
#include "xdemuxerthread.h"

static xdemuxerthread dt{};

QtFFmpegPlayer::QtFFmpegPlayer(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	dt.start();
}

QtFFmpegPlayer::~QtFFmpegPlayer()
{
	dt.close();
}

void QtFFmpegPlayer::updatePlayPause(bool isPaused)
{
	ui.pushButtonPlayPause->setText(isPaused ? "Play" : "Pause");
}

void QtFFmpegPlayer::slotPushButtonOpenFile()
{
	QString file = QFileDialog::getOpenFileName(this, "Choose Video File");
	//file = "rtmp://192.168.1.168:10086/live/test";
	if (file.isEmpty()) { return; }
	setWindowTitle(file);
	if (!dt.open(file.toLocal8Bit(), ui.openGLWidget)) {
		QMessageBox::critical(this, "Error", "无法打开视频文件");
		return;
	}
	updatePlayPause(dt.isPaused());
}

void QtFFmpegPlayer::slotPushButtonPlayPause()
{
	qDebug() << "slotPushButtonPlayPause";
	bool p = !dt.isPaused();
	dt.pause(p);
	updatePlayPause(p);
	qDebug() << "pause" << p;
}
