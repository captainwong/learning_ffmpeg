#pragma once

#include <QtWidgets/QWidget>
#include "ui_QtFFmpegPlayer.h"

class QtFFmpegPlayer : public QWidget
{
	Q_OBJECT

public:
	QtFFmpegPlayer(QWidget *parent = Q_NULLPTR);

private:
	Ui::QtFFmpegPlayerClass ui;
};
