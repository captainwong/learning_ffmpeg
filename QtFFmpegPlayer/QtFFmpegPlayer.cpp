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
	ui.pushButtonPlayPause->setText(isPaused ? "播放" : "暂停");
}

void QtFFmpegPlayer::slotPushButtonOpenFile()
{
	QString file = QFileDialog::getOpenFileName(this, "选择视频文件");
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
	dt.pause(!dt.isPaused());
	updatePlayPause(dt.isPaused());
}
