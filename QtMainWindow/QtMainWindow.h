#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtMainWindow.h"

class QtMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	QtMainWindow(QWidget *parent = Q_NULLPTR);

private:
	Ui::QtMainWindowClass ui;
};
