#pragma once

#include <QSlider>
#include <QMouseEvent>

class XSlider : public QSlider
{
	Q_OBJECT

public:
	XSlider(QWidget *parent = Q_NULLPTR);
	~XSlider();

	virtual void mousePressEvent(QMouseEvent* e) override;
	virtual void mouseReleaseEvent(QMouseEvent* e) override;
};
