#include "XSlider.h"

XSlider::XSlider(QWidget *parent)
	: QSlider(parent)
{

}

XSlider::~XSlider()
{
}

void XSlider::mousePressEvent(QMouseEvent* e)
{
	double pos = (double)e->pos().x() / (double)width();
	setValue(pos * maximum());
	//e->ignore();
	//QSlider::mousePressEvent(e);
	QSlider::sliderReleased();
}

void XSlider::mouseReleaseEvent(QMouseEvent* e)
{
	//double pos = (double)e->pos().x() / (double)width();
	//setValue(pos * maximum());
	//e->accept();

	QSlider::mouseReleaseEvent(e);
}
