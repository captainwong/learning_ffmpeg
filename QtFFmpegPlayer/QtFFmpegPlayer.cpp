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
	resizeEvent(nullptr);
	dt.start();
	startTimer(30);
}

QtFFmpegPlayer::~QtFFmpegPlayer()
{
	dt.close();
}

void QtFFmpegPlayer::updatePlayPause(bool isPaused)
{
	ui.pushButtonPlayPause->setText(isPaused ? "Play" : "Pause");
}

void QtFFmpegPlayer::timerEvent(QTimerEvent* e)
{
	if (isSliderPressed_) { return; }
	if (dt.totalMs() > 0) {
		double pos = (double)dt.pts() / (double)dt.totalMs();
		ui.slider->setValue(ui.slider->maximum() * pos);
	}
}

void QtFFmpegPlayer::resizeEvent(QResizeEvent* e)
{
	ui.slider->move(50, height() - 50);
	ui.slider->resize(width() - 100, ui.slider->height());
	ui.pushButtonOpenFile->move(100, height() - 100);
	ui.pushButtonPlayPause->move(ui.pushButtonOpenFile->x() + ui.pushButtonOpenFile->width() + 10, ui.pushButtonOpenFile->y());
	ui.openGLWidget->resize(size());
}

void QtFFmpegPlayer::mouseDoubleClickEvent(QMouseEvent* e)
{
	if (isFullScreen()) {
		showNormal();
	} else {
		showFullScreen();
	}
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

void QtFFmpegPlayer::slotSliderPressed()
{
	isSliderPressed_ = true;
}

void QtFFmpegPlayer::slotSliderReleased()
{
	isSliderPressed_ = false;
	//double pos = (double)e->pos().x() / (double)width();
	//setValue(pos * maximum());
	double pos = (double)ui.slider->value() / (double)ui.slider->maximum();
	dt.seek(pos);
}
