#pragma once

#include <QtWidgets/QWidget>
#include "ui_QtFFmpegPlayer.h"
#include <QTimerEvent>
#include <QResizeEvent>
#include <QMouseEvent>

class QtFFmpegPlayer : public QWidget
{
	Q_OBJECT

public:
	QtFFmpegPlayer(QWidget *parent = Q_NULLPTR);
	virtual ~QtFFmpegPlayer();

protected:
	void updatePlayPause(bool isPaused);

	virtual void timerEvent(QTimerEvent* e) override;
	virtual void resizeEvent(QResizeEvent* e) override;
	virtual void mouseDoubleClickEvent(QMouseEvent* e) override;

protected slots:
	void slotPushButtonOpenFile();
	void slotPushButtonPlayPause();
	void slotSliderPressed();
	void slotSliderReleased();

private:
	Ui::QtFFmpegPlayerClass ui;
	bool isSliderPressed_ = false;
};
