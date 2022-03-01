#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QGLShaderProgram>
#include <mutex>
#include "IVideoCall.h"

struct AVFrame;

class XVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions, public jlib::qt::xplayer::IVideoCall
{
	Q_OBJECT

public:
	XVideoWidget(QWidget* parent = nullptr);
	virtual ~XVideoWidget();

protected:
	virtual void doInit(int width, int height) override;
	virtual void doRepaint(AVFrame* frame) override;

	virtual void initializeGL() override;
	virtual void paintGL() override;
	virtual void resizeGL(int width, int height) override;

private:
	std::mutex mutex_{};
	QGLShaderProgram program_{};
	GLuint uniforms_[3]{ 0 };
	GLuint textures_[3]{ 0 };
	//! ²ÄÖÊÄÚ´æ¿Õ¼ä
	unsigned char* data_[3]{ nullptr };
	int width_ = 640;
	int height_ = 480;
};
