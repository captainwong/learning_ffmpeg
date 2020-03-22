#pragma once

#include <QtWidgets/QWidget>
#include "ui_QtFFmpegPlayer.h"

class QtFFmpegPlayer : public QWidget
{
	Q_OBJECT

public:
	QtFFmpegPlayer(QWidget *parent = Q_NULLPTR);
	virtual ~QtFFmpegPlayer();

protected:
	void updatePlayPause(bool isPaused);

protected slots:
	void slotPushButtonOpenFile();
	void slotPushButtonPlayPause();

private:
	Ui::QtFFmpegPlayerClass ui;
};
