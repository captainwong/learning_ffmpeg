#pragma once

#include <QtWidgets/QWidget>
#include "ui_desktop_record.h"

class desktop_record : public QWidget
{
	Q_OBJECT

public:
	desktop_record(QWidget *parent = Q_NULLPTR);

private:
	Ui::desktop_recordClass ui;
};
